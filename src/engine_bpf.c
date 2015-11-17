/**
 * \file engine_bpf.c
 * \brief bpf_engine
 * \author R. Ly
 *
 * Compile pcap to bytecode  
 */


#include <filter.h>
#include <stdint.h>
#include "engine_bpf.h"


/************************************/

static const char border[11] = "**********";
static const char subborder[11] = "   ---\n";

static struct bpf_prog *fp;


/************************************/
/**
 * \fn static int compile_bpf_program(struct sock_fprog *bpf)
 * \brief jit compile bpf bytecode
 *
 * \param *bpf (struct sock_fprog) bytecode
 * \return 
 * - 0 SUCCESS
 * - salve exits if failed
 */
static int 
compile_bpf_program(struct sock_fprog *bpf){
  
  fp = compile_filter((struct sock_fprog_kern*) bpf);
  if (*(int*)fp < 0){
    fprintf(stderr,"Error compiling BPF code\n");
    exit(1);
  }
  return 0;
}

#ifndef _DOXYGEN_
/**
 * \fn int compile_pcap_bpf_program(struct pcap_bpf_program *pcap_bpf)
 * \brief compile pcap expression to bytecode
 * NOT USED HERE
 * \param *pcap_bpf pointer to struct pcap_bpf_program
 * \return 
 * - 0 if success
 * - exit if failed
 */
int  
compile_pcap_bpf_program(struct pcap_bpf_program *pcap_bpf){

  struct sock_fprog bpf;
  
  bpf.len = pcap_bpf->bf_len;
  bpf.filter =(struct sock_filter*) pcap_bpf->bf_insns;

  fp = compile_filter((struct sock_fprog_kern*) &bpf);

  if (*(int*)fp < 0){
    fprintf(stderr,"Error compiling BPF code\n");
    exit(1);
  }

  return 0;
}
#endif

/**
 * \fn static void compile_bpf_filter2(__attribute__((unused)) void)
 * \brief static def of bytecode
 * a configuration file should be used
 * \param bytecode
 * wrap bytecode into a struct sock_fprog bpf
 */
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

  bpf.len= ARRAY_SIZE(code);
  bpf.filter = code;
    compile_bpf_program(&bpf);

}



/**
 * \fn int BPF_pkt_filter(char *pkt, uint32_t pkt_length)
 * \brief run bpf program
 *
 * \param *pkt pkt pointer
 * \param pkt_length length of packet (header + payload)
 * \return 
 * - -1 if match
 * - 0 otherwise
 */
int 
BPF_pkt_filter(char *pkt, uint32_t pkt_length){
  return run_filter(fp,(u_char *) pkt, pkt_length);
}



/**
 * \fn int BPF_init(char __attribute__((unused)) *tmp){
 * \brief bpf entry point (do nothing)
 *
 */
int 
BPF_init(char __attribute__((unused)) *tmp){
  printf("%s",subborder);
  printf("BPF init\n");
  compile_bpf_filter2();
  
  return 0;
}
