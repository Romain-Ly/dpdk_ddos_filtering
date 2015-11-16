int hash_pkt_filter(struct  rte_mbuf *m, int32_t *ret);

int hash_pkt_filter4(struct  rte_mbuf* m[4], int32_t *ret);
int hash_pkt_filter4_ipv6(struct  rte_mbuf* m[4], int32_t *ret);
int init_hash(char *file_path,int proc_id,int socket_id);
