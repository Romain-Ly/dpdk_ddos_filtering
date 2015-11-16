#include "main.h"
#include "engine_misc.h"



/**
 * Drop packets (modulo)
 **/

static int modulo;


inline int
modulo_engine(__attribute__((unused)) char *pkt, 
	      __attribute__((unused)) uint32_t pkt_length){
  static int n = 0;
  n++;
  if (n % modulo == 0){
    return 0; 
  } else {
    return -1; //drop pkt
  }
    
}


int 
modulo_init(__attribute__((unused)) char *args){
  modulo = atoi(args);
  if (modulo < 0){
    fprintf(stderr,"ERROR, args (%s) should be > 0, Leaving",args);
    return -1;
  }
  return 0;
}
