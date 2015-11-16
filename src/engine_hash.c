#include <rte_hash.h>
#include <rte_hash_crc.h>

#include <rte_ip.h>


#include "main.h"
#include "engine_hash.h"
/**
 * Copied and adapted from L3fwd example
 * 
 **/


/***************************** Static definitions *****************************/

/**
 * Hash database constant
 **/
#ifdef RTE_ARCH_X86_64
/* default to 4 million hash entries (approx) */
#define BRIDGE_HASH_ENTRIES              1024*1024*4
#else
/* 32-bit has less address-space for hugepage memory, limit to 1M entries */
#define BRIDGE_HASH_ENTRIES              1024*1024*1
#endif
#define HASH_ENTRY_NUMBER_DEFAULT       4

/*TODO move that in main.h*/
#define NB_SOCKETS 2

/* lookup struct */
typedef struct rte_hash lookup_struct_t;
static lookup_struct_t *ipv4_bridge_lookup_struct[NB_SOCKETS];
static lookup_struct_t *ipv6_bridge_lookup_struct[NB_SOCKETS];


static lookup_struct_t *lookup_struct;


/* use ipv6 */
static int ipv6 = 0; /**< ipv6 is false by default. */


/* if_out after lookup */
static uint8_t ipv4_bridge_out_if[BRIDGE_HASH_ENTRIES] __rte_cache_aligned;
static uint8_t ipv6_bridge_out_if[BRIDGE_HASH_ENTRIES] __rte_cache_aligned;


/* Ipv6 length in word of 8 bits */
#define IPV6_ADDR_LEN 16

/**
 * Automatic population of hash entries 
 **/
/* number of hash entry for automatic population */
static uint32_t hash_entry_number = HASH_ENTRY_NUMBER_DEFAULT;



#define ALL_32_BITS 0xffffffff
#define BIT_8_TO_15 0x0000ff00
#define BIT_16_TO_23 0x00ff0000

/**
 * SSE
 * Streaming SIMD Extensions 
 * 8 128 bits registers xmm0->xmm7
 * for 32bit machine, we can store and process in parallel 4 int
 * ex: sqrt of 4 float in single operation
 *  float a[]__attribute__ ((aligned (16)))={41892., 81.5091, 3.14,42.666};
 * __m128 *ptr = (__m128*)a;
 * __m128_t = _mm_sqrt_ps(*ptr)
 */
static __m128i mask0;
static __m128i mask1;
static __m128i mask2;


#define NUMBER_PORT_USED 1
#define BYTE_VALUE_MAX 256
#define PORT_VALUE_MAX 65536



/**
 * key entry 
 **/
struct ipv4_5tuple {
  uint32_t ip_dst;
  uint32_t ip_src;
  uint16_t port_dst;
  uint16_t port_src;
  uint8_t  proto;
} __attribute__((__packed__));

union ipv4_5tuple_host {
  struct {
    uint8_t  pad0;
    uint8_t  proto;
    uint16_t pad1;
    uint32_t ip_src;
    uint32_t ip_dst;
    uint16_t port_src;
    uint16_t port_dst;
  };
  __m128i xmm;
};

#define XMM_NUM_IN_IPV6_5TUPLE 3

struct ipv6_5tuple {
  uint8_t  ip_dst[IPV6_ADDR_LEN];
  uint8_t  ip_src[IPV6_ADDR_LEN];
  uint16_t port_dst;
  uint16_t port_src;
  uint8_t  proto;
} __attribute__((__packed__));

union ipv6_5tuple_host {
  struct {
    uint16_t pad0;
    uint8_t  proto;
    uint8_t  pad1;
    uint8_t  ip_src[IPV6_ADDR_LEN];
    uint8_t  ip_dst[IPV6_ADDR_LEN];
    uint16_t port_src;
    uint16_t port_dst;
    uint64_t reserve;
  };
  __m128i xmm[XMM_NUM_IN_IPV6_5TUPLE];
};


struct ipv4_bridge_route {
  struct ipv4_5tuple key;
  uint8_t if_out;
};

struct ipv6_bridge_route {
  struct ipv6_5tuple key;
  uint8_t if_out;
};

static struct ipv4_bridge_route ipv4_bridge_route_array[] = {
  /*  src                  dst       port src,dst    proto   if_out*/
  {{IPv4(59,1,1,18), IPv4(19,168,2,1),  53, 54750, IPPROTO_UDP}, 0},
  {{IPv4(201,0,0,0), IPv4(200,20,0,1),  102, 12, IPPROTO_TCP}, 0},
  {{IPv4(111,0,0,0), IPv4(100,30,0,1),  101, 11, IPPROTO_TCP}, 1},
  {{IPv4(211,0,0,0), IPv4(200,40,0,1),  102, 12, IPPROTO_TCP}, 1},
};



