#include <rte_ip.h>
#include <rte_icmp.h>
#include <rte_ether.h>
#include <rte_log.h>
//#include <rte_cycles.h>


#include <unistd.h> //sleep
#include <stdio.h>
#include <inttypes.h>// for printing uint16_t

/*-------------------------------------------*/

#define IPv4_VERSION    4
#define IPv6_VERSION	6





/* A tsc-based timer responsible for triggering statistics printout */
/* #define TIMER_MILLISECOND 2000000ULL /\* around 1ms at 2 Ghz *\/ */
/* #define MAX_TIMER_PERIOD 86400 /\* 1 day max *\/ */
//variable timer_period (static)

/* #define BURST_TX_DRAIN_US 100 /\* TX drain every ~100us *\/ */

/*-------------------------------------------*/

typedef struct packet_information_s {
  /* ethernet */
  uint64_t ethernet_addr_dst;
 } packet_information_t;


/* /\* Per-port statistics struct *\/ */
/* struct ping_port_statistics { */
/*   uint64_t tx; */
/*   uint64_t rx; */
/*   uint64_t dropped; */
/* } __rte_cache_aligned; */
/* struct ping_port_statistics * port_statistics; */


/* BRIDGE LOCAL conf */

struct mbuf_table {
  unsigned length;
  struct rte_mbuf *mbufs[MAX_PKT_BURST];
};

struct slave_conf_s {
  uint8_t conf_id;
  uint8_t port_rx;
  uint8_t port_tx; /* Port id for that lcore */
  uint8_t queue_rx; /* Queue id to receive and forward pkts */
  uint8_t queue_tx;
  struct mbuf_table mbuf_tx;

}__attribute__((packed)) __rte_cache_aligned; 
/*-------------------------------------------*/

int bridge_launch_one_lcore(__attribute__((unused)) void *dummy);
int lcore_bridge(void*  port_info);
void print_stats(__attribute__((unused)) void);

