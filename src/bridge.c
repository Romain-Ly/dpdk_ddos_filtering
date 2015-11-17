/**
 * \file bridge.c
 * \brief filtering system executed by each slave
 * \author R. Ly
 *
 * 
 */


#include "main.h"
#include "bridge.h"
#include "aton.h"
#include "ipc.h"
#include "flib.h"
#include "dpdk_mp.h"
#include "engine_bpf.h"
#include "engine_bpf_pcap.h"
#include "engine_fast_strstr.h"
#include "engine_misc.h"
#include "engine_hash.h"
#include "bridge_optimized.h"

#include "debug.h"


/*************************** Static definitions ******************************/
/* mask of enabled ports */
static uint32_t main_enabled_port_mask = 0;

/* nb of ports */
static uint8_t port_nb = 0;

/* Timer */
//static int64_t timer_period = 10 * TIMER_MILLISECOND * 1000; 
/* default period is 10 seconds */

/* log*/
static FILE *bridge_log_fd;


/******** Design ********/
static const char *border = "==============================";
//static const char *singleborder = "----------------------------";
//static const char *indentation = "    ";
//static const char *warning_border = "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";

/**
 * Engine 
 *
 **/
#define ENGINE_MAX_SIZE 32
#define ENGINE_ERROR -1
#define ENGINE_DEFAULT 0
#define ENGINE_BPF 1
#define ENGINE_BPF_PCAP 2
#define ENGINE_FAST_STRSTR 3
#define ENGINE_MODULO 4
#define ENGINE_HASH 5

typedef struct engine_s{
  char key[ENGINE_MAX_SIZE];
  int val;
} engine_t;

static engine_t engine_lookup[] = {
  {"DEFAULT",ENGINE_DEFAULT},
  {"BPF",ENGINE_BPF},
  {"BPF_PCAP",ENGINE_BPF_PCAP},
  {"FAST_STRSTR",ENGINE_FAST_STRSTR},
  {"MODULO",ENGINE_MODULO},
  {"HASH",ENGINE_HASH}
};

#define ENGINE_NKEYS (sizeof(engine_lookup)/sizeof(engine_t))

static int (*filter_engine)(char*,uint32_t);


/**
 * Datapath struct
 * Packets
 **/
//#define MAX_PKT_BURST 64



/*local conf*/
static struct slave_conf_s slave_conf;


static int my_core_id;

/****************** Engine functions *************************/

static int
string2key(char *key){
  unsigned i;
  for (i=0 ;i < ENGINE_NKEYS; i++){
    engine_t  tmp = *(engine_lookup + i);
    if (strcmp(tmp.key, key) == 0){
      return tmp.val;
    }
  }//endfor
  return ENGINE_ERROR;
}


/**
 * Forward packets
 * may be not optimized by this way ...
 **/
static inline int
l2fwd(__attribute__((unused)) char *pkt,
      __attribute__((unused)) uint32_t pkt_length){
  return 0; 
}


static inline int
l2fwd_init(char __attribute__((unused)) *dummy){
  return 0;
}


/***************************** ACL ********************************/


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
  


/*
 * Packet Filter
 */
static inline int
bridge_filter(struct rte_mbuf * mbuf){
  
  struct ether_hdr *ethernet_header;
  ethernet_header = rte_pktmbuf_mtod(mbuf, struct ether_hdr *);

  if (filter_engine((char*)ethernet_header,rte_pktmbuf_data_len(mbuf))== 0) {
  /* not in filter*/
    return bridge_add_packet(mbuf) ;
  } else { // MATCH !
    /* free buffer*/
    return -1;
  }
  
  return 0;
}





/***************************** main loop ********************************/
/**
 * Main loop
 * int engine(u_char * pkt_ptr, uint32_t header.len)
 **/