static struct ipv6_bridge_route ipv6_bridge_route_array[] = {
  {{
      {0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0x02, 0x1e, 0x67, 0xff, 0xfe, 0, 0, 0},
      {0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0x02, 0x1b, 0x21, 0xff, 0xfe, 0x91, 0x38, 0x05},
      101, 11, IPPROTO_TCP}, 0},
  
  {{
      {0xfe, 0x90, 0, 0, 0, 0, 0, 0, 0x02, 0x1e, 0x67, 0xff, 0xfe, 0, 0, 0},
      {0xfe, 0x90, 0, 0, 0, 0, 0, 0, 0x02, 0x1b, 0x21, 0xff, 0xfe, 0x91, 0x38, 0x05},
      102, 12, IPPROTO_TCP}, 1},
  
  {{
      {0xfe, 0xa0, 0, 0, 0, 0, 0, 0, 0x02, 0x1e, 0x67, 0xff, 0xfe, 0, 0, 0},
      {0xfe, 0xa0, 0, 0, 0, 0, 0, 0, 0x02, 0x1b, 0x21, 0xff, 0xfe, 0x91, 0x38, 0x05},
      101, 11, IPPROTO_TCP}, 2},
  
  {{
      {0xfe, 0xb0, 0, 0, 0, 0, 0, 0, 0x02, 0x1e, 0x67, 0xff, 0xfe, 0, 0, 0},
      {0xfe, 0xb0, 0, 0, 0, 0, 0, 0, 0x02, 0x1b, 0x21, 0xff, 0xfe, 0x91, 0x38, 0x05},
      102, 12, IPPROTO_TCP}, 3},
};


/******************** hash_function ********************/
/**
 * rte_hash_crc_4byte  : single crc32 instruction on 4 byte value
***/
static inline uint32_t
ipv4_hash_crc(const void *data, __rte_unused uint32_t data_len,
	      uint32_t init_val){
  const union ipv4_5tuple_host *k;
  uint32_t t;
  const uint32_t *p;
  
  k = data;
  t = k->proto;
  p = (const uint32_t *)&k->port_src;
  
#ifdef RTE_MACHINE_CPUFLAG_SSE4_2
  init_val = rte_hash_crc_4byte(t, init_val);         /* proto */
  init_val = rte_hash_crc_4byte(k->ip_src, init_val); /* ip_src */
  init_val = rte_hash_crc_4byte(k->ip_dst, init_val); /* ip_dst */
  init_val = rte_hash_crc_4byte(*p, init_val);        /* port_src */
#else /* RTE_MACHINE_CPUFLAG_SSE4_2 */
  init_val = rte_jhash_1word(t, init_val);
  init_val = rte_jhash_1word(k->ip_src, init_val);
  init_val = rte_jhash_1word(k->ip_dst, init_val);
  init_val = rte_jhash_1word(*p, init_val);
#endif /* RTE_MACHINE_CPUFLAG_SSE4_2 */
  return (init_val);
}


/**
 * ipv6 version
 ***/
static inline uint32_t
ipv6_hash_crc(const void *data, __rte_unused uint32_t data_len, uint32_t init_val)
{
  const union ipv6_5tuple_host *k;
  uint32_t t;
  const uint32_t *p;
#ifdef RTE_MACHINE_CPUFLAG_SSE4_2
  const uint32_t  *ip_src0, *ip_src1, *ip_src2, *ip_src3;
  const uint32_t  *ip_dst0, *ip_dst1, *ip_dst2, *ip_dst3;
#endif /* RTE_MACHINE_CPUFLAG_SSE4_2 */
  
  k = data;
  t = k->proto;
  p = (const uint32_t *)&k->port_src;
  
#ifdef RTE_MACHINE_CPUFLAG_SSE4_2
  ip_src0 = (const uint32_t *) k->ip_src;
  ip_src1 = (const uint32_t *)(k->ip_src+4);
  ip_src2 = (const uint32_t *)(k->ip_src+8);
  ip_src3 = (const uint32_t *)(k->ip_src+12);
  ip_dst0 = (const uint32_t *) k->ip_dst;
  ip_dst1 = (const uint32_t *)(k->ip_dst+4);
  ip_dst2 = (const uint32_t *)(k->ip_dst+8);
  ip_dst3 = (const uint32_t *)(k->ip_dst+12);
  init_val = rte_hash_crc_4byte(t, init_val);
  init_val = rte_hash_crc_4byte(*ip_src0, init_val);
  init_val = rte_hash_crc_4byte(*ip_src1, init_val);
  init_val = rte_hash_crc_4byte(*ip_src2, init_val);
  init_val = rte_hash_crc_4byte(*ip_src3, init_val);
  init_val = rte_hash_crc_4byte(*ip_dst0, init_val);
  init_val = rte_hash_crc_4byte(*ip_dst1, init_val);
  init_val = rte_hash_crc_4byte(*ip_dst2, init_val);
  init_val = rte_hash_crc_4byte(*ip_dst3, init_val);
  init_val = rte_hash_crc_4byte(*p, init_val);
#else /* RTE_MACHINE_CPUFLAG_SSE4_2 */
  init_val = rte_jhash_1word(t, init_val);
  init_val = rte_jhash(k->ip_src, sizeof(uint8_t) * IPV6_ADDR_LEN, init_val);
  init_val = rte_jhash(k->ip_dst, sizeof(uint8_t) * IPV6_ADDR_LEN, init_val);
  init_val = rte_jhash_1word(*p, init_val);
#endif /* RTE_MACHINE_CPUFLAG_SSE4_2 */

return (init_val);
}


