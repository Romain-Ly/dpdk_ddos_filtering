
#include <pcap/pcap.h>
#include <string.h> 
#include <stdint.h> 
//#include <filter.h>
#include <stdio.h>


#include "engine_bpf_pcap.h"
#include "engine_bpf.h"

/*************************************************************/
/**
 * STATIC DEF
 **/

#define BPF_PCAP_MAX_ARGS 1024

struct _pcap_file_header{
  uint8_t magic[4];
  uint16_t version_major;
  uint16_t version_minor;
  int32_t thiszone;
  uint32_t sigfigs;
  uint32_t snaplen;
  uint32_t linktype;
};

struct _pcap_pkthdr{
  struct {
    uint32_t tv_sec;
    uint32_t tv_usec;
  } ts;
  uint32_t caplen;
  uint32_t len;
};

//static struct bpf_prog *fp;

/*******************************************************************/


/* inline int */
/* BPF_pcap_filter(u_char *pkt, uint32_t pkt_length){ */

/*   int pcap_offline_filter(bpf, */
/* 			  (const struct pcap_pkthdr) pkt,  */
/* 			  (const u_char) *pkt) */
/*   return run_filter(fp, pkt, pkt_length); */
/* } */




int 
BPF_pcap_init(char *file_path){
  
  
  /* struct _pcap_file_header hdr; */
  /* struct _pcap_pkthdr pkt; */


  FILE * fp;
  char * line = NULL;
  size_t len = 0;
  ssize_t read;
  pcap_t *handle;
  /* printf("string : %s",file_path); */
  /* fflush(stdout) */;

  fp = fopen(file_path, "r");
  if((read = getline(&line, &len, fp)) == -1){
    fprintf(stderr,"BRIDGE : Something is missing in the configuration file\n");
    return -1;
  }
  if (read > BPF_PCAP_MAX_ARGS){
    fprintf(stderr,"BRIDGE : Something is missing in the configuration file\n");
    return -1;
  }
  
  printf("pcap config = '%s'\n",line);
  /* pca_compile_nopcap = wrapper of pcap_open_dead, pcap_compile, pcap_close */
  /* if(pcap_compile_nopcap(1800, */
  /* 			 DLT_EN10MB, */
  /* 			 bpf, */
  /* 			 line,          /\* filter expression *\/ */
  /* 			 0,             /\* optimisation *\/ */
  /* 			 0              /\* netmask *\/ */
  /* 			 ) != 0){ */
  /*   perror("pcap_compile error"); */
  /*   return -1; */
  /* } */

  /* pcap_compile(pcap_t *p, struct bpf_program *fp, */
  /* 	       const char *str, int optimize, bpf_u_int32 netmask) */

  handle = pcap_open_dead( DLT_EN10MB,1500);
  struct bpf_program bpf;	/* The compiled filter expression */

  if  (pcap_compile(handle,
		    &bpf,
		    line,
		    1,
		    0)!=0){
    printf("ERROR: pcap_compile error");
    }
	 
 
  
  /* struct bpf_insn { */
  /* 	u_short	code; */
  /* 	u_char 	jt; */
  /* 	u_char 	jf; */
  /* 	long	k; */
  /* }; */


  /* struct bpf_insn *insn = bpf.bf_insns; */
  /* printf("bpf bytecode :\n"); */
  // print bytecode
  /* for (i = 0; i < bpf.bf_len; ++insn, ++i) { */
  /*   printf("{ 0x%x, %d, %d, 0x%08x },\n", */
  /* 	   insn->code, insn->jt, insn->jf, insn->k); */
  /* } */
  /* fflush(stdout */

  
  /* struct sock_fprog pcap_bpf = { */
  /*   .len = bpf.bpf_len, */
  /*   .filter = bpf.bpf_insn, */
  /* }; */
  
  
  
  compile_pcap_bpf_program((struct pcap_bpf_program *)& bpf); 

  pcap_close(handle);
/* /\* Bytecode compilation *\/ */
  /* fp = compile_filter((struct sock_fprog_kern*) &bpf); */
  /* if (*(int*)fp < 0){ */
  /*   fprintf(stderr,"Error compiling BPF code\n"); */
  /*   exit(1); */
  /* } */
 
  return 0; 
};



  //TODO finish taht
  
                /* filter_func = (filter_t)bpf_filter; */
                /* prog = bpf.bf_insns; */