static void 
  bridge_main_loop(int (*engine)(char*,uint32_t)){
  struct rte_mbuf *packets_burst[MAX_PKT_BURST];
  struct rte_mbuf *mbuf;

  uint64_t prev_tsc, diff_tsc, cur_tsc;
  
  const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1) 
    / US_PER_S * BURST_TX_DRAIN_US;//TODO make a comment on that
  prev_tsc = 0;
 
 /* uint64_t old_tsc, delta_tsc; */
 /*  const uint64_t test_tsc = drain_tsc /2000; */

  unsigned lcore_id;
  uint8_t j;
  unsigned pkts_received;
 
  filter_engine = engine;
  
  /* port mask*/
  main_enabled_port_mask = main_information.port_mask;

  /* nb ports */
  port_nb = main_information.port_nb;

  /* core id */
  lcore_id = rte_lcore_id();
  my_core_id = lcore_id;
    
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


    //struct bridge_msg_s msg;
  
    cur_tsc = rte_rdtsc(); //timestamp counter
   

    /* /\** */
    /*  * STOP message */
    /*  **\/ */
    /* if (unlikely(get_cmd(lcore_id, &msg, I_AM_SLAVE) == 0)) { */
    /*   send_cmd(lcore_id, &msg, 0); */

    /*   /\* If get stop command, stop forwarding and exit *\/ */
    /*   if (msg.cmd == STOP) { */
    /* 	return; */
    /*   } */
    /* } */


    /*
     * TX queue burst (send burst)
     */

    /* ONLY on Tx/Rx port for each proc*/
    diff_tsc = cur_tsc - prev_tsc;
    if (unlikely(diff_tsc > drain_tsc)) {
      if (slave_conf.mbuf_tx.length != 0){
	bridge_send_burst(slave_conf.mbuf_tx.length);
	slave_conf.mbuf_tx.length = 0;
      }
    }


    /* micro_sleep */
    /* old_tsc = rte_rdtsc(); */
    /* delta_tsc = 0; */
    /* while(delta_tsc <  test_tsc){ */
    /*   delta_tsc = rte_rdtsc() - old_tsc; */
    /* } */

    /*
     * read packets 
     */
    /* we receive from only one port and queue */
    pkts_received = rte_eth_rx_burst(slave_conf.port_rx,
				     slave_conf.queue_rx,
				     packets_burst,
				     MAX_PKT_BURST);
    if (unlikely(pkts_received == 0)){
      continue;
    } else {
      port_statistics[slave_conf.conf_id].rx += pkts_received;

      
      for (j=0 ; j < pkts_received ; j++){
	mbuf = packets_burst[j];
	port_statistics[slave_conf.conf_id].rx_b += rte_pktmbuf_data_len(mbuf);
	rte_prefetch0(rte_pktmbuf_mtod(mbuf,void *));
	if( bridge_filter(mbuf)!=0) {
	  port_statistics[slave_conf.conf_id].filtered += 1;
	  port_statistics[slave_conf.conf_id].filtered_b +=
	    rte_pktmbuf_data_len(mbuf);
	  rte_pktmbuf_free(mbuf);
	} 
      }
    }//endfor (readpackets)
    
  }//end LOOP

}


/**
 * \fn static int bridge_init_engine(__attribute__((unused)) void)
 * \brief init filtering algorithm engine
 *
 * \return 
 * - 0 engine loaded
 * - -1 failed
 */