/********************  Automatic population of hash tables ********************/
/**
 * convert to Big Endian
 **/
static void convert_ipv4_5tuple(struct ipv4_5tuple* key1,
				union ipv4_5tuple_host* key2){

  key2->ip_dst = rte_cpu_to_be_32(key1->ip_dst);
  key2->ip_src = rte_cpu_to_be_32(key1->ip_src);
  key2->port_dst = rte_cpu_to_be_16(key1->port_dst);
  key2->port_src = rte_cpu_to_be_16(key1->port_src);
  key2->proto = key1->proto;
  key2->pad0 = 0;
  key2->pad1 = 0;
  return;

}


static void convert_ipv6_5tuple(struct ipv6_5tuple* key1,
                union ipv6_5tuple_host* key2){

  uint32_t i;
  for (i = 0; i < 16; i++)
    {
      key2->ip_dst[i] = key1->ip_dst[i];
      key2->ip_src[i] = key1->ip_src[i];
    }
  key2->port_dst = rte_cpu_to_be_16(key1->port_dst);
  key2->port_src = rte_cpu_to_be_16(key1->port_src);
  key2->proto = key1->proto;
  key2->pad0 = 0;
  key2->pad1 = 0;
  key2->reserve = 0;
  return;
}


/**
 * populate few :
 *    add 4 keys, hardcoded in ipv4_bridge_route_array
 **/

static inline void
populate_ipv4_few_flow_into_table(const struct rte_hash* h){

  uint32_t i;
  int32_t ret;
  uint32_t array_len = sizeof(ipv4_bridge_route_array)/
    sizeof(ipv4_bridge_route_array[0]);
  
  mask0 = _mm_set_epi32(ALL_32_BITS, ALL_32_BITS, ALL_32_BITS, BIT_8_TO_15);

  for (i = 0; i < array_len; i++) {
    struct ipv4_bridge_route  entry;
    union ipv4_5tuple_host newkey;
    entry = ipv4_bridge_route_array[i];
    convert_ipv4_5tuple(&entry.key, &newkey);
    ret = rte_hash_add_key (h,(void *) &newkey);
    if (ret < 0) {
      rte_exit(EXIT_FAILURE, "Unable to add entry %" PRIu32
	       " to the bridge hash.\n", i);
    }
    ipv4_bridge_out_if[ret] = entry.if_out;
  }
  printf("Hash: Adding 0x%" PRIx32 " keys\n", array_len);
}

static inline void
populate_ipv6_few_flow_into_table(const struct rte_hash* h){

  uint32_t i;
  int32_t ret;
  uint32_t array_len = sizeof(ipv6_bridge_route_array)/
    sizeof(ipv6_bridge_route_array[0]);
  
  mask1 = _mm_set_epi32(ALL_32_BITS, ALL_32_BITS, ALL_32_BITS, BIT_16_TO_23);
  mask2 = _mm_set_epi32(0, 0, ALL_32_BITS, ALL_32_BITS);
  for (i = 0; i < array_len; i++) {
    struct ipv6_bridge_route entry;
    union ipv6_5tuple_host newkey;
    entry = ipv6_bridge_route_array[i];
    convert_ipv6_5tuple(&entry.key, &newkey);
    ret = rte_hash_add_key (h, (void *) &newkey);
    if (ret < 0) {
      rte_exit(EXIT_FAILURE, "Unable to add entry %" PRIu32
	       " to the bridge hash.\n", i);
    }
    ipv6_bridge_out_if[ret] = entry.if_out;
  }
  printf("Hash: Adding 0x%" PRIx32 "keys\n", array_len);
}




