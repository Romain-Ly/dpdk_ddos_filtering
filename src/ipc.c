#define _GNU_SOURCE

#include "main.h" 
#include "ipc.h" 
#include "flib.h"

/* /\* mutex *\/ */
/* static rte_spinlock_t mutex_lock = RTE_SPINLOCK_INITIALIZER; */



/*************** message between master and slaves *********/

/* send command to pair in paired master and slave ring */
int
send_cmd(unsigned slave_id, struct bridge_msg_s * msg2send, int is_master){
  
  struct lcore_resource_s *lcore_res = &lcore_resource[slave_id];
  void *bridge_msg_void;
  int fd = !is_master;
  /* fd = 0 master send, slave read */
  /* fd =1 slave send, master read */

  /* Only check master, it must be enabled and running if it is slave */
  if (is_master && !lcore_res->enabled){
    return -1;
  }
  
  if (lcore_res->ring[fd] == NULL){
    return -1;
  }
  
  if (rte_mempool_get(message_pool, &bridge_msg_void) < 0) {
    printf("Error to get message buffer\n");
    return -1;
  }

  *(struct bridge_msg_s *)bridge_msg_void = *msg2send;

  if (rte_ring_enqueue(lcore_res->ring[fd], bridge_msg_void) != 0) {
    printf("Enqueue error\n");
    rte_mempool_put(message_pool, bridge_msg_void);
    return -1;
  }
  
  return 0;
}



/* Get command from pair in paired master and slave ring */
int
get_cmd(unsigned slave_id, struct bridge_msg_s * msg2rcv, int is_master){

  struct lcore_resource_s *lcore_res = &lcore_resource[slave_id];
  void *bridge_msg_void;
  int fd = !!is_master;
  int retval;
  
  /* Only check master, it must be enabled and running if it is slave */
  if (is_master && (!lcore_res->enabled)){
    return -1;
  }
  
  if (lcore_res->ring[fd] == NULL){
    return -1;
  }
  
  retval = rte_ring_dequeue(lcore_res->ring[fd], &bridge_msg_void);
  
  if (retval == 0) { /* = success */
    *msg2rcv = *(struct bridge_msg_s *)bridge_msg_void;
    rte_mempool_put(message_pool, bridge_msg_void);
  }

  return retval;
}




/* Master send command to slave and wait until ack received or error met */
int
master_send_cmd_with_ack(unsigned slave_id, struct bridge_msg_s * msg2send){

  struct bridge_msg_s msg_ack;
  int ret = -1;
  
  if (send_cmd(slave_id, msg2send, I_AM_MASTER) != 0){
    RTE_LOG(ERR,BRIDGE, 
	    "Failed to send message from master to slave %u\n",
	    slave_id);
    //rte_exit(EXIT_FAILURE, "Failed to send message\n");
  }
  
  /* Get ack, akc = repeat command name*/
  while (1) {
    ret = get_cmd(slave_id, &msg_ack, I_AM_MASTER);
    if (ret == 0 && msg2send->cmd == msg_ack.cmd ){
      /* success */
      break;
    }
    /* If slave not running yet, return an error */
    if (flib_query_slave_status(slave_id) != ST_RUN) {
      ret = -ENOENT;
      break;
    }
  }
  
  return ret;
}

/* slave send cmd to master and get a response */
int
slave_request(unsigned slave_id, struct bridge_msg_s * msg2send, 
	      struct bridge_msg_s * msg2recv){


  int ret = -1;

  /* send a message */
  if (send_cmd(slave_id, msg2send, I_AM_SLAVE) != 0){
    RTE_LOG(CRIT,BRIDGE,"SLAVE %d failed to request CFG",rte_lcore_id());
  }
  

  while (1) {
    ret = get_cmd(slave_id, msg2recv, I_AM_SLAVE);
    if (ret == 0 && msg2send->cmd == msg2recv->cmd ){
      /* success */
      break;
    }
    /* /\* If slave not running yet, return an error *\/ */
    /* if (flib_query_slave_status(main_information.main_id) != ST_RUN) { */
    /*   ret = -ENOENT; */
    /*   break; */
    /* } */
  }
  
  return ret;
}


/***************************** Affinity  ********************************/

/* clear affinity struct */
int
clear_cpu_affinity(void){
  int s;
  
  s = sched_setaffinity(0, cpu_aff.size, &cpu_aff.set);
  if (s != 0) {
    fprintf(stderr,"sched_setaffinity failed:%s\n", strerror(errno));
    RTE_LOG(ERR, USER1, "sched_setaffinity failed:%s\n ",strerror(errno));
    return -1;
  }
  return 0;
}


