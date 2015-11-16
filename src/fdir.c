#include "main.h"
#include "fdir.h"

#include <rte_ip.h>
//#include "fdir_yaml_parser.h"

/***************************** FDIR  ********************************/
/*
 *  PERFECT FILTER
 *
 */

int 
fdir_init(uint8_t port_id){
  int return_value;
  struct rte_eth_fdir_info fdir_info;

  /* check fdir card support */
  return_value = rte_eth_dev_filter_supported(port_id, RTE_ETH_FILTER_FDIR);
  if (return_value < 0) {
    RTE_LOG(CRIT,PORT,"FDIR not supported on port %-2d\n",port_id);
    return -1;

  }


  /* check filter is supported */
  return_value = rte_eth_dev_filter_ctrl(port_id,
				   RTE_ETH_FILTER_FDIR,
				   RTE_ETH_FILTER_NOP,
				   NULL
				   );


  
  rte_eth_dev_filter_ctrl(port_id,
			  RTE_ETH_FILTER_FDIR,
			  RTE_ETH_FILTER_INFO,
			  &fdir_info);

  /* check fdir is activated */
  if (fdir_info.mode == RTE_FDIR_MODE_PERFECT) {
    printf("MODE PERFECT RULE ENABLED\n");
  } else if (fdir_info.mode == RTE_FDIR_MODE_SIGNATURE) {
    printf("MODE SIGNATURE ENABLED\n");
  } else {
    RTE_LOG(CRIT,PORT,"FDIR not enabled on port %-2d\n",port_id);
    return -1;
  }


  if (return_value < 0){
    RTE_LOG(ERR, PORT, "error FDIR : FILTER not supported,"
	    "error %i (%s)\n", rte_errno, strerror(rte_errno));
    return rte_errno;
  }
  return 0;

}

/******************** FDIR MAIN ************************/
/**
 * Main functions
 **/

int
fdir_callback_add_rules(struct fdir_parsing* filter_tmp){
  
  /**
   *  struct fdir_parsing {
   *    int port;
   *    enum fdir_parsing_cmd cmd;
   *    struct rte_eth_fdir_filter filter;
   *   };
   **/
  int return_value ;
  int port_id = filter_tmp->port;
  
  if (( main_information.port_mask & (1 << port_id)) != 0) {
    return_value = rte_eth_dev_filter_ctrl(port_id,
					   RTE_ETH_FILTER_FDIR,
					   RTE_ETH_FILTER_ADD,
					   &filter_tmp->filter);


  
    if (return_value != 0 ){
      RTE_LOG(ERR, PORT, "error adding FDIR rules, error %i (%s)\n", 
	      rte_errno, strerror(rte_errno));
      return rte_errno;
    }
    
  }//endif
  return 0;
}



int
fdir_main(char * file_name){

  RTE_LOG(INFO,MASTER,"FDIR configuration file :%s\n",file_name);
  
  struct fdir_parsing filter_tmp;
  memset(&filter_tmp,0,sizeof(struct fdir_parsing));
  
  /**
   * yaml_parser
   * input : conf.yaml
   * output : struct fdir_parsing
   **/
  yaml_parser(file_name,&filter_tmp,fdir_callback_add_rules);
  return 0;//TODO Error catching
}















/************************ OLD functions ***********************/

/* int  */
/* fdir_add_rules(uint8_t port_id){ */

/*   int return_value; */
/*   struct rte_eth_fdir_filter fdir_rules; */

/*   memset(&fdir_rules, 0, sizeof(struct rte_eth_fdir_filter)); */

/*   /\* soft_id *\/ */
/*   fdir_rules.soft_id = 1; // with fdir_rules.action.report_status */
	
/*   /\* flow *\/ */
/*   fdir_rules.input.flow_type = RTE_ETH_FLOW_NONFRAG_IPV4_OTHER; */
/*   //defined in rte_eth_ctrl.h  */

/*   fdir_rules.input.flow.ip4_flow.src_ip = IPv4(0,3,16,172); //rte_ip.h */
/*   fdir_rules.input.flow.ip4_flow.dst_ip = 0;//IPv4(2,2,16,172); */
  
/*   /\* flow_ext *\/ */
/*   fdir_rules.input.flow_ext.vlan_tci = rte_cpu_to_be_16(0x0); */
  
/*   /\* actions *\/ */
/*   fdir_rules.action.flex_off = 0;  /\*use 0 by default *\/ */
/*   fdir_rules.action.behavior = RTE_ETH_FDIR_ACCEPT; //RTE_ETH_FDIR_ACCEPT;// */
  
/*   // or RTE_ETH_FDIR_NO_REPORT_STATUS; */
/*   fdir_rules.action.report_status = RTE_ETH_FDIR_REPORT_ID; */

/*   fdir_rules.action.rx_queue = 0; */
/*   fdir_rules.action.flex_off = 0 ; */

/*   return_value = rte_eth_dev_filter_ctrl(port_id, */
/* 				   RTE_ETH_FILTER_FDIR, */
/* 				   RTE_ETH_FILTER_ADD, */

/* 				   &fdir_rules); */
/*   if (return_value != 0 ){ */
/*     RTE_LOG(ERR, PORT, "error adding FDIR rules, error %i (%s)\n",  */
/* 	    rte_errno, strerror(rte_errno)); */
/*     return rte_errno; */
/*   } */

/*   return 0; */
/* } */




/* int  */
/* fdir_set_masks(__attribute__((unused)) uint8_t port_id){ */
/*   /\* struct rte_fdir_masks fdir_masks; *\/ */
/*   /\* /\\* int return_value; *\\/ *\/ */

/*   /\* /\\* /\\\* /\\\\* create filter mask*\\\\/ *\\\/ *\\/ *\/ */
/*   /\* /\\* /\\\* memset(&fdir_masks,0,sizeof(struct rte_fdir_masks)); *\\\/ *\\/ *\/ */
/*   /\* /\\* /\\\* fdir_masks.src_ipv4_mask = 0xffffff00; *\\\/ *\\/ *\/ */
/*   /\* /\\* /\\\* fdir_masks.only_ip_flow = 1; *\\\/ *\\/ *\/ */
  
/*   /\* /\\* /\\\* return_value = rte_eth_dev_fdir_set_masks(port_id, &fdir_masks); *\\\/ *\\/ *\/ */
/*   /\* /\\* /\\\* if ( return_value < 0) { *\\\/ *\\/ *\/ */
/*   /\* /\\* /\\\*   printf("return_value :%d \n",return_value); *\\\/ *\\/ *\/ */
/*   /\* /\\* /\\\*   RTE_LOG(ERR, PORT, "error setting FDIR masks, error %i (%s)\n", rte_errno, strerror(rte_errno)); *\\\/ *\\/ *\/ */
/*   /\* /\\* /\\\*   //return rte_errno; TODO (remove comments when done) *\\\/ *\\/ *\/ */
/*   /\* /\\* /\\\* } *\/  */

/*   //  struct rte_eth_fdir_masks *fdir_mask; */
/*   /\** port is not started **\/ */
  
/*   /\* fdir_mask = &port_conf_default.fdir_conf.mask; *\/ */
  
/*   /\* fdir_mask->vlan_tci_mask = 0; *\/ */
/*   /\* fdir_mask->ipv4_mask.src_ip = 0x0000ff00; *\/ */
/*   /\* fdir_mask->ipv4_mask.dst_ip = 0x00000000; *\/ */


/*   return 0; */
/* } */