static inline void
populate_ipv4_many_flow_into_table(const struct rte_hash* h,
				   unsigned int nr_flow){

  /** SSE (SIMD) instructions 
   * set 4 sign,ed 32-bits integer values 
   */
  mask0 = _mm_set_epi32(ALL_32_BITS, ALL_32_BITS, ALL_32_BITS, BIT_8_TO_15);



    /* BYTE_VALUE_MAX = 256 */
    /* PORT_USED = 4 */
    /* uint8_t a = (uint8_t) ((i/NUMBER_PORT_USED)%BYTE_VALUE_MAX); */
    /* uint8_t b = (uint8_t) (((i/NUMBER_PORT_USED)/BYTE_VALUE_MAX)%BYTE_VALUE_MAX); */
    /* uint8_t c = (uint8_t) ((i/NUMBER_PORT_USED)/(BYTE_VALUE_MAX*BYTE_VALUE_MAX)); */
   
    /* Create the ipv4 exact match flow */
    /* memset(&entry, 0, sizeof(entry)); */
    /* switch (i & (NUMBER_PORT_USED -1)) { */
    /* case 0: */
    /*   entry = ipv4_bridge_route_array[0]; */
    /*   entry.key.ip_dst = IPv4(192,c,b,a); */
    /*   break; */
    /* case 1: */
    /*   entry = ipv4_bridge_route_array[1]; */
    /*   entry.key.ip_dst = IPv4(201,c,b,a); */
    /*   break; */
    /* case 2: */
    /*   entry = ipv4_bridge_route_array[2]; */
    /*   entry.key.ip_dst = IPv4(111,c,b,a); */
    /*   break; */
    /* case 3: */
    /*   entry = ipv4_bridge_route_array[3]; */
    /*   entry.key.ip_dst = IPv4(211,c,b,a); */
    /*   break; */
    /* }; */
    
  /* for (i = 0; i < nr_flow; i++) { */
  int j =0;
  uint64_t i = 0;
  uint32_t ip_dsts[]={
    IPv4(59,1,1,182),
    IPv4(59,1,1,97),
    IPv4(59,1,1,9),
    IPv4(59,1,1,148),
    IPv4(59,1,1,154),
    IPv4(59,1,1,122),
    IPv4(59,1,1,27),
    IPv4(59,1,1,235),
    IPv4(59,1,1,91),
    IPv4(59,1,1,18)
  };
  int ip_dsts_size = sizeof(ip_dsts)/sizeof(uint32_t);
  
  for (j = 0; j < ip_dsts_size; j++){
  
    struct ipv4_bridge_route entry; 
    union ipv4_5tuple_host newkey; 
    
    for (i=0;i<65536;i++){
      memset(&entry, 0, sizeof(entry));
      entry = ipv4_bridge_route_array[0];
      
      entry.key.ip_dst = IPv4(192,168,2,1);
      entry.key.ip_src = ip_dsts[j];
      entry.key.port_src = 53;
      entry.key.port_dst = i;
    
      /* convert to BE */
      convert_ipv4_5tuple(&entry.key, &newkey);
      
      /* add key to hash */
      /* not multi_thread safe */
      int32_t ret = rte_hash_add_key(h,(void *) &newkey);
      if (ret < 0) {
	rte_exit(EXIT_FAILURE, "Unable to add entry");
      }
    
      /* output interface array */
      ipv4_bridge_out_if[ret] = (uint8_t) entry.if_out;
    }
  }//endfor
  printf("Hash: Adding 0x%x keys\n", nr_flow);
}


/**
 * see function above for comments
 **/
static inline void
populate_ipv6_many_flow_into_table(const struct rte_hash* h,
                unsigned int nr_flow){
  
  unsigned i;
  mask1 = _mm_set_epi32(ALL_32_BITS, ALL_32_BITS, ALL_32_BITS, BIT_16_TO_23);
  mask2 = _mm_set_epi32(0, 0, ALL_32_BITS, ALL_32_BITS);
   
  for (i = 0; i < nr_flow; i++) {
    struct ipv6_bridge_route entry;
    union ipv6_5tuple_host newkey;
    uint8_t a = (uint8_t) ((i/NUMBER_PORT_USED)%BYTE_VALUE_MAX);
    uint8_t b = (uint8_t) (((i/NUMBER_PORT_USED)/BYTE_VALUE_MAX)%BYTE_VALUE_MAX);
    uint8_t c = (uint8_t) ((i/NUMBER_PORT_USED)/(BYTE_VALUE_MAX*BYTE_VALUE_MAX));
    /* Create the ipv6 exact match flow */
    memset(&entry, 0, sizeof(entry));
    switch (i & (NUMBER_PORT_USED - 1)) {
    case 0: entry = ipv6_bridge_route_array[0]; break;
    case 1: entry = ipv6_bridge_route_array[1]; break;
    case 2: entry = ipv6_bridge_route_array[2]; break;
    case 3: entry = ipv6_bridge_route_array[3]; break;
    };
    entry.key.ip_dst[13] = c;
    entry.key.ip_dst[14] = b;
    entry.key.ip_dst[15] = a;
    convert_ipv6_5tuple(&entry.key, &newkey);
    int32_t ret = rte_hash_add_key(h,(void *) &newkey);
    if (ret < 0) {
      rte_exit(EXIT_FAILURE, "Unable to add entry %u\n", i);
    }
    ipv6_bridge_out_if[ret] = (uint8_t) entry.if_out;
    
  }
  printf("Hash: Adding 0x%x keys\n", nr_flow);
}



