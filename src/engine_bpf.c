

#include <filter.h>
#include <stdint.h>

#include "engine_bpf.h"


/************************************/
/**
 * STATIC DEF
 **/
static const char border[11] = "**********";
static const char subborder[11] = "   ---\n";



/************************************/

static struct bpf_prog *fp;


/* struct bpf_prog { */
/*   u32                     jited:1,        /\* Is our filter JIT'ed? *\/ */
/*     len:31;         /\* Number of filter blocks *\/ */
/*   struct sock_fprog_kern  *orig_prog;     /\* Original BPF program *\/ */
/*   unsigned int            (*bpf_func)(const struct sk_buff *skb, */
/* 				      const struct bpf_insn *filter); */
/*   union { */
/*     struct sock_filter      insns[0]; */
/*     struct bpf_insn         insnsi[0]; */
/*     /\* struct work_struct   work; *\/ */
/*   }; */
/* }; */



/* static int  */
/* compile_bpf_filter(__attribute__((unused)) void){ */
/*   /\* From : tcpdump -i eth0 port 80 -dd *\/ */
/*   struct sock_filter code[] = { */
/*     { 0x28, 0, 0, 0x0000000c }, */
/*     { 0x15, 0, 8, 0x000086dd }, */
/*     { 0x30, 0, 0, 0x00000014 }, */
/*     { 0x15, 2, 0, 0x00000084 }, */
/*     { 0x15, 1, 0, 0x00000006 }, */
/*     { 0x15, 0, 17, 0x00000011 }, */
/*     { 0x28, 0, 0, 0x00000036 }, */
/*     { 0x15, 14, 0, 0x00000050 }, */
/*     { 0x28, 0, 0, 0x00000038 }, */
/*     { 0x15, 12, 13, 0x00000050 }, */
/*     { 0x15, 0, 12, 0x00000800 }, */
/*     { 0x30, 0, 0, 0x00000017 }, */
/*     { 0x15, 2, 0, 0x00000084 }, */
/*     { 0x15, 1, 0, 0x00000006 }, */
/*     { 0x15, 0, 8, 0x00000011 }, */
/*     { 0x28, 0, 0, 0x00000014 }, */
/*     { 0x45, 6, 0, 0x00001fff }, */
/*     { 0xb1, 0, 0, 0x0000000e }, */
/*     { 0x48, 0, 0, 0x0000000e }, */
/*     { 0x15, 2, 0, 0x00000050 }, */
/*     { 0x48, 0, 0, 0x00000010 }, */
/*     { 0x15, 0, 1, 0x00000050 }, */
/*     { 0x6, 0, 0, 0x00040000 }, */
/*     { 0x6, 0, 0, 0x00000000 }, */
/*   }; */

/*   struct sock_fprog bpf = { */
/*     .len = ARRAY_SIZE(code), */
/*     .filter = code, */
/*   }; */

/*   fp = compile_filter((struct sock_fprog_kern*) &bpf); */

/*   return 0; */
/* } */





static int 
compile_bpf_program(struct sock_fprog *bpf){

 
  
  fp = compile_filter((struct sock_fprog_kern*) bpf);
  if (*(int*)fp < 0){
    fprintf(stderr,"Error compiling BPF code\n");
    exit(1);
  }
  return 0;
}

/**
 * Cast hack.. bpf_pcap to bpf_linux
 **/
int  
compile_pcap_bpf_program(struct pcap_bpf_program *pcap_bpf){

  /* struct pcap_bpf_program { */
  /* 	u_int bf_len; */
  /* 	struct bpf_insn *bf_insns; */
  /* }; */
  
  struct sock_fprog bpf;
  
  bpf.len = pcap_bpf->bf_len;
  bpf.filter =(struct sock_filter*) pcap_bpf->bf_insns;
  /* struct sock_fprog bpf = { */
  /*   .len = ARRAY_SIZE(code), */
  /*   .filter = code, */
  /* }; */

  /* bpf.len= ARRAY_SIZE(code); */
  /* bpf.filter = code; */
  //compile_bpf_program(&bpf);
 
  fp = compile_filter((struct sock_fprog_kern*) &bpf);
  if (*(int*)fp < 0){
    fprintf(stderr,"Error compiling BPF code\n");
    exit(1);
  }

  /* fp = compile_filter((struct sock_fprog_kern*) &bpf); */
  /* if (*(int*)fp < 0){ */
  /*   fprintf(stderr,"Error compiling BPF code\n"); */
  /*   exit(1); */
  /* } */
  return 0;
}



