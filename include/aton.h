
#include <ctype.h>
#include <rte_ether.h>

struct ether_addr * ether_aton_r (const char *asc, struct ether_addr * addr);
struct ether_addr * ether_aton (const char *asc);