/********************* filtering functions **********************/
int 
hash_pkt_filter(struct rte_mbuf* m,  int32_t *ret){

  

  union ipv4_5tuple_host key;

  __m128i data;

  data = _mm_loadu_si128((__m128i*)(rte_pktmbuf_mtod(m, unsigned char *) +
				    sizeof(struct ether_hdr) + 
				    offsetof(struct ipv4_hdr, time_to_live)));

  /* Get 5 tuple: dst port, src port, dst IP address, src IP address and protocol */
  key.xmm = _mm_and_si128(data, mask0);
  
  /* Find destination port */
  *ret = rte_hash_lookup(lookup_struct, (const void *)&key);

  return 0;
} 


int 
hash_pkt_filter4(struct rte_mbuf* m[4],  int32_t *ret){
  
  __m128i data[4];
  union ipv4_5tuple_host key[4];
    

  data[0] = _mm_loadu_si128((__m128i*)(rte_pktmbuf_mtod(m[0], unsigned char *) +
				       sizeof(struct ether_hdr) + 
				       offsetof(struct ipv4_hdr, time_to_live)));
  data[1] = _mm_loadu_si128((__m128i*)(rte_pktmbuf_mtod(m[1], unsigned char *) +
				       sizeof(struct ether_hdr) +
				       offsetof(struct ipv4_hdr, time_to_live)));
  data[2] = _mm_loadu_si128((__m128i*)(rte_pktmbuf_mtod(m[2], unsigned char *) +
				       sizeof(struct ether_hdr) + 
				       offsetof(struct ipv4_hdr, time_to_live)));
  data[3] = _mm_loadu_si128((__m128i*)(rte_pktmbuf_mtod(m[3], unsigned char *) +
				       sizeof(struct ether_hdr) +
				       offsetof(struct ipv4_hdr, time_to_live)));
  
  key[0].xmm = _mm_and_si128(data[0], mask0);
  key[1].xmm = _mm_and_si128(data[1], mask0);
  key[2].xmm = _mm_and_si128(data[2], mask0);
  key[3].xmm = _mm_and_si128(data[3], mask0);


  const void *key_array[4] = {&key[0], &key[1], &key[2],&key[3]};
  rte_hash_lookup_multi(*ipv4_bridge_lookup_struct, &key_array[0], 4, ret);


  /**
   * here we do not care of ipv4_l3fwd_out_if (pass or filtered)
   **/
  /* dst_port[0] = (uint8_t) ((ret[0] < 0) ? portid : ipv4_l3fwd_out_if[ret[0]]); */
  /* dst_port[1] = (uint8_t) ((ret[1] < 0) ? portid : ipv4_l3fwd_out_if[ret[1]]); */
  /* dst_port[2] = (uint8_t) ((ret[2] < 0) ? portid : ipv4_l3fwd_out_if[ret[2]]); */
  /* dst_port[3] = (uint8_t) ((ret[3] < 0) ? portid : ipv4_l3fwd_out_if[ret[3]]); */

  return 0;
} 



static inline void 
get_ipv6_5tuple(struct rte_mbuf* m0, __m128i mask0, __m128i mask1,
		union ipv6_5tuple_host * key){
  
  __m128i tmpdata0 = _mm_loadu_si128((__m128i*)(rte_pktmbuf_mtod(m0, unsigned char *)
						+ sizeof(struct ether_hdr)
						+ offsetof(struct ipv6_hdr, payload_len)));
  __m128i tmpdata1 = _mm_loadu_si128((__m128i*)(rte_pktmbuf_mtod(m0, unsigned char *)
						+ sizeof(struct ether_hdr)
						+ offsetof(struct ipv6_hdr, payload_len)
						+  sizeof(__m128i)));
  __m128i tmpdata2 = _mm_loadu_si128((__m128i*)(rte_pktmbuf_mtod(m0, unsigned char *)
						+ sizeof(struct ether_hdr)
						+ offsetof(struct ipv6_hdr, payload_len)
						+ sizeof(__m128i) + sizeof(__m128i)));
  key->xmm[0] = _mm_and_si128(tmpdata0, mask0);
  key->xmm[1] = tmpdata1;
  key->xmm[2] = _mm_and_si128(tmpdata2, mask1);
  return;
}

/**
 * not used (ipv6)
 **/