int
get_cpu_affinity(void){
  int s;
  
  cpu_aff.size = sizeof(cpu_set_t);
  /* fromsched.h macro (not dpdk)
   * clear set that it contains no CPU
   */
  CPU_ZERO(&cpu_aff.set); 
  
  s = sched_getaffinity(0, cpu_aff.size, &cpu_aff.set);
  if (s != 0) {
    fprintf(stderr,"sched_getaffinity failed:%s\n", strerror(errno));
    RTE_LOG(ERR, USER1, "sched_getaffinity failed:%s\n ",strerror(errno));
    return -1;
  }
  return 0;
}





/* Malloc with rte_malloc on structures that shared by master and slave */
int
bridge_malloc_shared_struct(void){
  /* malloc port_statistics struct */
  port_statistics = 
    rte_zmalloc("bridge_port_statistics",
		sizeof(struct bridge_port_statistics)
		* main_information.proc_config_nb, //stats per processus
		0);
  
  if (port_statistics == NULL){
    RTE_LOG(ERR, MALLOC, "rte_zmalloc failed (bridge_port_statistics) %s\n ",
	    strerror(errno));
    return -1;
  }
  
  /* main_information = rte_zmalloc("main_information", */
  /* 				 sizeof(struct main_information_s), */
  /* 				 0); */
  /* if (main_information == NULL){ */
  /*   RTE_LOG(ERR, MALLOC, "rte_zmalloc failed (main_information_s) %s\n ", */
  /* 	    strerror(errno)); */
  /*   return -1; */
  /* } */ //TODO remove 
				 
  
  /* allocate  mapping_id array */
  //  if (float_proc) { /* here always float proc */
  
  int i;
  mapping_id = rte_malloc("mapping_id", 
			  sizeof(unsigned) * RTE_MAX_LCORE,
			  0);
  
  if (mapping_id == NULL) {
    /* rte_free(main_information); */
    RTE_LOG(ERR, MALLOC, "rte_malloc failed %s\n ",strerror(errno));
    return -1;
  }
  
  for (i = 0 ;i < RTE_MAX_LCORE; i++){
    mapping_id[i] = INVALID_MAPPING_ID;
  };
  // }
  return 0;
}



/***************************** IPC  ********************************/
/**
 *  create ring  or re-attach an existed ring .
 **/
static struct rte_ring *
create_ring(const char *ring_name, unsigned count,
	    int socket_id,unsigned flags){

  struct rte_ring *ring;
  
  if (ring_name == NULL){
    return NULL;
  }
  
  /* If already create, just attached it */
  if (likely((ring = rte_ring_lookup(ring_name)) != NULL)){
    return ring;
  }
  
  /* First call it, create one */
  return rte_ring_create(ring_name,   /* name of ring */
			 count,       /* size power of 2 */
			 socket_id,   
			 flags);
}


/**
 *  create ring for IPC between master/slave
 **/
int
create_ms_ring(unsigned lcore_id){

  /* single producer scheme with rte_ring_enqueue and dequeue */
  unsigned flag = RING_F_SP_ENQ | RING_F_SC_DEQ;

  struct lcore_resource_s *lcore_res = &lcore_resource[lcore_id];
  unsigned socketid = rte_socket_id();
  
  /* Always assume create ring on master socket_id */
  /* Default only create a ring size 32 */
  
  snprintf(lcore_res->ring_name[0],
	    SLAVE_RING_MAX_NAME_LEN, "%s%u",
	   RING_MASTER_NAME, lcore_id);
  
  if ((lcore_res->ring[0] = 
       create_ring(lcore_res->ring_name[0], 
		   IPC_NB_MSGBUF,
		   socketid,
		   flag)) == NULL) {
    printf("Create master to slave ring %s failed\n", lcore_res->ring_name[0]);
    return -1;
  }
  
  snprintf(lcore_res->ring_name[1],
	    SLAVE_RING_MAX_NAME_LEN, "%s%u",
	   RING_SLAVE_NAME, lcore_id);
  if ((lcore_res->ring[1] =
       create_ring(lcore_res->ring_name[1],
		   IPC_NB_MSGBUF,
		   socketid, flag)) == NULL) {
    printf("Create slave to master ring %s failed\n", lcore_res->ring_name[1]);
    return -1;
  }
  
  return 0;
}





