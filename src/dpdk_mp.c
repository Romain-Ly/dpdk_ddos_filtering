#include "main.h"

#include "bridge.h"
#include "dpdk_mp.h"
#include "flib.h"
#include "ipc.h"



/**
 * remapping resource belong to slave_id to new lcore that gets from
 * flib_assign_lcore_id(), used only floating process option applied.
 *
 * @param slave_id
 *   original lcore_id that apply for remapping
 **/
void
remapping_slave_resource(unsigned old_id, unsigned new_id){
  /**
   * remapping lcore_resource 
   * memcpy(dst,src,size)     
   * new_id = map_id
   * old_id = slave_id
   **/

  memcpy(&lcore_resource[new_id], &lcore_resource[old_id],
	 sizeof(struct lcore_resource_s));
  
  
  /* /\* remapping lcore_queue_conf *\/ */
  /* memcpy(&lcore_queue_conf[map_id], &lcore_queue_conf[slave_id], */
  /* 	 sizeof(struct lcore_queue_conf)); */

}



/******************************************************************/
/**
 * callback fct when slave exit
 * /!\ We are in threads -> need lock
 */
void 
slave_exit_cb(unsigned lcore_id, __attribute__((unused))int stat) {
 
  struct lcore_resource_s *lcore_res = &lcore_resource[lcore_id];
  struct proc_config_s *proc_cfg = &proc_config[lcore_res->config_id];

  printf("Get slave %u leave info\n", lcore_id);
  if (!lcore_res->enabled) {
    printf("Lcore=%u not registered for it's exit\n", lcore_id);
    return;
  }

  rte_spinlock_lock(&mutex_lock);
  
  proc_cfg->used =0;
  /* Change the state and wait master to start them */
  if ((lcore_res->flags & SLAVE_DONT_RECREATE_FLAG)==0){
    lcore_res->flags = SLAVE_RECREATE_FLAG;
  }

  rte_spinlock_unlock(&mutex_lock);
}


/******************************************************************/
/**
 * Functions to recreate a slave after termination
 * adataped from l2fwd dpdk examples
 *
 **/



/**
 *  Free rings
 */
/* static int */
/* reset_shared_structures(unsigned lcore_id){ */

/*   struct lcore_resource_s *lcore_res = &lcore_resource[lcore_id]; */
/*   int retval = 0; */
  
/*     char buf_name[SLAVE_RING_MAX_NAME_LEN]; */
/*   struct rte_mempool *pool; */

/*   pool = rte_mempool_lookup(lcore_res->ring_name[0]); */
/*   /\* if (pool){ *\/ */
/*     printf("Lcore %d, ring mempool %s free object is %u\n", */
/*   	   lcore_res->lcore_id, */
/*   	   buf_name, */
/*   	   rte_mempool_count(lcore_res->rte_ring[0])); */

/*   /\* } else { *\/ */
/*   /\*   printf("Can't find mempool '%s'\n", buf_name); *\/ */
/*   /\*   retval =-1; *\/ */
/*   /\* } *\/ */

/*   /\* pool = rte_mempool_lookup(lcore_res->ring_name[1]); *\/ */
/*   /\* if (pool){ *\/ */
/*     printf("Lcore %d, ring mempool %s free object is %u\n",  */
/* 	   lcore_res->lcore_id, */
/* 	   buf_name, */
/* 	   rte_mempool_count()lcore_res->rte_ring[1]); */

/*   /\* } else { *\/ */
/*   /\*   printf("Can't find mempool '%s'\n", buf_name); *\/ */
/*   /\*   retval =-1; *\/ */
/*   /\* } *\/ */


    
/*   return retval; */
  
/* } */



/**
 * Call this function to re-create resource that needed for slave process that
 * exited in last instance
 **/
static int
init_slave_res(unsigned lcore_id){
  struct lcore_resource_s *lcore_res = &lcore_resource[lcore_id];
  struct bridge_msg_s drain;
  memset(&drain,0,sizeof(struct bridge_msg_s));

  if (!lcore_res->enabled) {
    printf("Lcore %u not enabled  enabled=%d\n",
	   lcore_id,
	   lcore_res->enabled);
    return -1;
  }
  
  /* Initialize ring */
  if (create_ms_ring(lcore_id) != 0){
    rte_exit(EXIT_FAILURE, "failed to create ring for slave %u\n",
	     lcore_id);
  }

  /* drain un-read buffer if have */
  while (get_cmd(lcore_id, &drain, I_AM_MASTER) == 0);
  while (get_cmd(lcore_id, &drain, I_AM_SLAVE) == 0);
  
  return 0;
}


static int
recreate_one_slave(unsigned lcore_id){
  int retval = 0;
  /* Re-initialize resource for stalled slave */
  if ((retval = init_slave_res(lcore_id)) != 0) {
    printf("Init slave=%u failed\n", lcore_id);
    return retval;
  }
  
  /**
   * - launch fct with arguments
   **/
  if ((retval = flib_remote_launch(bridge_launch_one_lcore, 
				   NULL,
				   lcore_id))
      != 0)
    printf("Launch slave %u failed\n", lcore_id);
  
  return retval;
}

/**
 *
 **/
int
reset_lcore(unsigned lcore_id){
  int retval;
  struct lcore_resource_s *lcore_res = &lcore_resource[lcore_id];
  struct proc_config_s *proc_config_t;

  /* if (reset_shared_structures(lcore_id)!=0){ */
  /*   return -1; */
  /* } */
  
  if (main_information.float_proc) {
    unsigned map_id = mapping_id[lcore_id];
    
    if (map_id != INVALID_MAPPING_ID) {
      printf("%u return mapping id %u\n", lcore_id, map_id);
      
      /* remove map_id from used id so we can used it again */
      flib_free_lcore_id(map_id);
      mapping_id[map_id] = INVALID_MAPPING_ID;
      
      proc_config_t = &proc_config[map_id];
      proc_config_t->used = 0;
    }
  }
  
  if (lcore_res->enabled ==1){
    if((retval = recreate_one_slave(lcore_id)) != 0){
      return -1;
    }   
  }
  
  lcore_res->flags=0;

  return 0;

}



