int 
hash_pkt_filter4_ipv6(struct rte_mbuf* m[4],  int32_t *ret){
  

  union ipv6_5tuple_host key[4];
 
  get_ipv6_5tuple(m[0], mask1, mask2, &key[0]);
  get_ipv6_5tuple(m[1], mask1, mask2, &key[1]);
  get_ipv6_5tuple(m[2], mask1, mask2, &key[2]);
  get_ipv6_5tuple(m[3], mask1, mask2, &key[3]);
  
  const void *key_array[4] = {&key[0], &key[1], &key[2],&key[3]};
  rte_hash_lookup_multi(*ipv6_bridge_lookup_struct, &key_array[0], 4, ret);
  /* dst_port[0] = (uint8_t) ((ret[0] < 0)? portid:ipv6_l3fwd_out_if[ret[0]]); */
  /* dst_port[1] = (uint8_t) ((ret[1] < 0)? portid:ipv6_l3fwd_out_if[ret[1]]); */
  /* dst_port[2] = (uint8_t) ((ret[2] < 0)? portid:ipv6_l3fwd_out_if[ret[2]]); */
  /* dst_port[3] = (uint8_t) ((ret[3] < 0)? portid:ipv6_l3fwd_out_if[ret[3]]); */
  
  return 0;
} 




/* <<<<<<< HEAD */
/* void */
/* init_hash(int socketid){ */
/* ======= */
/************************ Initialization *************************/


/* old = socketid*/
int
init_hash(char *file_path,int proc_id,int socket_id){
  //  printf("socket id = %d \n",socket_id);
  FILE * fp;
  char buff[CONFIG_MAX_ARGS_LEN];
  
  fp = fopen(file_path, "r");
  uint64_t keys_nb;


  if (fp == NULL){
    fprintf(stderr,"error cannot open config file (%s)\n",
	    file_path);
    return -1;
  } 
  

  /* Read the first line of the file  */
  if (fgets(buff,CONFIG_MAX_ARGS_LEN,fp)== NULL){
    fprintf(stderr,"The first line should indicate"
	    "the number of strings\n");
    return -1;
  }

  if (sscanf(buff,"%lu",&keys_nb) == -1){
    fprintf(stderr,"The first line should indicate"
	    "the number of hash keys\n");
    return -1;
  }
  
  printf("Hash entry number : %lu\n",keys_nb);
  hash_entry_number = keys_nb;

  //>>>>>>> hash_optimized
  struct rte_hash_parameters ipv4_bridge_hash_params = {
    .name = NULL,                              /* name */
    .entries = BRIDGE_HASH_ENTRIES,             /* Total nb of hash entries*/
    .reserved =0,
    //    .bucket_entries = 4,                       /* bucket entries */    //removed in dpdk2.1.0
    .key_len = sizeof(union ipv4_5tuple_host), /* length of hash kley */
    .hash_func = ipv4_hash_crc,                /* hash function */
    .hash_func_init_val = 0,                   /*  init value used by hash */
  };
  
  /* struct rte_hash_parameters ipv6_bridge_hash_params = { */
  /*   .name = NULL, */
  /*   .entries = BRIDGE_HASH_ENTRIES, */
  /*   .bucket_entries = 4, */
  /*   .key_len = sizeof(union ipv6_5tuple_host), */
  /*   .hash_func = ipv6_hash_crc, */
  /*   .hash_func_init_val = 0, */
  /* }; */

  char s[64];
  memset(s,0,64);
  /* create ipv4 hash */
  snprintf(s, sizeof(s), "ipv4_bridge_hash_%d", proc_id);
  ipv4_bridge_hash_params.name = s;
  ipv4_bridge_hash_params.socket_id = socket_id;
  ipv4_bridge_lookup_struct[socket_id] = rte_hash_create(&ipv4_bridge_hash_params);

  if (ipv4_bridge_lookup_struct[socket_id] == NULL){
    fprintf(stderr,"ERROR cannot create Ipv4 hash entries\n"
	    "error %i (%s)\n", rte_errno, strerror(rte_errno));
    RTE_LOG(ERR, EAL, "ERROR hash create ipv4 ," 
	    "error %i (%s)\n", rte_errno, strerror(rte_errno)); 
    rte_exit(EXIT_FAILURE, "Unable to create the bridge hash on "
	     "socket %d\n", socket_id);
  }

  if (hash_entry_number != HASH_ENTRY_NUMBER_DEFAULT) {
    /* For testing hash matching with a large number of flows we
     * generate millions of IP 5-tuples with an incremented dst
     * address to initialize the hash table. */

    if (ipv6 == 0) {
      /* populate the ipv4 hash */
      populate_ipv4_many_flow_into_table(ipv4_bridge_lookup_struct[socket_id],
					 hash_entry_number);
    } else {
      /* populate the ipv6 hash */
      populate_ipv6_many_flow_into_table(ipv6_bridge_lookup_struct[socket_id],
					 hash_entry_number);
    }
  } else {
    /* Use data in ipv4/ipv6 bridge lookup table directly to initialize the hash table */
    if (ipv6 == 0) {
      /* populate the ipv4 hash */
      populate_ipv4_few_flow_into_table(ipv4_bridge_lookup_struct[socket_id]);
    } else {
      /* populate the ipv6 hash */
      populate_ipv6_few_flow_into_table(ipv6_bridge_lookup_struct[socket_id]);
    }
  }
  
  struct proc_config_s *proc_config_t;
  proc_config_t = &proc_config[proc_id];
  proc_config_t->ipv4_lookup_struct = ipv4_bridge_lookup_struct[socket_id];
  proc_config_t->ipv6_lookup_struct = ipv6_bridge_lookup_struct[socket_id];

  lookup_struct = ipv4_bridge_lookup_struct[socket_id];
  return 0;
}


