/**
 * \file main.c
 * \brief main file
 * \author R. Ly
 *
 * 
 */


#include "main.h"
#include "bridge.h"
#include "getport_capabilities.h"
#include "flib.h"
#include "parser.h"
#include "ipc.h"
#include "fdir.h"
#include "dpdk_mp.h"
#include "debug.h"


/***************************** Static definitions *****************************/
/* Default port */
/* ETHER_MAX_LEN = 1518 */
/* http://dpdk.org/doc/api/rte__ether_8h.html */

/* flow director structure
 * activation of PERFECT MODE
 * allocation of 2K-2 perfect rules max
 * see intel 82599 controller ref p. 284
 */

/**
 * \struct rte_eth_conf_port_default
 * \brief nic configuration structure
 *
 * configuration
 */
static struct rte_eth_conf port_conf_default = {
  .rxmode = { .max_rx_pkt_len = 9000,//ETHER_MAX_LEN  ,/* Only used if jumbo 
	      //   frame enabled */
	      .split_hdr_size = 0,
	      .header_split   = 0, /**< Header Split  */
	      .hw_ip_checksum = 0, /**< IP checksum offload  */
	      .hw_vlan_filter = 0, /**< VLAN filtering */
	      .hw_vlan_strip = 0,
	      .jumbo_frame    = 1, /**< Jumbo Frame Support  */
  },
  .txmode = {
    .mq_mode = ETH_MQ_TX_NONE,
  },
  .rx_adv_conf = {
    .rss_conf = {
      .rss_key = NULL,
      .rss_hf  = 0x00,
    },
  },
  .fdir_conf =  {
    .mode  = RTE_FDIR_MODE_PERFECT,
    .pballoc = RTE_FDIR_PBALLOC_64K,
    .status = RTE_FDIR_NO_REPORT_STATUS, /* don't report status on
  					    mbuf->hash->fdir->id */
    .drop_queue = 127, //DROP
    .mask = {
      .vlan_tci_mask = 0,
      .ipv4_mask = {
	//	.src_ip = 0x00000001, /* LE */
  	.src_ip = 0x00000001,
	.dst_ip = 0x00000000,
      },
      .ipv6_mask = {
  	.src_ip = {0x00000000, 0x00000000, 0x00000000, 0x00000000},
  	.dst_ip = {0x00000000, 0x00000000, 0x00000000, 0x00000000},
      },
      .src_port_mask = 0x0000,
      .dst_port_mask = 0x0000,
    },
  },
};


/******** Design ********/
 static const char *border = "=============================="; 
 static const char *singleborder = "----------------------------"; 
 static const char *indentation = "    "; 
/* //static const char *warning_border = "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"; */

/***************************** Check port link status *************************/
/* Check the link status of all ports in up to 9s, and print them finally
 * wait before link up from dpdk example
 * prevent sending packets before link up
 */

