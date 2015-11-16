#include "aton.h"

static inline int
xdigit (char c) {
  unsigned d;
  d = (unsigned)(c-'0');
  if (d < 10) return (int)d;
  d = (unsigned)(c-'a');
  if (d < 6) return (int)(10+d);
  d = (unsigned)(c-'A');
  if (d < 6) return (int)(10+d);
  return -1;
}

/*
 * Convert Ethernet address in the standard hex-digits-and-colons to binary
 * representation.
 * Re-entrant version (GNU extensions)
 */
struct ether_addr * ether_aton_r (const char *asc, struct ether_addr * addr)
{
  int i, val0, val1;
  for (i = 0; i < ETHER_ADDR_LEN; ++i) {
    val0 = xdigit(*asc);
    asc++;
    if (val0 < 0)
      return NULL;
    
    val1 = xdigit(*asc);
    asc++;
    if (val1 < 0)
      return NULL;
    
    addr->addr_bytes[i] = (u_int8_t)((val0 << 4) + val1);
    if (i < ETHER_ADDR_LEN - 1) {
      if (*asc != ':')
	return NULL;
      asc++;
    }
  }
  if (*asc != '\0')
    return NULL;
  return addr;
}





/*
 * Convert Ethernet address in the standard hex-digits-and-colons to binary
 * representation.
 */
struct ether_addr *
ether_aton (const char *asc)
{
  static struct ether_addr addr;
  return ether_aton_r(asc, &addr);
}