static int
bridge_init_engine(__attribute__((unused)) void){
  
  struct proc_config_s *proc_config_t;
  proc_config_t = &proc_config[slave_conf.conf_id];
  
  /* My port_configuration for */
  printf("Filter engine = %s\n",proc_config_t->engine);

  slave_conf.port_rx = proc_config_t->port_rx;
  slave_conf.port_tx = proc_config_t->port_tx;
  slave_conf.queue_rx = proc_config_t->queue_rx; 
  slave_conf.queue_tx = proc_config_t->queue_tx;  


  int (*engine)(char*,uint32_t);
  int (*engine1)(struct  rte_mbuf*, int32_t *ret);
  int (*engine4)(struct  rte_mbuf**, int32_t *ret);

  /* optimized version of bridge_loop */
  /* 0 = no */
  /* 1 = yes */
  int bridge_optimized = 0;
  
  switch (string2key(proc_config_t->engine)){
  case ENGINE_DEFAULT:
    printf("/%s%s/",border,border);
    printf("DEFAULT ENGINE (forward all)");
    //init_algo = &l2fwd_init;
    l2fwd_init(proc_config_t->args);
    engine = &l2fwd;
    break;
  case ENGINE_BPF:
    printf("/%s%s/\n",border,border);
    printf("BPF ENGINE\n");
    //    init_algo = &BPF_init;
    BPF_init(proc_config_t->args);
    engine = &BPF_pkt_filter;
    break;
  case ENGINE_BPF_PCAP:
    printf("/%s%s/\n",border,border);
    printf("BPF PCAP COMPILER\n");
    
    BPF_pcap_init(proc_config_t->args);
    engine = &BPF_pkt_filter;
    break;
  case ENGINE_FAST_STRSTR:
    printf("/%s%s/\n",border,border);
    printf("FAST_STRSTR search pattern (Rabin Karp algo)\n");
    //init_algo = &fast_str_init; 
    fast_str_init(proc_config_t->args);
    engine = &fast_strstr_filter_one_hashed;  
    break;
  case ENGINE_HASH:
    printf("/%s%s/\n",border,border);
    printf("5-tuple HASH\n");
    
    init_hash(proc_config_t->args,
	      slave_conf.conf_id,
	      rte_lcore_to_socket_id(rte_lcore_id()));
    engine4 = &hash_pkt_filter4;
    engine1 = &hash_pkt_filter;
    
    bridge_optimized = 1;
    bridge_loop_optimized(engine4,engine1,&slave_conf);
  
    break;
  case ENGINE_MODULO:
    printf("/%s%s/\n",border,border);
    printf("MODULO drop %%%s \n",proc_config_t->args);
    //    init_algo = &modulo_init; 
    modulo_init(proc_config_t->args);
    engine = &modulo_engine;  
    break;
  case ENGINE_ERROR:
    RTE_LOG(ERR,BRIDGE,"ERROR Engine, we don't recognize %s\n",
	    proc_config_t->engine); 
    RTE_LOG(WARNING,BRIDGE,"WARNING %d is leaveing",
	    rte_lcore_id()); 
    return -1;
    break;
  }

  if (bridge_optimized == 0){
    bridge_main_loop(engine);
  } 
  return 0;
}


/***************** log    ****************/

/**
 * \fn static int bridge_log_init(unsigned lcore_id)
 * \brief Fonction de création d'une nouvelle instance d'un objet Str_t.
 *
 * \param sz Chaîne à stocker dans l'objet Str_t, ne peut être NULL.
 * \return Instance nouvelle allouée d'un objet de type Str_t ou NULL.
 */

/**
 * \fn int abridge_launch_one_lcore(__attribute__((unused)) void *dummy)
 * \brief children process main function (entry point)
 * - flcore_id is the new lcore_id of the slave
 * - lcore_id is the old lcore_id of the slave
 *
 * \return return only if slaves killed 
 */
static int
bridge_log_init(unsigned lcore_id){
  char log_file[LOG_MAX_SIZE];

  snprintf(log_file,LOG_MAX_SIZE,LOG_NAME,lcore_id);
      
  bridge_log_fd = fopen(log_file,"w+");

  if (rte_openlog_stream(bridge_log_fd) != 0){ 
    RTE_LOG(ERR,BRIDGE,"ERROR openlog_stream (log)\n"); 
    return -1;
  } 
  
  RTE_LOG(INFO,BRIDGE,"##########################\n"); 
  
  return 0;
}


