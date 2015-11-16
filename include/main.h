#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/queue.h>
#include <getopt.h> // getopt_long
#include <sched.h>

#include <fcntl.h> // openfile


#include <rte_common.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_cycles.h>
#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_eth_ctrl.h>
#include <rte_errno.h>
#include <rte_spinlock.h>
#include <rte_hash.h>
#include <rte_hash_crc.h>
#include <rte_mbuf.h>
#include <rte_ip.h>





#define RX_RING_SIZE 512 //128
#define TX_RING_SIZE 512
#define NB_QUEUE 1
#define MAX_PKT_BURST 64 //32




/******** Packets Mempool args ************/
#define NUM_MBUFS 8192
#define MBUF_SIZE (2048 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#define MBUF_CACHE_SIZE 256 /* cache size for per_lcore 
			     * cache_size should be
			     * NUM_MBFUS % CACHE_SIZE = 0
			     */
#define MBUF_NAME  "bridge_pool_%d"
struct rte_mempool * bridge_pool[RTE_MAX_ETHPORTS]; //TODO static or not


/*************************************/
#define TX_PTHRESH 36 /**< Default values of TX prefetch threshold reg. */
#define TX_HTHRESH 0  /**< Default values of TX host threshold reg. */
#define TX_WTHRESH 0  /**< Default values of TX write-back threshold reg. */



/******* Log ***********/
FILE *main_log_fd;    /* main log fd */

#define LOG_NAME "log_%d" //error TODO
#define LOG_MAX_SIZE 128 /* log name max size */

#define RTE_LOGTYPE_BRIDGE RTE_LOGTYPE_USER1
#define RTE_LOGTYPE_MASTER RTE_LOGTYPE_USER2

/* for queues */
#define MAX_RX_QUEUE_PER_LCORE 16
#define MAX_TX_QUEUE_PER_PORT 16

/*********************************************/
#define MAX_FILE_NAME 128

typedef struct main_information_s {
  uint8_t port_nb;     /* nb of all ports */
  uint8_t port_nb_enabled;     /* nb of ports in mask*/
  uint32_t port_mask;  /* enabled port mask */
  //  uint32_t port_enabled_nb; /* nb of enabled port */
  /* use floating processes  allow changing affinity */
  int float_proc; 
  int proc_config_nb; /* number of slaves defined by config_master file */
  int proc_config_used_nb; /* number of salves in used */

  char config_file_name[MAX_FILE_NAME];

  int fdir ; /* activation of fdir*/
  char fdir_file_name[MAX_FILE_NAME]; /* fdir configuration file name */
  

  unsigned main_id; /* main lcore_id */
  
} __attribute__((packed)) __rte_cache_aligned main_information_t;

main_information_t main_information;



/*******************************************/
/* Multi process */
/* adapted from l2fwd_fork */

#define SLAVE_RING_MAX_NAME_LEN    32
#define CONFIG_MAX_ENGINE_LEN 64
#define CONFIG_MAX_ARGS_LEN 128



/* Save original cpu affinity */
struct cpu_aff_arg{
  cpu_set_t set;
  size_t size;
}cpu_aff;


/*******************************************/
/**
 * configuration file (contains config file information)
 *
 **/
typedef struct rte_hash lookup_struct_t;


struct proc_config_s {
  int enabled;    /* ==1 if enabled */
  int used;       /* ==1 if a proc used this cfg */
  uint8_t port_rx;
  uint8_t port_tx; /* Port id for that lcore 
		      to receive and forward packets */
  uint8_t queue_rx; /* Queue id to receive and forward pkts */
  uint8_t queue_tx;
  char engine[CONFIG_MAX_ENGINE_LEN];        /* engine */
  char args[CONFIG_MAX_ARGS_LEN];          /* arguments of engine function*/
  lookup_struct_t * ipv4_lookup_struct;
  lookup_struct_t * ipv6_lookup_struct;
 
  uint16_t lcore_id;   /* lcore_id using this config */

}__attribute__((packed)) __rte_cache_aligned;

/* slace process information */
struct proc_config_s proc_config[RTE_MAX_LCORE];



/*************** Struct per lcore (proc)  **********/
/**
 * struct contained information for
 * - IPC
 * - Rx/Tx Queues and ports
 * - 
 **/
struct lcore_resource_s {
  int enabled;    /* Only set in case this lcore involved into 
		     packet forwarding */
  int flags;          /* Set only slave need to restart or recreate */

  unsigned config_id; /* config id (0 == no config)*/
  unsigned lcore_id;  /*  lcore ID */
  unsigned pair_id;    /* dependency lcore ID on port */
  

  /* IPC ring[0] for master send cmd, slave read 
   *     ring[1] for slave send ack, master read 
   */
  char ring_name[2][ SLAVE_RING_MAX_NAME_LEN];
  struct rte_ring *ring[2];
  //  struct proc_config_s proc_config;
}__attribute__((packed)) __rte_cache_aligned;

/* slace process information */
struct lcore_resource_s lcore_resource[RTE_MAX_LCORE];

/* mutex */
rte_spinlock_t mutex_lock;

/********* IPC Mempool args *********/
#define IPC_NB_MSGBUF  64
#define IPC_MSG_SIZE 128
#define RING_MASTER_NAME        "bridge_m2s_"
#define RING_SLAVE_NAME         "bridge_s2m_"

/* for ipc ring communication */
#define I_AM_MASTER 1
#define I_AM_SLAVE 0


enum bridge_cmd{
  START, 
  STOP,
  PING,
  PROC,
  CFG
};

struct bridge_msg_s{
  enum bridge_cmd cmd;
  char msg[IPC_MSG_SIZE];
};

struct rte_mempool *message_pool;

/******************************************/

/* RECREATE flag indicate needs initialize 
   resource and launch slave_core again */
#define SLAVE_RECREATE_FLAG 0x1
#define SLAVE_DONT_RECREATE_FLAG 0x2

/* configuration of port */
struct port_queue_conf_s {
  uint16_t queue_rx_nb;
  uint16_t queue_tx_nb;
  struct rte_eth_dev_info port_conf;
}__attribute__((packed)) __rte_cache_aligned;

struct port_queue_conf_s port_queue_conf[RTE_MAX_ETHPORTS];


/************************************/
/* TODO static or not ? */
//uint32_t bridge_dst_ports[RTE_MAX_ETHPORTS];




/*******************************************/
/* malloced struct  */

/* Per-port statistics struct */
struct bridge_port_statistics {
  uint64_t tx;
  uint64_t rx;
  uint64_t filtered;
  uint64_t dropped;
  uint64_t rx_b;      /* tx_b = rx_b - dropped_b - filtered_b; */
  uint64_t filtered_b;
  uint64_t dropped_b;
  int pid;
} __rte_cache_aligned;
struct bridge_port_statistics *port_statistics;



/**
 * pointer to lcore ID mapping array, used to return lcore id in case slave
 * process exited unexpectedly, use only floating process option applied
 **/
unsigned *mapping_id;
  

/* LCORE_ID_ANY = UNIT32_MAX */
#define INVALID_MAPPING_ID      ((unsigned)LCORE_ID_ANY)


/**************************************/
/**
 * Timers
 **/

/* timer for printed stats and TX drain */
char timer_file[MAX_FILE_NAME];
int64_t timer_period;

/* Timer for writed stats */
int64_t timer_stats;
#define BUFSIZE 512


#define TIMER_MILLISECOND 2660046ULL /* around 1ms at 2 Ghz */
#define MAX_TIMER_PERIOD 86400 /* 1 day max */
#define BURST_TX_DRAIN_US 100 /* TX drain every ~100us */



