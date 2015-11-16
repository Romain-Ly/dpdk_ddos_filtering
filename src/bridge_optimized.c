

#include "main.h"
#include "bridge.h"
#include "aton.h"
#include "ipc.h"
#include "flib.h" 
#include "dpdk_mp.h"

#include "engine_hash.h"
#include "bridge_optimized.h"

#include "debug.h"


/**
 * Copied and adapted from L3fwd example
 * 
 **/
/***************************** Static definitions ****************************/
/* mask of enabled ports */
static uint32_t main_enabled_port_mask = 0;

/* nb of ports */
static uint8_t port_nb = 0;

#define MASK_ALL_PKTS    0xf
#define EXECLUDE_1ST_PKT 0xe
#define EXECLUDE_2ND_PKT 0xd
#define EXECLUDE_3RD_PKT 0xb
#define EXECLUDE_4TH_PKT 0x7

/* struct mbuf_table { */
/*   unsigned length; */
/*   struct rte_mbuf *mbufs[MAX_PKT_BURST]; */
/* }; */

/* struct slave_conf_s { */
/*   uint8_t conf_id; */
/*   uint8_t port_rx; */
/*   uint8_t port_tx; /\* Port id for that lcore *\/ */
/*   uint8_t queue_rx; /\* Queue id to receive and forward pkts *\/ */
/*   uint8_t queue_tx; */
/*   struct mbuf_table mbuf_tx; */

/* }__attribute__((packed)) __rte_cache_aligned;  */
static struct slave_conf_s slave_conf;

static int my_core_id;



static int (*filter_engine4)(struct rte_mbuf*[4],int32_t*);
static int (*filter_engine)(struct rte_mbuf*,int32_t*);


/* workaround for undeclared identifier for PKT_RX_IPV4_HDR and PKT_RX_IPV6_HDR
 * should be in rte_mbuf.h
 *
 */
#define PKT_RX_IPV4_HDR      (1ULL << 5)  /**< RX packet with IPv4 header. */
#define PKT_RX_IPV4_HDR_EXT  (1ULL << 6)  /**< RX packet with extended IPv4 header. */
#define PKT_RX_IPV6_HDR      (1ULL << 7)  /**< RX packet with IPv6 header. */
#define PKT_RX_IPV6_HDR_EXT  (1ULL << 8)  /**< RX packet with extended IPv6 header. */


/***************************** Forward func ******************************/


/**
 * Send packets
 **/
static int
bridge_send_burst(unsigned pkts_nb){

  struct rte_mbuf **mbuf_table;
  unsigned return_value;
  
  mbuf_table = (struct rte_mbuf **)slave_conf.mbuf_tx.mbufs;
  
  return_value = rte_eth_tx_burst(slave_conf.port_tx,
			    slave_conf.queue_tx,
			    mbuf_table,
			    (uint16_t) pkts_nb);
  
  port_statistics[slave_conf.conf_id].tx += return_value; 
  if (unlikely(return_value < pkts_nb)) { 
    port_statistics[slave_conf.conf_id].dropped += (pkts_nb - return_value); 
    while (return_value < pkts_nb) { 
      //TOTO
      port_statistics[slave_conf.conf_id].dropped_b += 
      	rte_pktmbuf_data_len(mbuf_table[return_value]);
      rte_pktmbuf_free(mbuf_table[return_value]); 
      return_value++; 
    }//endwhile 
  } 
  
  return 0;
}



/**
 * Add a packet to the array and send it if full
 **/
static int
bridge_add_packet(struct rte_mbuf *mbuf){
  unsigned pkts_nb;
  
  pkts_nb = slave_conf.mbuf_tx.length;

  slave_conf.mbuf_tx.mbufs[pkts_nb] = mbuf;
  pkts_nb++;
  

  /* enough pkts to be sent */
  if (unlikely(pkts_nb == MAX_PKT_BURST)) {
    bridge_send_burst(MAX_PKT_BURST);
    pkts_nb = 0;
  }
  
  slave_conf.mbuf_tx.length = pkts_nb;
  return 0;
}