static void
compile_bpf_filter2(__attribute__((unused)) void){
  /*  From : tcpdump -i eth0 port 80 -dd*/
  /* struct sock_filter code[] = { */
  /*   { 0x28, 0, 0, 0x0000000c }, */
  /*   { 0x15, 0, 8, 0x000086dd }, */
  /*   { 0x30, 0, 0, 0x00000014 }, */
  /*   { 0x15, 2, 0, 0x00000084 }, */
  /*   { 0x15, 1, 0, 0x00000006 }, */
  /*   { 0x15, 0, 17, 0x00000011 }, */
  /*   { 0x28, 0, 0, 0x00000036 }, */
  /*   { 0x15, 14, 0, 0x00000050 }, */
  /*   { 0x28, 0, 0, 0x00000038 }, */
  /*   { 0x15, 12, 13, 0x00000050 }, */
  /*   { 0x15, 0, 12, 0x00000800 }, */
  /*   { 0x30, 0, 0, 0x00000017 }, */
  /*   { 0x15, 2, 0, 0x00000084 }, */
  /*   { 0x15, 1, 0, 0x00000006 }, */
  /*   { 0x15, 0, 8, 0x00000011 }, */
  /*   { 0x28, 0, 0, 0x00000014 }, */
  /*   { 0x45, 6, 0, 0x00001fff }, */
  /*   { 0xb1, 0, 0, 0x0000000e }, */
  /*   { 0x48, 0, 0, 0x0000000e }, */
  /*   { 0x15, 2, 0, 0x00000050 }, */
  /*   { 0x48, 0, 0, 0x00000010 }, */
  /*   { 0x15, 0, 1, 0x00000050 }, */
  /*   { 0x6, 0, 0, 0x00040000 }, */
  /*   { 0x6, 0, 0, 0x00000000 }, */
  /* }; */

  /* From : bpfgen_tools (cloudflare) */
  /* ./bpfgen  -s dns -- www.example.com */
  /* struct sock_filter code[] = { */
  /*   { 0xb1,  0,  0, 0x0000000e }, */
  /*   { 0000,  0,  0, 0x00000022 }, */
  /*   { 0x0c,  0,  0, 0000000000 }, */
  /*   { 0x07,  0,  0, 0000000000 }, */
  /*   { 0x40,  0,  0, 0000000000 }, */
  /*   { 0x15,  0,  9, 0x03777777 }, */
  /*   { 0x40,  0,  0, 0x00000004 }, */
  /*   { 0x15,  0,  7, 0x07657861 }, */
  /*   { 0x40,  0,  0, 0x00000008 }, */
  /*   { 0x15,  0,  5, 0x6d706c65 }, */
  /*   { 0x40,  0,  0, 0x0000000c }, */
  /*   { 0x15,  0,  3, 0x03636f6d }, */
  /*   { 0x50,  0,  0, 0x00000010 }, */
  /*   { 0x15,  0,  1, 0000000000 }, */
  /*   { 0x06,  0,  0, 0x00000001 }, */
  /*   { 0x06,  0,  0, 0000000000 }, */
  /* }; */

  // isc.org
  struct sock_filter code[] = {
    { 0xb1,  0,  0, 0x0000000e },
    { 0000,  0,  0, 0x00000022 },
    { 0x0c,  0,  0, 0000000000 },
    { 0x07,  0,  0, 0000000000 },
    { 0x40,  0,  0, 0000000000 },
    { 0x15,  0,  5, 0x03697363 },
    { 0x40,  0,  0, 0x00000004 },
    { 0x15,  0,  3, 0x036f7267 },
    { 0x50,  0,  0, 0x00000008 },
    { 0x15,  0,  1, 0000000000 },
    { 0x06,  0,  0, 0x00000001 },
    { 0x06,  0,  0, 0000000000 },
  };

  /*copy paste from cloudflare blog*/
 /* struct sock_filter code[] = { */
 /*    { 0000,  0,  0, 0x00000014 }, */
 /*    { 0xb1,  0,  0, 0000000000 }, */
 /*    { 0x0c,  0,  0, 0000000000 }, */
 /*    { 0x07,  0,  0, 0000000000 }, */
 /*    { 0x40,  0,  0, 0000000000 }, */
 /*    { 0x15,  0,  7, 0x07657861 }, */
 /*    { 0x40,  0,  0, 0x00000004 }, */
 /*    { 0x15,  0,  5, 0x6d706c65 }, */
 /*    { 0x40,  0,  0, 0x00000008 }, */
 /*    { 0x15,  0,  3, 0x03636f6d }, */
 /*    { 0x50,  0,  0, 0x0000000c }, */
 /*    { 0x15,  0,  1, 0000000000 }, */
 /*    { 0x06,  0,  0, 0x00000001 }, */
 /*    { 0x06,  0,  0, 0000000000 }, */
 /*  }; */


  //  arp
  /* struct sock_filter code[] = { */
  /*   { 0x28,  0,  0, 0x0000000c }, */
  /*   { 0x15,  0,  1, 0x00000806 }, */
  /*   { 0x06,  0,  0, 0xffffffff }, */
  /*   { 0x06,  0,  0, 0000000000 } */
  /* }; */


  /* PCAP compiled BPF*/
  /* struct sock_filter code[] = { */
  /*   { 0x28, 0, 0, 0x0000000c }, */
  /*   { 0x15, 0, 18, 0x000086dd }, */
  /*   { 0x30, 0, 0, 0x00000014 }, */
  /*   { 0x15, 0, 4, 0x00000084 }, */
  /*   { 0x28, 0, 0, 0x00000036 }, */
  /*   { 0x15, 46, 0, 0x00000050 }, */
  /*   { 0x28, 0, 0, 0x00000038 }, */
  /*   { 0x15, 44, 0, 0x00000050 }, */
  /*   { 0x30, 0, 0, 0x00000014 }, */
  /*   { 0x15, 0, 4, 0x00000006 }, */
  /*   { 0x28, 0, 0, 0x00000036 }, */
  /*   { 0x15, 40, 0, 0x00000050 }, */
  /*   { 0x28, 0, 0, 0x00000038 }, */
  /*   { 0x15, 38, 0, 0x00000050 }, */
  /*   { 0x30, 0, 0, 0x00000014 }, */
  /*   { 0x15, 0, 4, 0x00000011 }, */
  /*   { 0x28, 0, 0, 0x00000036 }, */
  /*   { 0x15, 34, 0, 0x00000050 }, */
  /*   { 0x28, 0, 0, 0x00000038 }, */
  /*   { 0x15, 32, 0, 0x00000050 }, */
  /*   { 0x28, 0, 0, 0x0000000c }, */
  /*   { 0x15, 0, 31, 0x00000800 }, */
  /*   { 0x30, 0, 0, 0x00000017 }, */
  /*   { 0x15, 0, 8, 0x00000084 }, */
  /*   { 0x28, 0, 0, 0x00000014 }, */
  /*   { 0x45, 6, 0, 0x00001fff }, */
  /*   { 0xb1, 0, 0, 0x0000000e }, */
  /*   { 0x48, 0, 0, 0x0000000e }, */
  /*   { 0x15, 23, 0, 0x00000050 }, */
  /*   { 0xb1, 0, 0, 0x0000000e }, */
  /*   { 0x48, 0, 0, 0x00000010 }, */
  /*   { 0x15, 20, 0, 0x00000050 }, */
  /*   { 0x30, 0, 0, 0x00000017 }, */
  /*   { 0x15, 0, 8, 0x00000006 }, */
  /*   { 0x28, 0, 0, 0x00000014 }, */
  /*   { 0x45, 6, 0, 0x00001fff }, */
  /*   { 0xb1, 0, 0, 0x0000000e }, */
  /*   { 0x48, 0, 0, 0x0000000e }, */
  /*   { 0x15, 13, 0, 0x00000050 }, */
  /*   { 0xb1, 0, 0, 0x0000000e }, */
  /*   { 0x48, 0, 0, 0x00000010 }, */
  /*   { 0x15, 10, 0, 0x00000050 }, */
  /*   { 0x30, 0, 0, 0x00000017 }, */
  /*   { 0x15, 0, 9, 0x00000011 }, */
  /*   { 0x28, 0, 0, 0x00000014 }, */
  /*   { 0x45, 7, 0, 0x00001fff }, */
  /*   { 0xb1, 0, 0, 0x0000000e }, */
  /*   { 0x48, 0, 0, 0x0000000e }, */
  /*   { 0x15, 3, 0, 0x00000050 }, */
  /*   { 0xb1, 0, 0, 0x0000000e }, */
  /*   { 0x48, 0, 0, 0x00000010 }, */
  /*   { 0x15, 0, 1, 0x00000050 }, */
  /*   { 0x6, 0, 0, 0x00000708 }, */
  /*   { 0x6, 0, 0, 0x00000000 }, */
  /* }; */
  struct sock_fprog bpf;
  /* struct sock_fprog bpf = { */
  /*   .len = ARRAY_SIZE(code), */
  /*   .filter = code, */
  /* }; */

  bpf.len= ARRAY_SIZE(code);
  bpf.filter = code;
    compile_bpf_program(&bpf);
  /* fp = compile_filter((struct sock_fprog_kern*) &bpf); */
  /* if (*(int*)fp < 0){ */
  /*   fprintf(stderr,"Error compiling BPF code\n"); */
  /*   exit(1); */
  /* } */
}



int 
BPF_pkt_filter(char *pkt, uint32_t pkt_length){
  return run_filter(fp,(u_char *) pkt, pkt_length);
}


int 
BPF_init(char __attribute__((unused)) *tmp){
  printf("%s",subborder);
  printf("BPF init\n");
  compile_bpf_filter2();
  
  return 0;
}
