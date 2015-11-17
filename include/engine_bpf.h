

struct pcap_bpf_program {
  u_int bf_len;
  struct bpf_insn *bf_insns;
};

//int compile_bpf_filter(__attribute__((unused)) void);
//int  compile_pcap_bpf_program(struct pcap_bpf_program *pcap_bpf);
int BPF_pkt_filter(char *pkt, uint32_t pkt_length);
int BPF_init(char __attribute__((unused)) *tmp);
