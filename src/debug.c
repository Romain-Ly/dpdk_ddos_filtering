/**********************
 *
 * Debug 
 *
 *
 **********************/

#include "main.h"
#include "debug.h"

/**************** Static def ************************/

//static const char *warning_border = "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";


/****************  functions ************************/
void 
print_proc_config(int id){

  struct proc_config_s *proc_config_t;

  proc_config_t = &proc_config[id];
  RTE_LOG(INFO,BRIDGE,"proc_config id = %d \n",id);
  RTE_LOG(INFO,BRIDGE,"enabled %d\n",proc_config_t->enabled);
  RTE_LOG(INFO,BRIDGE,"engine : %s\n",proc_config_t->engine); 
  

}


/**
 *Check arithmetics operations
 *
 **/
size_t 
lg(uint32_t a) {
  size_t bits=0;
  while (a!=0) {
    ++bits;
    a>>=1;
  };
  return bits;
}

int 
addition_overflow(uint32_t a, uint32_t b) {
  size_t a_bits=lg(a), b_bits=lg(b);
  return (a_bits<32 && b_bits<32);
}



/* static int multiplication_overflow(uint32_t a, uint32_t b) { */
/*   size_t a_bits=lg(a), b_bits=lg(b); */
/*   return (a_bits+b_bits<=32); */
/* } */


/* static int exponentiation_overflow(uint32_t a, uint32_t b) { */
/*   size_t a_bits=lg(a); */
/*   return (a_bits*b<=32); */
/* } */