static void
check_all_ports_link_status(uint8_t port_num, uint32_t port_mask){
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */

  uint8_t portid, count, all_ports_up, print_flag = 0;
  struct rte_eth_link link;

  printf("Checking link status\n");
  fflush(stdout);
  for (count = 0; count <= MAX_CHECK_TIME; count++) {
    all_ports_up = 1;

    for (portid = 0; portid < port_num; portid++) {
      if ((port_mask & (1 << portid)) == 0){
	continue;
      }
 
      memset(&link, 0, sizeof(link));
      rte_eth_link_get_nowait(portid, &link);
      /* print link status if flag set */
      if (print_flag == 1) {
	if (link.link_status){
	  printf("Port %d Link Up - speed %u "
		 "Mbps - %s\n", (uint8_t)portid,
		 (unsigned)link.link_speed,
		 (link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
		 ("full-duplex") : ("half-duplex\n"));
      } else {
	  printf("Port %d Link Down\n",
		 (uint8_t)portid);
	}
	continue;
      }
      /* clear all_ports_up flag if any link down */
      if (link.link_status == 0) {
	all_ports_up = 0;
	break;
      }
    }
    /* after finally printing all link status, get out */
    if (print_flag == 1){
      break;
    }

    if (all_ports_up == 0) {
      printf(".");
      fflush(stdout);
      rte_delay_ms(CHECK_INTERVAL);
    }

    /* set the print_flag if all ports up or timeout */
    if (all_ports_up == 1 || count == (MAX_CHECK_TIME - 1)) {
      print_flag = 1;
      printf("done\n");
    }
  }
}



/***************************** Port initilisation *****************************/
/*/**                                                                                                                                                                                                                
 * \struct rte_eth_conf_port_default                                                                                                                                                                               
 * \brief nic configuration structure                                                                                                                                                                              
 *                                                                                                                                                                                                                 
 * configuration                                                                                                                                                                                                   
 */  
 * Initialize a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */

static inline int
port_init(uint8_t port_id, struct rte_mempool *mbuf_pool){
  //	  port_information_t * port_info){
  
  struct rte_eth_conf port_conf = port_conf_default;
  struct port_queue_conf_s *port_qconf;
  struct rte_eth_rxconf * port_rx_qconf;
  struct rte_eth_txconf * port_tx_qconf;  

  uint16_t tx_rings, rx_rings;
  int return_value;
  uint16_t q;

  if (port_id >= rte_eth_dev_count()){
    RTE_LOG(CRIT,PORT,"PORT init error\n");
    return -1;
  }



   /**************** configuration of ethernet device **************/
  /* Configure the Ethernet device. */
  port_qconf = &port_queue_conf[port_id];
  rx_rings = port_qconf->queue_rx_nb;
  tx_rings = port_qconf->queue_tx_nb;

  printf("%s %s\n",indentation,singleborder);
  printf("%s Con/**                                                                                                                                                                                                                
 * \struct rte_eth_conf_port_default                                                                                                                                                                               
 * \brief nic configuration structure                                                                                                                                                                              
 *                                                                                                                                                                                                                 
 * configuration                                                                                                                                                                                                   
 */  figure eth device\n",indentation);
  return_value = rte_eth_dev_configure(port_id, rx_rings, tx_rings, &port_conf);
  if (return_value != 0){
    RTE_LOG(CRIT, PORT, "rte_eth_dev_configure error %i (%s)\n", 
	    rte_errno, strerror(rte_errno));
    //    RTE_LOG(CRIT,PORT,"ERROR rte_eth_dev_configure\n");
    return return_value;
  }


  /****************  Conf RX queues  **************/
  /* rte_eth_dev_socket_id check socket validity, */
  /*  if it fails => 0 (=default) */
  printf("%s %s\n",indentation,singleborder);
  printf("%s Configure Rx queues\n",indentation);
  //AQW
  

 
  
  port_rx_qconf =  &port_qconf->port_conf.default_rxconf;
  for (q = 0; q < rx_rings; q++) {
    return_value = rte_eth_rx_queue_setup(port_id, 
					  q,               /* queue id*/
					  RX_RING_SIZE,    /* nb of descriptors */
					  rte_eth_dev_socket_id(port_id),
					  
					  port_rx_qconf, 
					  mbuf_pool);
    if (return_value < 0){
      RTE_LOG(CRIT,PORT,"Rx queue setup\n");
      return return_value;
    }
  }

  /****************  Conf TX queues  **************/
  printf("%s %s\n",indentation,singleborder);
  printf("%s Configure Tx queues\n",indentation);

  port_tx_qconf =  &port_qconf->port_conf.default_txconf;
  for (q = 0; q < tx_rings; q++) {
    return_value = rte_eth_tx_queue_setup(port_id,
					  q, 
					  TX_RING_SIZE,
					  rte_eth_dev_socket_id(port_id),
					  port_tx_qconf);
    if (return_value < 0){
      RTE_LOG(CRIT,PORT,"Tx queue setup\n");
      return return_value;
    }
  }


  /****************  Conf FDIR  **************/
  if (main_information.fdir == 1){
    printf("%s %s\n",indentation,singleborder);
    printf("%s Configure Fdir\n",indentation);
    
    if (fdir_init(port_id) != 0){
      rte_exit(EXIT_FAILURE, "Cannot init FDIR %"PRIu8 "\n", port_id);
    }
  }
    /**
     * useless
     **/
  /* if (fdir_set_masks(port_id) != 0){ */
  /*   rte_exit(EXIT_FAILURE, "Cannot set masks %"PRIu8 "\n", port_id); */
  /* } */
  
  
  /********************** Start device !! ***************************/
  
  printf("%s %s\n",indentation,singleborder);
  printf("%s Start device\n",indentation);
  return_value = rte_eth_dev_start(port_id);
  if (return_value < 0){
    RTE_LOG(CRIT,PORT,"start error\n");
    return return_value;
  }
   


  
  /* Enable RX in promiscuous mode for the Ethernet device. */
  rte_eth_promiscuous_enable(port_id);

  return 0;
}



/******************************** #stats  ******************************/
/**
 * from dpdk, stats
 */
void print_stats(__attribute__((unused)) void) {
  uint64_t total_packets_dropped, total_packets_tx, 
    total_packets_rx, total_packets_filtered;
  uint64_t total_packets_dropped_b, total_packets_tx_b, 
    total_packets_rx_b, total_packets_filtered_b;
  total_packets_dropped = total_packets_tx= 
   total_packets_rx= total_packets_filtered=0;
  total_packets_dropped_b= total_packets_tx_b= 
    total_packets_rx_b= total_packets_filtered_b=0;
  int conf_id;
  
  int Mbytes = 8*1024*1024;

  const char clr[] = { 27, '[', '2', 'J', '\0' };
  const char topLeft[] = { 27, '[', '1', ';', '1', 'H','\0' };

  /* Clear screen and move to top left */
  printf("%s%s", clr, topLeft); //TODO print that only if smallest core id

  printf("\n============================================================\n");
  
  for (conf_id = 0; conf_id <  main_information.proc_config_nb ; conf_id++) {
 
    printf("\n(pid : %6"PRIu32") conf %3u ------ Packets -------------- MBytes"
	   "\n    Sent: %28"PRIu64"  %20"PRIu64
	   "\n    Received: %24"PRIu64"  %20"PRIu64
	   "\n    Filtered: %24"PRIu64"  %20"PRIu64
	   "\n    Dropped: %25"PRIu64"  %20"PRIu64,
	   port_statistics[conf_id].pid,
	   conf_id,
	   port_statistics[conf_id].tx,
	   (port_statistics[conf_id].rx_b-port_statistics[conf_id].filtered_b-
	    port_statistics[conf_id].dropped_b)/Mbytes,
	   port_statistics[conf_id].rx,
	   port_statistics[conf_id].rx_b/Mbytes,
	   port_statistics[conf_id].filtered,
	   port_statistics[conf_id].filtered_b/Mbytes, 
	   port_statistics[conf_id].dropped,
	   port_statistics[conf_id].dropped_b/Mbytes);
    
    total_packets_dropped += port_statistics[conf_id].dropped;
    total_packets_tx += port_statistics[conf_id].tx;
    total_packets_rx += port_statistics[conf_id].rx;
    total_packets_filtered += port_statistics[conf_id].filtered;

    total_packets_dropped_b += port_statistics[conf_id].dropped_b;
    total_packets_rx_b += port_statistics[conf_id].rx_b;
    total_packets_filtered_b += port_statistics[conf_id].filtered_b;
    total_packets_tx_b = total_packets_rx_b - total_packets_filtered_b
      - total_packets_dropped_b;
    
  }
  printf("\nAggregate statistics ======================================="
	 "\n    Total sent: %22"PRIu64"  %20"PRIu64
	 "\n    Total received: %18"PRIu64"  %20"PRIu64
	 "\n    Total filtered: %18"PRIu64"  %20"PRIu64
	 "\n    Total dropped: %19"PRIu64"  %20"PRIu64,
	 total_packets_tx,total_packets_tx_b/Mbytes,
	 total_packets_rx,total_packets_rx_b/Mbytes,
	 total_packets_filtered,total_packets_filtered_b/Mbytes,
	 total_packets_dropped,total_packets_dropped_b/Mbytes);
  printf("\n============================================================\n");
  
}


static void
print_port_stats(__attribute__((unused)) void) {

  int port_nb = main_information.port_nb;
  uint8_t port_id;
  char buffer_char[100]  ;
  memset(buffer_char,0,100);

  static const char *border = "==============================";
  static const char *singleborder = "----------------------------";
 
  struct rte_eth_stats stats[port_nb];


  printf("\nNIC statistics");
  for (port_id = 0; port_id < port_nb; port_id++){
    if ((main_information.port_mask & (1 << port_id)) == 0){
      continue;
     }
    rte_eth_stats_get(port_id, &stats[port_id]);
    
  }
  printf("\n%s%s%s\n",border,border,border);

  printf("Port |");
                                /* X =  width columns           */
  printf(" Rx-packets|"   );    /* 12  received #packets        */
  printf("  Rx-missed|"   );    /* 12  full FIFO                */
  printf("%10c   Rx-bytes|",0); /* 20 received #bytes           */
  printf(" Tx-packets|"   );    /* 12  TX #packets              */
  printf("  Tx-errors|"   );    /* 12  Tx pkts failed           */
  printf("%10c   Tx-bytes|",0); /* 20 Tx #bytes           */

  printf("\n");
  //TODO max ports == 8 (prevent overflow on buffer_char)
  //WARNING we print with %10 but int is 64bit long
  for  (port_id = 0; port_id < port_nb; port_id++){
    if ((main_information.port_mask & (1 << port_id)) == 0){
      continue;
    }
    printf(" %4d:",port_id);
    printf(" %10"PRIu64 "| %10"PRIu64 "|%20"PRIu64"|",
	   stats[port_id].ipackets,
	   stats[port_id].imissed,
	   stats[port_id].ibytes
	   );
    printf(" %10"PRIu64 "| %10"PRIu64 "|%20"PRIu64"",
	   stats[port_id].opackets,
	   stats[port_id].oerrors,
	   stats[port_id].obytes
	   );

    printf("|\n");
  }

  printf("%s\n\n",singleborder);
  printf("Port |");
  printf("  Rx-badcrc|"   );     /* 12  #packets  bad crc        */
  printf("  Rx-badlen|"   );     /* 12  #pkts bad len            */
  printf("  Rx-errors|"   );     /* 12  #pkts errors             */
  printf("  Rx-nombuf|"   );     /* 12  #pkts errors             */
  printf("\n");

  for  (port_id = 0; port_id < port_nb; port_id++){
    if ((main_information.port_mask & (1 << port_id)) == 0){
      continue;
    }
    printf(" %4d:",port_id);
    printf(" %10"PRIu64 "| %10"PRIu64 "| %10"PRIu64 "| %10"PRIu64"",
	   stats[port_id].ibadcrc,
	   stats[port_id].ibadlen,
	   stats[port_id].ierrors,
	   stats[port_id].rx_nombuf
	   );
    printf("|\n");
    
  }

  /* stats fdir */
  // if (fdir_conf.mode != RTE_FDIR_MODE_NONE) TODO print only if FDIR ENABLED
  printf("%s\n\n",singleborder);
    
  printf("Port |");
  printf("%10c  Fdirmatch|",0   );     /* 20  Rx #packets matching filter   */
  printf("%10c   Fdirmiss|",0  );     /* 20  Rx #pkts not matching filter  */
  printf("\n");

  for  (port_id = 0; port_id < port_nb; port_id++){
    if ((main_information.port_mask & (1 << port_id)) == 0){
      continue;
    }
    printf(" %4d:",port_id);
    printf("%20"PRIu64 "|%20"PRIu64 "",
	   stats[port_id].fdirmatch,
	   stats[port_id].fdirmiss
	   );
    printf("|\n");
  }

        

  printf("\n%s%s%s\n",border,border,border);
}

static void 
write_stats(FILE *fp,uint64_t cumulative_tsc){
  
  int bufsize = BUFSIZE *sizeof(char);
  char *buffer;
  int conf_id;
  int retval;

 
  if (unlikely((buffer=rte_zmalloc("stats buffer",BUFSIZE,0))==0)){
    RTE_LOG(ERR, MALLOC, "  cannot malloc stats buffer, "
	    "error %i (%s)\n", rte_errno, strerror(rte_errno));
    rte_exit(EXIT_FAILURE, "ERROR Main loop\n");
  }

  /**
   * write in csv fashion
   **/
  for (conf_id = 0; conf_id <  main_information.proc_config_nb ; conf_id++) {
    memset(buffer,0,bufsize);
    retval = snprintf(buffer,bufsize,
		      "%lu, "                     /* cumulative_tsc time series */
		      "%s, "                      /* type : app or nic stats */
		      "%d, "                      /* pid of proc */
		      "%u, "                      /* conf id */
		      "%s, "                      /* engine args*/
		      "%lu, %lu, %lu, %lu\n"        /* rx, tx, filtered, dropped*/ 
		      ,cumulative_tsc 
		      ,"app"
		      ,port_statistics[conf_id].pid
		      ,conf_id
		      ,proc_config[conf_id].engine
		      ,port_statistics[conf_id].tx
		      ,port_statistics[conf_id].rx
		      ,port_statistics[conf_id].filtered
		      ,port_statistics[conf_id].dropped
		      );

    if (retval > bufsize) {
      RTE_LOG(ERR, MALLOC, " stats buffer too small, change bufsize"
	      "or do a workaround");
      rte_exit(EXIT_FAILURE, "ERROR write stats in main loop\n");
    }
    if(fwrite(buffer,sizeof(char),retval,fp)!=(unsigned)retval){
      RTE_LOG(ERR, MALLOC, " cannot write stats, "
	      "error %i (%s)\n", rte_errno, strerror(rte_errno));
      rte_exit(EXIT_FAILURE, "ERROR Main loop\n");
    }
  }

 
  rte_free(buffer);
  

}


/************************** main #loop functions *************************/
/**
 * Functions for Master process
 * should be optimized
 */
static int
main_set_lcore_cfg(unsigned lcore_id){
  
  struct bridge_msg_s msg2send;
  struct proc_config_s *proc_cfg; 
  struct lcore_resource_s* lcore_res;
  int retval;

  lcore_res = &lcore_resource[lcore_id]; 
  
  memset(&msg2send,0,sizeof(struct bridge_msg_s));
  msg2send.cmd = CFG;

  RTE_LOG(DEBUG,MASTER," send answer message cfg\n");

  /* find a free proc */
  unsigned proc_id;
  for (proc_id = 0; proc_id < RTE_MAX_LCORE; proc_id++){
    proc_cfg = & proc_config[proc_id];
    if ((proc_cfg->used == 1)|(proc_cfg->enabled ==0)){
      continue;
    } else {
      retval = snprintf(msg2send.msg,10,"%d",proc_id);
      if (retval <=0 && retval >10){	
	RTE_LOG(ERR,MASTER,"Error proc_id size too large : %d",proc_id);
	return -1;
      }
      
      
      if (send_cmd(lcore_id, &msg2send, I_AM_MASTER) != 0){
	return -1;
      } else {
	proc_cfg->used = 1;
	lcore_res->config_id = proc_id;
	main_information.proc_config_used_nb ++;
	
	printf("%s\n",singleborder);
	printf("%s",indentation);
	RTE_LOG(INFO,MASTER,"slave %d use config %d\n",lcore_id,proc_id);
	
	
	return 0;
      }//end send_cmd
    }
  }//endfor
  
  return -1;
}


/**
 * Main_loop
 **/

static void 
main_loop(__attribute__((unused)) void){

  FILE *fp;
  unsigned lcore_id;
  uint64_t prev_tsc, diff_tsc, cur_tsc,timer_tsc, timer_stats_tsc,cumulative_tsc;
  
  //  const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1)
  // / US_PER_S * BURST_TX_DRAIN_US;
	
  RTE_LOG(INFO, MASTER, "entering main loop on main lcore\n");
  
  int init_config = main_information.proc_config_nb - 
    main_information.proc_config_used_nb;
  struct bridge_msg_s msg;
        
  /**
   * statistics
   **/
  if (timer_stats > 0){

    fp = fopen(timer_file, "w");
	      
    if (fp == NULL){
      RTE_LOG(ERR, MASTER, "cannot open file '%s' for stats ",timer_file);
      rte_exit(EXIT_FAILURE, "ERROR main loop\n");
    }
    
  }

  prev_tsc = cur_tsc = rte_rdtsc();
  cumulative_tsc = timer_tsc = 0;
  timer_stats_tsc = 0;
  /**
   * Main Loop
   **/ 
  while (1){
    
    cur_tsc = rte_rdtsc();
    //can overflow after many hours

    diff_tsc = cur_tsc - prev_tsc;
    cumulative_tsc += diff_tsc;     
    /**
     * Timer for printed stats
     **/
    if (timer_period > 0) {
      /* advance the timer */
      timer_tsc += diff_tsc;
      /* if timer has reached its timeout */
      if (unlikely(timer_tsc >= (uint64_t) timer_period)) {
	print_stats();
	print_port_stats();
	/* reset the timer */
	timer_tsc = 0;
      }
    }


    /**
     * Timer for logged stats
     **/
    if (timer_stats > 0){
      timer_stats_tsc += diff_tsc;
      if (unlikely(timer_stats_tsc >= (uint64_t) timer_stats)) {
	write_stats(fp,cumulative_tsc);
	timer_stats_tsc = 0;
      }
    }
    prev_tsc = cur_tsc;
		

    int return_value = -1;
    for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
      /**
       * check messages 
       **/
      return_value = get_cmd(lcore_id, &msg, I_AM_MASTER);
      if (return_value == 0){
	/* success */
	if (msg.cmd == CFG){
	  /* config */
	  if(main_set_lcore_cfg(lcore_id)==0){
	    init_config--;
	  } else {
	    RTE_LOG(ERR,MASTER, "Failed to send cfg to slave %d\n",lcore_id);
	  }
	}
      }//endif
    }//endfor
      
    /**
     * Check any slave need restart or recreate
     * Need lock (threads)
     **/
    rte_spinlock_lock(&mutex_lock);
    for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
      struct lcore_resource_s *lcore_res  = &lcore_resource[lcore_id];
      
      if ((lcore_res->enabled==1) && (lcore_res->flags==SLAVE_RECREATE_FLAG)) {
	//struct lcore_resource_struct *pair = &lcore_resource[lcore_res->pair_id];
	  
	if(reset_lcore(lcore_res->lcore_id)!=0){
	  fprintf(stderr,"ERROR reset\n");
	} else {
	  lcore_res->flags&=~SLAVE_RECREATE_FLAG;
	}
      }
	
    }//endfor
    rte_spinlock_unlock(&mutex_lock);
  }//end while

      
}