static inline void
bridge_filter(struct rte_mbuf *m){

  int32_t ret=0;
  
  filter_engine(m,&ret);
  /**
   * if not found in hash table ret = -2 (ENOENT)
   **/
  if (ret>0){
    port_statistics[slave_conf.conf_id].filtered += 1;
    port_statistics[slave_conf.conf_id].filtered_b +=
      rte_pktmbuf_data_len(m);
    rte_pktmbuf_free(m);
    } else {
    bridge_add_packet(m); 
  }
  
  /*  send_single_packet(m, dst_port); */
}


/**
 * not used (ipv6)
 **/

static inline void
ipv6_bridge_filter_4pkts(struct rte_mbuf* m[4]){
  
  int32_t ret[4];
   memset(&ret,0,4*sizeof(int32_t));

  //  hash_pkt_filter4_ipv6(m,ret);
  filter_engine4(m,ret);
  for (int i=0; i <4 ; i++){
    if (ret[i]>0){
      port_statistics[slave_conf.conf_id].filtered += 1;
      port_statistics[slave_conf.conf_id].filtered_b +=
	rte_pktmbuf_data_len(m[i]);
      rte_pktmbuf_free(m[i]);
    } else {
      bridge_add_packet(m[i]); 
    }
  }
}

static inline void
ipv4_bridge_filter_4pkts(struct rte_mbuf* m[4])
//			 struct lcore_conf *qconf)
{
  //  struct ether_hdr *eth_hdr[4];
  //struct ipv4_hdr *ipv4_hdr[4];
  //  void *d_addr_bytes[4];
  //  uint8_t dst_port[4];
  int32_t ret[4];
  
  memset(&ret,0,4*sizeof(int32_t));
  
  /* eth_hdr[0] = rte_pktmbuf_mtod(m[0], struct ether_hdr *); */
  /* eth_hdr[1] = rte_pktmbuf_mtod(m[1], struct ether_hdr *); */
  /* eth_hdr[2] = rte_pktmbuf_mtod(m[2], struct ether_hdr *); */
  /* eth_hdr[3] = rte_pktmbuf_mtod(m[3], struct ether_hdr *); */
  
  /* Handle IPv4 headers.*/
  /* ipv4_hdr[0] = (struct ipv4_hdr *)(rte_pktmbuf_mtod(m[0], unsigned char *) + */
  /* 				    sizeof(struct ether_hdr)); */
  /* ipv4_hdr[1] = (struct ipv4_hdr *)(rte_pktmbuf_mtod(m[1], unsigned char *) + */
  /* 				    sizeof(struct ether_hdr)); */
  /* ipv4_hdr[2] = (struct ipv4_hdr *)(rte_pktmbuf_mtod(m[2], unsigned char *) + */
  /* 				    sizeof(struct ether_hdr)); */
  /* ipv4_hdr[3] = (struct ipv4_hdr *)(rte_pktmbuf_mtod(m[3], unsigned char *) + */
  /* 				    sizeof(struct ether_hdr)); */


#ifdef DO_RFC_1812_CHECKS
  /* Check to make sure the packet is valid (RFC1812) */
  uint8_t valid_mask = MASK_ALL_PKTS;
  if (is_valid_ipv4_pkt(ipv4_hdr[0], m[0]->pkt_len) < 0) {
    rte_pktmbuf_free(m[0]);
    valid_mask &= EXECLUDE_1ST_PKT;
  }
  if (is_valid_ipv4_pkt(ipv4_hdr[1], m[1]->pkt_len) < 0) {
    rte_pktmbuf_free(m[1]);
    valid_mask &= EXECLUDE_2ND_PKT;
  }
  if (is_valid_ipv4_pkt(ipv4_hdr[2], m[2]->pkt_len) < 0) {
    rte_pktmbuf_free(m[2]);
    valid_mask &= EXECLUDE_3RD_PKT;
  }
  if (is_valid_ipv4_pkt(ipv4_hdr[3], m[3]->pkt_len) < 0) {
    rte_pktmbuf_free(m[3]);
    valid_mask &= EXECLUDE_4TH_PKT;
  }
  if (unlikely(valid_mask != MASK_ALL_PKTS)) {
    if (valid_mask == 0){
      return;
    } else {
      uint8_t i = 0;
      for (i = 0; i < 4; i++) {
	if ((0x1 << i) & valid_mask) {
	  l3fwd_simple_forward(m[i], portid, qconf);
	}
      }
      return;
    }
  }

#endif // End of #ifdef DO_RFC_1812_CHECKS
 
  /**
   * the following function sohould do the lookup and the filter
   **/
  //  hash_pkt_filter4(m,ret);
  filter_engine4(m,ret);
  uint8_t i = 0;
  for (i=0; i <4 ; i++){
  /**
   * if not found in hash table ret = -2 (ENOENT)
   **/
    if (ret[i]>0){
      port_statistics[slave_conf.conf_id].filtered += 1;
      port_statistics[slave_conf.conf_id].filtered_b +=
	rte_pktmbuf_data_len(m[i]);
      rte_pktmbuf_free(m[i]);
    } else {
      bridge_add_packet(m[i]); 
    }
  /* bridge_add_packet(m[0]); */
  /* bridge_add_packet(m[1]); */
  /* bridge_add_packet(m[2]); */
  /* bridge_add_packet(m[3]); */

  }

  
}