/* <<<<<<< HEAD */
/* inline void */
/* simple_ipv4_bridge_4pkts(struct rte_mbuf* m[4], uint8_t portid){ */
/* //			 struct lcore_conf *qconf){ */

/*   struct ether_hdr *eth_hdr[4]; */
/*   struct ipv4_hdr *ipv4_hdr[4]; */
/*   void *d_addr_bytes[4]; */
/*   uint8_t dst_port[4]; */
/*   int32_t ret[4]; */
/*   union ipv4_5tuple_host key[4]; */
/*   __m128i data[4]; */
  
/*   eth_hdr[0] = rte_pktmbuf_mtod(m[0], struct ether_hdr *); */
/*   eth_hdr[1] = rte_pktmbuf_mtod(m[1], struct ether_hdr *); */
/*   eth_hdr[2] = rte_pktmbuf_mtod(m[2], struct ether_hdr *); */
/*   eth_hdr[3] = rte_pktmbuf_mtod(m[3], struct ether_hdr *); */
  
/*   /\* Handle IPv4 headers.*\/ */
/*   ipv4_hdr[0] = (struct ipv4_hdr *)(rte_pktmbuf_mtod(m[0], unsigned char *) + */
/* 				    sizeof(struct ether_hdr)); */
/*   ipv4_hdr[1] = (struct ipv4_hdr *)(rte_pktmbuf_mtod(m[1], unsigned char *) + */
/* 				    sizeof(struct ether_hdr)); */
/*   ipv4_hdr[2] = (struct ipv4_hdr *)(rte_pktmbuf_mtod(m[2], unsigned char *) + */
/* 				    sizeof(struct ether_hdr)); */
/*   ipv4_hdr[3] = (struct ipv4_hdr *)(rte_pktmbuf_mtod(m[3], unsigned char *) + */
/* 				    sizeof(struct ether_hdr)); */
  
  
/* #ifdef DO_RFC_1812_CHECKS */
/* /\* Check to make sure the packet is valid (RFC1812) *\/ */
/*   uint8_t valid_mask = MASK_ALL_PKTS; */
/*   if (is_valid_ipv4_pkt(ipv4_hdr[0], m[0]->pkt_len) < 0) { */
/*     rte_pktmbuf_free(m[0]); */
/*     valid_mask &= EXECLUDE_1ST_PKT; */
/*   } */
/*   if (is_valid_ipv4_pkt(ipv4_hdr[1], m[1]->pkt_len) < 0) { */
/*     rte_pktmbuf_free(m[1]); */
/*     valid_mask &= EXECLUDE_2ND_PKT; */
/*   } */
/*   if (is_valid_ipv4_pkt(ipv4_hdr[2], m[2]->pkt_len) < 0) { */
/*     rte_pktmbuf_free(m[2]); */
/*     valid_mask &= EXECLUDE_3RD_PKT; */
/*   } */
/*   if (is_valid_ipv4_pkt(ipv4_hdr[3], m[3]->pkt_len) < 0) { */
/*     rte_pktmbuf_free(m[3]); */
/*     valid_mask &= EXECLUDE_4TH_PKT; */
/*   } */
/*   if (unlikely(valid_mask != MASK_ALL_PKTS)) { */
/*     if (valid_mask == 0){ */
/*       return; */
/*     } else { */
/*       uint8_t i = 0; */
/*       for (i = 0; i < 4; i++) { */
/* 	if ((0x1 << i) & valid_mask) { */
/* 	  l3fwd_simple_forward(m[i], portid, qconf); */
/* 	} */
/*       } */
/*       return; */
/*     } */
/*   } */
/* #endif // End of #ifdef DO_RFC_1812_CHECKS */
  
/*   /\** */
/*    * _mm_load_si128 - load 128-bit value */
/*    * return same value in a variable representing a register */
/*    *\/ */