/****************************** Main *************************************/
/*
 */
int main(int argc, char **argv){
  int return_value;

  uint8_t port_id;
  uint8_t nb_ports;
  uint8_t nb_ports_available;

  struct lcore_resource_s* lcore_res;

  unsigned mempool_flags;

  int i;

  /********************** Log file  ************************/
  /* not all log information goes to log_fd */
  /* log file */
  //  FILE *log_fd;
  if ((main_log_fd = fopen("log_master","w+")) == NULL){
    RTE_LOG(ERR,MASTER,"ERROR open file (log)\n");
  } else { //TODO fix that
    if (rte_openlog_stream(main_log_fd) != 0){
      RTE_LOG(ERR,MASTER,"ERROR openlog_stream (log)\n");
    }
  }
  rte_set_log_level(RTE_LOG_DEBUG);
  RTE_LOG(INFO,MASTER,"Log file :\n");//TODO close that

  /********************** affinity init  ************************/
  /* Save cpu_affinity first, restore it in case it's floating process 
     option in struct cpu_aff_arg*/
  if (get_cpu_affinity() != 0){
    rte_exit(EXIT_FAILURE, "get_cpu_affinity error\n");
  }
  
  /* Also tries to set cpu affinity to detect whether  it will fail in 
     child process */
  if(clear_cpu_affinity() != 0){
    rte_exit(EXIT_FAILURE, "clear_cpu_affinity error\n");
  }
 

  /********************** EAL init  ************************/
  /*initialisation EAL*/
  printf("%s %s\n",indentation,singleborder);
  printf("EAL init\n\n");
  return_value = rte_eal_init(argc, argv);
  if (return_value < 0){
    rte_exit(EXIT_FAILURE,"Cannot init EAL\n");
  }
  argc -= return_value;
  argv += return_value;

  /********************** Default values ************************/
  /**
   * These values should be declared before parsing args
   **/ 
  timer_period = 10 * TIMER_MILLISECOND * 1000;
  timer_stats = 0;
  

  /********************** Parse args  ************************/
  /* parse application arguments (after the EAL ones) */
  return_value =  parse_args(argc, argv);
  if (return_value < 0){
    rte_exit(EXIT_FAILURE, "Invalid arguments\n");
  }

  

  /********************** Multi process init ************************/
  /* flib init */
  /* init memory for process information */
  printf("Flib initialisation : \n");
  printf("%s %s\n",indentation,singleborder);
  
  if (flib_init() != 0){
    rte_exit(EXIT_FAILURE, "flib init error");
  }

  /* intialize memory for shared ressources */
  printf("Statistics memory initialization : \n");
  printf("%s %s\n",indentation,singleborder);
  
  if (bridge_malloc_shared_struct() != 0){
    rte_exit(EXIT_FAILURE, "malloc mem failed\n");
  }
  
  /* Initialize lcore_resource structures */
  printf("Struct memory initialization : \n");
  printf("%s %s\n",indentation,singleborder);

  memset(lcore_resource, 0, sizeof(lcore_resource));
  for (i = 0; i < RTE_MAX_LCORE; i++){//TODO utility ?
    lcore_resource[i].lcore_id = i;
  }


  /********************** Port check  ************************/
  /* Check number of ports to send/receive */
  /* Rte_eth_dev_count = get total number of eth devices  */
 
  printf("Port check : \n");
  printf("%s %s\n",indentation,singleborder);


  nb_ports = rte_eth_dev_count();
  main_information.port_nb = nb_ports;
  if (nb_ports < 1){
    rte_exit(EXIT_FAILURE, "Error: need at least 1 port with dpdk driver\n");
  }

  if (nb_ports > RTE_MAX_ETHPORTS){
    nb_ports = RTE_MAX_ETHPORTS;
  }

 
  printf("%s Number of dpdk ports = %d\n",indentation,nb_ports);

  /********************** Mempool init ************************/
  /* Creates a new mempool in memory to  build packets */
  printf("%s %s\n",indentation,singleborder);
  printf("Create mempools for packets\n");
  
  /* char mpool_name[15]; */
  /* if (snprintf(mpool_name,12,"MASTER_POOL_") != 12){ */
  /*   RTE_LOG(ERR, MEMPOOL, "error MEMPOOL name : %s \n format not correct \n ", */
  /* 	    mpool_name); */
  /*   rte_exit(EXIT_FAILURE, "error MEMPOOL"); */
	
  /* } */


  for (port_id = 0; port_id < nb_ports; port_id++){
    /* skip ports that are not enabled */
    if (( main_information.port_mask & (1 << port_id)) == 0){
      continue;
    }
    
    char buf_name[RTE_MEMPOOL_NAMESIZE];
    mempool_flags = MEMPOOL_F_SP_PUT | MEMPOOL_F_SC_GET;
    snprintf(buf_name, RTE_MEMPOOL_NAMESIZE, MBUF_NAME, port_id);
    
    bridge_pool[port_id] = 
      rte_mempool_create(buf_name,
			 NUM_MBUFS * nb_ports, /* max mbuf */
			 MBUF_SIZE, 
			 MBUF_CACHE_SIZE,
			 sizeof(struct rte_pktmbuf_pool_private),
			 rte_pktmbuf_pool_init, NULL,
			 rte_pktmbuf_init, NULL,
			 rte_socket_id(), 
			 mempool_flags);
    
    if (bridge_pool[port_id] == NULL){
      RTE_LOG(ERR, EAL, "  cannot create mbuf pool, "
	      "error %i (%s)\n", rte_errno, strerror(rte_errno));
      rte_exit(EXIT_FAILURE, "Cannot init mbuf pool\n");
    }
    
    printf("%s Create mbuf %s\n",indentation, buf_name);
  }

  /******************* Mempool for IPC *******************/
 printf("%s %s\n",indentation,singleborder);
 message_pool = 
    rte_mempool_create("message_pool",
		       IPC_NB_MSGBUF * RTE_MAX_LCORE, /* # element */
		       sizeof(struct bridge_msg_s),         /* ele size */
		       IPC_NB_MSGBUF / 2,             /* cache_size */
		       0,                              /* private size*/
		       rte_pktmbuf_pool_init, NULL,    /* launch fun */
		       rte_pktmbuf_init, NULL,             
		       rte_socket_id(),
		       0);
  
  if (message_pool == NULL){
    rte_exit(EXIT_FAILURE, "Create msg mempool failed\n");
  }
  printf("Create mempools for IPC\n");  

  /********************** Tx queue init  ************************/
  /* set TX QUEUE */
  /* reset bridge_dst_ports */        
  //TODO REMOVE THAT ???
	

  unsigned nb_ports_in_mask = 0;
  //  for (port_id = 0; port_id < RTE_MAX_ETHPORTS; port_id++)
  /*   bridge_dst_ports[port_id] = 0; */
  /* last_port = 0; */

  /* each lcore = a dedicated TX queue on each port */
  for (port_id = 0; port_id < nb_ports; port_id++) {
    /* skip ports that are not enabled */
    if ((main_information.port_mask & (1 << port_id)) == 0){
      continue;
    }
    /* if (nb_ports_in_mask % 2) { */
    /*   bridge_dst_ports[port_id] = last_port; */
    /*   bridge_dst_ports[last_port] = port_id; */
    /* } else { */
    /*   last_port = port_id; */
    /* } */
    //    struct rte_eth_dev_info dev_info;

    rte_eth_dev_info_get(port_id, &port_queue_conf[port_id].port_conf);

    struct port_queue_conf_s *port_conf ;     
    port_conf = &port_queue_conf[port_id];
    port_conf->port_conf.default_txconf.txq_flags &= ~ETH_TXQ_FLAGS_NOMULTSEGS;

    nb_ports_in_mask++;
    //AZERT
    
  }//endfor

  if (nb_ports_in_mask % 2) {
    RTE_LOG(WARNING,MASTER,"WARNING: Too many lcores enabled.");
    printf("Notice: odd number of ports in portmask.\n");
    //    bridge_dst_ports[last_port] = last_port;
  }

  /* total ports available (with mask) */
  //main_information.port_enabled_nb = nb_ports_in_mask;
  
  rte_spinlock_init(&mutex_lock);

  /********************** Lcore init  ************************/
  /**
   * Initialize Lcore configuration
   **/
  
  /* search core configured for that port */

  printf("%s%s\n",border,border);
  printf("Configuration of lcore :\n");
  //  for (i = 0; i < RTE_MAX_LCORE; i++){
  for (i = rte_get_next_lcore(-1,1,0);
       i < RTE_MAX_LCORE;
       i = rte_get_next_lcore(i,1,0)){
  // RTE_LCORE_FOREACH_SLAVE(i) { 
    printf("toto\n");
    //    proc_cfg = &proc_config[i];
    //    if (proc_cfg->enabled ==1) {
    lcore_res = &lcore_resource[i]; 
    lcore_res->enabled = 1;
    
  }//endfor

  /********************** Port Initialisation  ************************/
  /**
   * Initialize all ports
   **/
  printf("%s%s\n",border,border);
  printf("Initialisation of ports :\n");
  fflush(stdout);
  nb_ports_available = nb_ports;
  for (port_id = 0; port_id < nb_ports; port_id++){
    printf("%s %s\n",indentation,singleborder);
    printf("\nInit port n° %"PRIu8 "\n",port_id);

    /* skip disabled ports */
    if (( main_information.port_mask & (1 << port_id)) == 0) {
      printf("Skipping disabled port %u\n", (unsigned) port_id);
      nb_ports_available--;
      continue;
    }


    struct port_queue_conf_s *port_qconf;
    port_qconf = &port_queue_conf[port_id];
    if (port_qconf->queue_tx_nb == 0 ){
      port_qconf->queue_tx_nb++;
    }
    if (port_qconf->queue_rx_nb == 0 ){
      port_qconf->queue_rx_nb++;
    }


    /* port init */
    if (port_init(port_id, bridge_pool[port_id]) != 0){
      rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8 "\n", port_id);
    }

    printf("\n");


    
  }

  main_information.port_nb_enabled = nb_ports_available;


  /* Display the port MAC address. */
  printf("%s %s\n",indentation,singleborder);
    
  for (port_id = 0; port_id < nb_ports; port_id++){
    /* skip disabled ports */
    if (( main_information.port_mask & (1 << port_id)) == 0) {
      continue;
    }

    struct ether_addr addr;
    rte_eth_macaddr_get(port_id, &addr);
    printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
	   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
	   (unsigned)port_id,
	   addr.addr_bytes[0], addr.addr_bytes[1],
	   addr.addr_bytes[2], addr.addr_bytes[3],
	   addr.addr_bytes[4], addr.addr_bytes[5]);
    
  }

  if (!nb_ports_available) {
    rte_exit(EXIT_FAILURE,"All available ports are disabled."
	     "Please set portmask.\n");
  }

  /****************** wait till device up ***************/
  printf("%s%s\n",border,border);
  check_all_ports_link_status(nb_ports,  main_information.port_mask);


  printf("%s%s\n",border,border);

  /****************** FDIR add rules ***************/
  if (main_information.fdir ==1) {
  
    if (fdir_main(main_information.fdir_file_name) != 0){
      rte_exit(EXIT_FAILURE, "Cannot add rules in FDIR %"PRIu8 "\n", port_id);
    }

      for (port_id = 0; port_id < nb_ports; port_id++){
	if (( main_information.port_mask & (1 << port_id)) == 0) {
	  continue;
	}
	check_fdir_capabilities(port_id);
      }
  }
  
  /******************** Ring for each master/slave pair **********/
  /**
   * - create ring message between Master and slave
   * - register callback fct when slave leaves 
   **/
  printf("%s%s\n",border,border);
  printf("create ring message for IPC\n");
  for (i = 0; i < RTE_MAX_LCORE; i++) {
    
    if (lcore_resource[i].enabled) {
      /* Create ring for master and slave communication */
      return_value = create_ms_ring(i);
      if (return_value != 0)
	rte_exit(EXIT_FAILURE, "Create ring for lcore=%u failed",i);
      
      if ((return_value=flib_register_slave_exit_notify(i,slave_exit_cb)) != 0){
	RTE_LOG(ERR, EAL, "error register slave," 
		"error %i (%s)\n", rte_errno, strerror(rte_errno)); 
	//	fprintf(stderr,"error :%d")
	rte_exit(EXIT_FAILURE, 
		 "Register master_slave_exit"
		 "callback function failed");
      }
    }
  }
  /****************** Main information ****************/
  main_information.main_id = rte_lcore_id();
  main_information.float_proc = 1;


  /****************** Launch app ! ****************/

  printf("%s\n",singleborder);
  printf("number of cores : %d\n",rte_lcore_count());
  
  /**
   * - launch fct with arguments
   **/
  flib_mp_remote_launch(bridge_launch_one_lcore, 
			NULL,
			SKIP_MASTER);
  
  // old way to launc lcores
  // unsigned lcore_id;
  /* RTE_LCORE_FOREACH_SLAVE(lcore_id) { */
  /*   /\* if(rte_eal_remote_launch(lcore_bridge,NULL, lcore_id) != 0){ *\/ */
  /*   /\*   rte_exit(EXIT_FAILURE, "error lcore\n"); *\/ */
  /*   /\* } *\/ */
  /*   struct bridge_msg_s *msg2send; */
  /*   memset(&msg2send,0,sizeof(struct bridge_msg_s)); */
  /*   msg2send->cmd = PING; */
  /*   if (master_send_cmd_with_ack(lcore_id,msg2send)!=0){ */
  /*     fprintf(stderr,"error"); */
  /*   } */
  /* } */
  
  
  printf("%s%s\n",border,border);

  /* master lcore */
  printf("%s\n",singleborder);

  main_loop();

  rte_eal_mp_wait_lcore();

  return 0;
}