/***************************** Check definitions ******************************/
/**
 * should be useless if router do the rfc check
 * not implemented
 **/
#ifdef DO_RFC_1812_CHECKS
static inline int
is_valid_ipv4_pkt(struct ipv4_hdr *pkt, uint32_t link_len){
  /* From http://www.rfc-editor.org/rfc/rfc1812.txt section 5.2.2 */
  /*
   * 1. The packet length reported by the Link Layer must be large
   * enough to hold the minimum length legal IP datagram (20 bytes).
   */
  if (link_len < sizeof(struct ipv4_hdr))
    return -1;
  
  /* 2. The IP checksum must be correct. */
  /* this is checked in H/W */
  
  /*
   * 3. The IP version number must be 4. If the version number is not 4
   * then the packet may be another version of IP, such as IPng or
   * ST-II.
   */
  if (((pkt->version_ihl) >> 4) != 4)
    return -3;
  /*
   * 4. The IP header length field must be large enough to hold the
   * minimum length legal IP datagram (20 bytes = 5 words).
   */
  if ((pkt->version_ihl & 0xf) < 5)
    return -4;
  
  /*
   * 5. The IP total length field must be large enough to hold the IP
   * datagram header, whose length is specified in the IP header length
   * field.
   */
  if (rte_cpu_to_be_16(pkt->total_length) < sizeof(struct ipv4_hdr))
    return -5;
  
  return 0;
}
#endif





/***************************** main loop ********************************/
/**
 * Main loop
 * int engine(u_char * pkt_ptr, uint32_t headeer.len)
 **/