/*   data[0] = _mm_loadu_si128((__m128i*)(rte_pktmbuf_mtod(m[0], unsigned char *) + */
/* 				       sizeof(struct ether_hdr) +  */
/* 				       offsetof(struct ipv4_hdr, time_to_live))); */
/*   data[1] = _mm_loadu_si128((__m128i*)(rte_pktmbuf_mtod(m[1], unsigned char *) + */
/* 				       sizeof(struct ether_hdr) +  */
/* 				       offsetof(struct ipv4_hdr, time_to_live))); */
/*   data[2] = _mm_loadu_si128((__m128i*)(rte_pktmbuf_mtod(m[2], unsigned char *) + */
/* 				       sizeof(struct ether_hdr) +  */
/* 				       offsetof(struct ipv4_hdr, time_to_live))); */
/*   data[3] = _mm_loadu_si128((__m128i*)(rte_pktmbuf_mtod(m[3], unsigned char *) + */
/* 				       sizeof(struct ether_hdr) +  */
/* 				       offsetof(struct ipv4_hdr, time_to_live))); */

/*   /\** */
/*    * _mm_and_si128 - bitwise of a and b ( 128-bit value) */
/*    *\/ */
/*   key[0].xmm = _mm_and_si128(data[0], mask0); */
/*   key[1].xmm = _mm_and_si128(data[1], mask0); */
/*   key[2].xmm = _mm_and_si128(data[2], mask0); */
/*   key[3].xmm = _mm_and_si128(data[3], mask0); */
  
/*   const void *key_array[4] = {&key[0], &key[1], &key[2],&key[3]}; */
/*   rte_hash_lookup_multi(qconf->ipv4_lookup_struct, &key_array[0], 4, ret); */
/*   dst_port[0] = (uint8_t) ((ret[0] < 0) ? portid : ipv4_l3fwd_out_if[ret[0]]); */
/*   dst_port[1] = (uint8_t) ((ret[1] < 0) ? portid : ipv4_l3fwd_out_if[ret[1]]); */
/*   dst_port[2] = (uint8_t) ((ret[2] < 0) ? portid : ipv4_l3fwd_out_if[ret[2]]); */
/*   dst_port[3] = (uint8_t) ((ret[3] < 0) ? portid : ipv4_l3fwd_out_if[ret[3]]); */
  
/*   if (dst_port[0] >= RTE_MAX_ETHPORTS || (enabled_port_mask & 1 << dst_port[0]) == 0) */
/*     dst_port[0] = portid; */
/*   if (dst_port[1] >= RTE_MAX_ETHPORTS || (enabled_port_mask & 1 << dst_port[1]) == 0) */
/*     dst_port[1] = portid; */
/*   if (dst_port[2] >= RTE_MAX_ETHPORTS || (enabled_port_mask & 1 << dst_port[2]) == 0) */
/*     dst_port[2] = portid; */
/*   if (dst_port[3] >= RTE_MAX_ETHPORTS || (enabled_port_mask & 1 << dst_port[3]) == 0) */
/*                 dst_port[3] = portid; */

/*   /\* 02:00:00:00:00:xx *\/ */
/*   d_addr_bytes[0] = &eth_hdr[0]->d_addr.addr_bytes[0]; */
/*   d_addr_bytes[1] = &eth_hdr[1]->d_addr.addr_bytes[0]; */
/*   d_addr_bytes[2] = &eth_hdr[2]->d_addr.addr_bytes[0]; */
/*   d_addr_bytes[3] = &eth_hdr[3]->d_addr.addr_bytes[0]; */
/*   *((uint64_t *)d_addr_bytes[0]) = 0x000000000002 + ((uint64_t)dst_port[0] << 40); */
/*   *((uint64_t *)d_addr_bytes[1]) = 0x000000000002 + ((uint64_t)dst_port[1] << 40); */
/*   *((uint64_t *)d_addr_bytes[2]) = 0x000000000002 + ((uint64_t)dst_port[2] << 40); */
/*   *((uint64_t *)d_addr_bytes[3]) = 0x000000000002 + ((uint64_t)dst_port[3] << 40); */
  

/*   /\* src addr *\/ */
/*   ether_addr_copy(&ports_eth_addr[dst_port[0]], &eth_hdr[0]->s_addr); */
/*   ether_addr_copy(&ports_eth_addr[dst_port[1]], &eth_hdr[1]->s_addr); */
/*   ether_addr_copy(&ports_eth_addr[dst_port[2]], &eth_hdr[2]->s_addr); */
/*   ether_addr_copy(&ports_eth_addr[dst_port[3]], &eth_hdr[3]->s_addr); */
  
/*   send_single_packet(m[0], (uint8_t)dst_port[0]); */
/*   send_single_packet(m[1], (uint8_t)dst_port[1]); */
/*   send_single_packet(m[2], (uint8_t)dst_port[2]); */
/*   send_single_packet(m[3], (uint8_t)dst_port[3]); */

/* } */


/* ======= */
/* >>>>>>> hash_optimized */
