//int BPF_pcap_filter(u_char *pkt, uint32_t pkt_length);
int BPF_pcap_init(char __attribute__((unused)) *file_path);









	 /* pcap_t *handle;		/\\* Session handle *\/ */
	 /* char dev[] = "rl0";		/\* Device to sniff on *\/ */
	 /* char errbuf[PCAP_ERRBUF_SIZE];	/\* Error string *\/ */
	 /* struct bpf_program fp;		/\* The compiled filter expression *\/ */
	 /* char filter_exp[] = "port 23";	/\* The filter expression *\/ */
	 /* bpf_u_int32 mask;		/\* The netmask of our sniffing device *\/ */
	 /* bpf_u_int32 net;		/\* The IP of our sniffing device *\/ */

	 /* if (pcap_lookupnet(dev, &net, &mask, errbuf) == -1) { */
	 /* 	 fprintf(stderr, "Can't get netmask for device %s\n", dev); */
	 /* 	 net = 0; */
	 /* 	 mask = 0; */
	 /* } */
	 /* handle = pcap_open_live(dev, BUFSIZ, 1, 1000, errbuf); */
	 /* if (handle == NULL) { */
	 /* 	 fprintf(stderr, "Couldn't open device %s: %s\n", dev, errbuf); */
	 /* 	 return(2); */
	 /* } */
	 /* if (pcap_compile(handle, &fp, filter_exp, 0, net) == -1) { */
	 /* 	 fprintf(stderr, "Couldn't parse filter %s: %s\n", filter_exp, pcap_geterr(handle)); */
	 /* 	 return(2); */
	 /* } */
	 /* if (pcap_setfilter(handle, &fp) == -1) { */
	 /* 	 fprintf(stderr, "Couldn't install filter %s: %s\n", filter_exp, pcap_geterr(handle)); */
	 /* 	 return(2); */
	 /* } */