void 
bridge_loop_optimized(int (*engine4)(struct rte_mbuf*[4],int32_t*),
		      int (*engine)(struct rte_mbuf*,int32_t*),
		      struct slave_conf_s *slave_conf_t
		      ){
  struct rte_mbuf *packets_burst[MAX_PKT_BURST];
  // struct rte_mbuf *mbuf;

  uint64_t prev_tsc, diff_tsc, cur_tsc;
  const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) 
    / US_PER_S * BURST_TX_DRAIN_US;//TODO make a comment on that
  prev_tsc = 0;

  unsigned lcore_id;
  uint8_t j;
  unsigned pkts_received;
 
  /* port mask*/
  main_enabled_port_mask = main_information.port_mask;

  /* nb ports */
  port_nb = main_information.port_nb;

  /* core id */
  lcore_id = rte_lcore_id();
  my_core_id = lcore_id;
    
  //  static int (*engine)(char*,uint32_t) =
  filter_engine4 = engine4;
  filter_engine = engine;

  
  slave_conf.conf_id =  slave_conf_t->conf_id;
  slave_conf.port_rx =  slave_conf_t->port_rx;
  slave_conf.port_tx =  slave_conf_t->port_tx ;
  slave_conf.queue_rx =  slave_conf_t->queue_rx;
  slave_conf.queue_tx =  slave_conf_t->queue_tx;

  RTE_LOG(INFO, BRIDGE, "entering main loop on lcore %u\n", lcore_id);
  RTE_LOG(INFO, BRIDGE, "Receiving on port %u, queue %u\n", 
	  slave_conf.port_rx,slave_conf.queue_rx);
  RTE_LOG(INFO, BRIDGE, "Transmitting on port %u, queue %u\n", 
	  slave_conf.port_tx,slave_conf.queue_tx);

  /**
   *
   * LOOP
   *
   * Datapth here
   *
   *
   **/
  while (1){


    struct bridge_msg_s msg;
  
    cur_tsc = rte_rdtsc(); //timestamp counter
   

    /**
     * STOP message
     **/
    if (unlikely(get_cmd(lcore_id, &msg, I_AM_SLAVE) == 0)) {
      send_cmd(lcore_id, &msg, 0);

      /* If get stop command, stop forwarding and exit */
      if (msg.cmd == STOP) {
	return;
      }
    }


    /**
     * TX queue burst (send burst)
     **/
    /* ONLY on Tx/Rx port for each proc*/
    diff_tsc = cur_tsc - prev_tsc;
    if (unlikely(diff_tsc > drain_tsc)) {
      if (slave_conf.mbuf_tx.length != 0){
	bridge_send_burst(slave_conf.mbuf_tx.length);
	slave_conf.mbuf_tx.length = 0;
      }
    }

    
    /**
     * read packets 
     **/
    /* we receive from only one port and one queue */
    pkts_received = rte_eth_rx_burst(slave_conf.port_rx,
				     slave_conf.queue_rx,
				     packets_burst,
				     MAX_PKT_BURST);
    if (unlikely(pkts_received == 0)){
      /* RTE_LOG(INFO, BRIDGE, "Nothing to do \n"); */
      /* sleep(1); */
      continue;
    } else {
      port_statistics[slave_conf.conf_id].rx += pkts_received;

      /**
       * Send pkts_received - pkts_received%4 packets
       * in groups of 4.
       **/
      int32_t n = RTE_ALIGN_FLOOR(pkts_received, 4);
      for (j = 0; j < n ; j+=4) {
	uint32_t ol_flag = packets_burst[j]->ol_flags
	  & packets_burst[j+1]->ol_flags
	  & packets_burst[j+2]->ol_flags
	  & packets_burst[j+3]->ol_flags;

	//TODO put that somewhere else
	/* dont forget forget for 4 pkts */
	port_statistics[slave_conf.conf_id].rx_b 
	  += rte_pktmbuf_data_len(packets_burst[j]);
	port_statistics[slave_conf.conf_id].rx_b 
	  += rte_pktmbuf_data_len(packets_burst[j+1]);
	port_statistics[slave_conf.conf_id].rx_b 
	  += rte_pktmbuf_data_len(packets_burst[j+2]);
	port_statistics[slave_conf.conf_id].rx_b 
	  += rte_pktmbuf_data_len(packets_burst[j+3]);
	/* if all 4 pkts == ipv4 pkt */ 
	if (ol_flag & PKT_RX_IPV4_HDR ) {
	  ipv4_bridge_filter_4pkts(&packets_burst[j]);

	/* if all 4 pkts == ipv6 pkt */ 
	} else if (ol_flag & PKT_RX_IPV6_HDR) {
	  ipv6_bridge_filter_4pkts(&packets_burst[j]);
	  
	/* else treat 4 pkt independently */ 
	} else {
	  bridge_filter(packets_burst[j]);
	  bridge_filter(packets_burst[j+1]);
	  bridge_filter(packets_burst[j+2]);
	  bridge_filter(packets_burst[j+3]);
	}
      }//endfor

      /**
       * send pkts_received%4 last pkts
       **/
      for (; j < pkts_received ; j++) {
	port_statistics[slave_conf.conf_id].rx_b 
	  += rte_pktmbuf_data_len(packets_burst[j]);
	bridge_filter(packets_burst[j]);
      }
    }//end read packets

  }//end LOOP

}
