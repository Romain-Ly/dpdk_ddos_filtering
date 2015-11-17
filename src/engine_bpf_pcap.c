
#include <pcap/pcap.h>
#include <string.h> 
#include <stdint.h> 
//#include <filter.h>
#include <stdio.h>


#include "engine_bpf_pcap.h"
#include "engine_bpf.h"

/*************************************************************/
/*
 * STATIC DEF
 */

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


int 
BPF_pcap_init(char *file_path){
  

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


  handle = pcap_open_dead( DLT_EN10MB,1500);
  struct bpf_program bpf;	/* The compiled filter expression */

  if  (pcap_compile(handle,
		    &bpf,
		    line,
		    1,
		    0)!=0){
    printf("ERROR: pcap_compile error");
    }
	 
  
  
  compile_pcap_bpf_program((struct pcap_bpf_program *)& bpf); 

  pcap_close(handle);

 
  return 0; 
};

