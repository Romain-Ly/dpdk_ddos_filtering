//void bridge_loop(int (*engine)(char**));

void 
bridge_loop_optimized(int (*engine4)(struct rte_mbuf*[4],int32_t*),
		      int (*engine)(struct rte_mbuf*,int32_t*),
		      struct slave_conf_s* slave_conf_t);