/************************* Slave bridge  *******************/
/**
 * \fn int bridge_launch_one_lcore(__attribute__((unused)) void *dummy)
 * \brief children process main function (entry point)
 * - flcore_id is the new lcore_id of the slave
 * - lcore_id is the old lcore_id of the slave
 *
 * \return return only if slaves killed 
 */
int
bridge_launch_one_lcore(__attribute__((unused)) void *dummy){
  unsigned lcore_id = rte_lcore_id();

  struct proc_config_s *proc_config_t;
  unsigned flcore_id;
  int retval;

  /* Change it to floating process, also change it's lcore_id */
  clear_cpu_affinity();
  
  /* RTE_PER_LCORE write per_lcore value */
  RTE_PER_LCORE(_lcore_id) = 0;
  
  /* Get a lcore_id */
  /* Find a lcore id not used yet, avoid to use lcore ID 0 */
  int new_id;
  if ((new_id=flib_assign_lcore_id() )< 0 ) {
    printf("flib_assign_lcore_id failed\n");
    return -1;
  }
  
  flcore_id = rte_lcore_id();

  RTE_LOG(INFO,BRIDGE,"SLAVE %d get id %d\n",lcore_id,new_id);
  RTE_LOG(INFO,BRIDGE,"SLAVE %d is in socket %d\n",
	  lcore_id,rte_lcore_to_socket_id(flcore_id));
  
  /* Set mapping id, so master can return it after slave exited */
  mapping_id[lcore_id] = flcore_id;
  printf("Org lcore_id = %u, current lcore_id = %u\n",
	 lcore_id, flcore_id);
  
  bridge_log_init(flcore_id);
  //print_proc_config(2);//debug fonction
  
  remapping_slave_resource(lcore_id, flcore_id);


  /* get configuration */
  RTE_LOG(DEBUG,BRIDGE,"SLAVE %d get cfg\n",flcore_id);

  struct bridge_msg_s msg2send;
  struct bridge_msg_s msg2recv;
  memset(&msg2send,0,sizeof(struct bridge_msg_s));
  msg2send.cmd = CFG;
  RTE_LOG(DEBUG,BRIDGE,"SLAVE %d send request message cfg\n",flcore_id);

  if(slave_request(flcore_id,&msg2send,&msg2recv)!=0){
    fprintf(stderr,"error config");
    RTE_LOG(WARNING,BRIDGE,"WARNING proc %d"
  	    "I HAVE NOTHING TO DO\n"
  	    "sleep 5s and then leave\n"
  	    ,lcore_id);
    sleep(5);
    
  } else {
    
    slave_conf.conf_id = atoi(msg2recv.msg);
    
    port_statistics[slave_conf.conf_id].pid = getpid();

    RTE_LOG(INFO,BRIDGE,"SLAVE %d Entering loop with conf %d\n",
	    flcore_id,slave_conf.conf_id);
    print_proc_config(slave_conf.conf_id);
 
    /*
     *
     * SLAVE LOOP
     *
     */
    retval = bridge_init_engine(); /* loop */

  }
  RTE_LOG(INFO,BRIDGE,"SLAVE %d end\n",flcore_id);


  /* return lcore_id before return */
  if (main_information.float_proc) {
     /* remove lcore_id from used id so we can used it again */
    flib_free_lcore_id(rte_lcore_id());
    mapping_id[lcore_id] = INVALID_MAPPING_ID;
    
    proc_config_t = &proc_config[lcore_id];
    proc_config_t->used = 0;
  }
  
  /* dont recreate a slave */
  if (retval == -1){
    struct lcore_resource_s *lcore_res = &lcore_resource[lcore_id];
    lcore_res->flags &= SLAVE_DONT_RECREATE_FLAG;

  }

  RTE_LOG(INFO,BRIDGE,"SLAVE %d leaving\n",flcore_id);
  return 0;
}

