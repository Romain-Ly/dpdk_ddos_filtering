

int send_cmd(unsigned slave_id, struct bridge_msg_s * msg2send, int is_master);
int get_cmd(unsigned slave_id, struct bridge_msg_s * msg2rcv, int is_master);
int master_send_cmd_with_ack(unsigned slave_id, struct bridge_msg_s * msg2send);
int slave_request(unsigned slave_id, struct bridge_msg_s * msg2send, 
		  struct bridge_msg_s * msg2recv);

int clear_cpu_affinity(void);
int get_cpu_affinity(void);
int bridge_malloc_shared_struct(void);
int create_ms_ring(unsigned slaveid);
//void slave_exit_cb(unsigned slave_id, __attribute__((unused))int stat);
