#include "main.h"
#include "engine_fast_strstr.h"
#include "debug.h"

#include <glib.h>
#include <rte_memory.h>

/**
 * fast_strstr
 * adapted from https://github.com/RaphaelJ/fast_strstr
 * for multiple pattern search
 * RL
 **/

#define FAST_STR_MAX_NUM 32
#define FAST_STR_MAX_STRING_SIZE 1200 

/*------------------------------------------------------------*/
/**
 * STATIC DEF
 **/
//static const char *warning_border = "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";
//static const char *border = "==============================";
//static const char *singleborder = "----------------------------";
static const char *indentation = "    ";

char** fast_strstr_strings;         /* patterns */
int strings_nb;                   /* number of pattern */

struct strings_nb_s {
  uint8_t key;             /* number of char */
  int value;               /* index in fast_strstr_strings */
};//TODO cache aligned DPDK

struct strings_nb_s *strings_nb_t;

//int * fast_strstr_strings_nb_char;  /* number of char for each pattern */
//int fast_strstr_strings_nb_char_max; /* max number of char */
//unsigned * fast_strstr_hashes;    /* hashes of pattern */


GTree *  fast_strstr_tree;
struct strings_tree_s {
  gint key;            /*hash */
  gint value;          /* index*/
};
struct strings_tree_s *strings_tree_t;

/**
 *    [ ]   [fast_strstr_strings]    [fast_strstr_tree (key:value)]
 *     0     "example.com"            125:0
 *     1     "toto.fr"                85:1
 *     ...
 *     n     pattern                  hash:n 
 **/

/*------------------------------------------------------------*/
/**
 * mystrchr  version from glibc strchr
 * Adapted to work with '\0'
 **/
static inline char 
*mystrchr(char *s, int c, uint32_t len) {
  while (*s != (char) c) {
    len --;
    if (len==0) {
      return NULL;
    }
    s++;
  }
  return (char *)s;
}

/**
 *  strcmp compare version for int (for binary tree)
 **/

static int 
compare(const void *pa,const void *pb){
  if(*(const int *)pa < *(const int *)pb) return -1;
  if(*(const int *)pa > *(const int *)pb) return 1;
  return 0;
}

/* static int  */
/* compare(const void *pa, const void *pb){ */
/*   if(*(int *)pa < *(int *)pb) return -1; */
/*   if(*(int *)pa > *(int *)pb) return 1; */
/*   return 0; */
/* } */

/* static void *xmalloc(unsigned n)  { */
/*   void *p; */
/*   p = malloc(n); */
/*   if(p) return p; */
/*   fprintf(stderr, "insufficient memory\n"); */
/*   exit(1); */
/* } */


static gboolean
TraverseFunc (gpointer key, gpointer value){
  
  fprintf(stdout, "get %d:%d\n",*(int *)(gint*)key, *(int *)(gint *)value );
  return FALSE;

}


/**
 *
 * functions for parsing hex
 **/

static inline int 
getVal(char c){

  int rtVal = 0;
  if(c >= '0' && c <= '9') {
    rtVal = c - '0';
  } else {
    rtVal = c - 'a' + 10;
  }
  
  return rtVal;
} 



static inline int
fast_strstr_hash(char *s, int n){
  unsigned retval =0;
  while (n!=0){
    retval += *s;
    s++;
    n--;
  }
  return retval;
}

/**
 * cmpfunc for quicksort
 **/
static int
cmpfunc (const void * a, const void * b){ 
  return ( ((const struct strings_nb_s*)a)->key - ((const struct strings_nb_s *)b)->key ); 
}


//TODO Used ?
static inline int
overflow_guard(const void *len){
  if (*(const int *)len ==0){
    return 1;
  }
}  



/*----------------------------------------------------------------*/
/**
 * Version of fast_strstr for dpdk
 * Modified to work with dpdk
 * search only one string  = fast_strstr_strings[0] 
 **/

int
fast_strstr_filter_one(char* haystack,uint32_t pkt_len){
  const char *needle = fast_strstr_strings[0];
  const char needle_first  = *needle;

  // Runs strchr() on the first section of the haystack as it has a lower
  // algorithmic complexity for discarding the first non-matching characters.
  haystack = mystrchr(haystack, needle_first,pkt_len);
  if (haystack ==0 ) // First character of needle is not in the haystack.
    return 0;
  
  // First characters of haystack and needle are the same now. Both are
  // guaranteed to be at least one character long.
  // Now computes the sum of the first needle_len characters of haystack
  // minus the sum of characters values of needle.

  const char   *i_haystack    = haystack + 1
    ,   *i_needle      = needle   + 1;
  
  unsigned int  sums_diff     = *haystack;
  unsigned identical = 1;
  
  while (*i_haystack && *i_needle) {
    sums_diff += *i_haystack;
    sums_diff -= *i_needle;
    identical &= *i_haystack++ == *i_needle++;
  }
  
  // i_haystack now references the (needle_len + 1)-th character.
  
  if (*i_needle){ // haystack is smaller than needle.
    return 0;
  } else if (identical){
    //return (char *) haystack;
    return 1;
  }

    size_t        needle_len    = i_needle - needle;
    size_t        needle_len_1  = needle_len - 1;
    
    // Loops for the remaining of the haystack, updating the sum iteratively.
    const char   *sub_start;
    for (sub_start = haystack; *i_haystack; i_haystack++) {
      sums_diff -= *sub_start++;
      sums_diff += *i_haystack;
      
      // Since the sum of the characters is already known to be equal at that
      // point, it is enough to check just needle_len-1 characters for
      // equality.
      if (
	  sums_diff == 0
	  && needle_first == *sub_start // Avoids some calls to memcmp.
	  && memcmp(sub_start, needle, needle_len_1) == 0
	  )
	//return (char *) sub_start;
	return 1;
    }
    
    return 0;
    
}

/**
 * Version of fast_strstr for dpdk
 * Modified to work with dpdk
 * search only one string = fast_strstr_strings[0]
 * with pre-calculated needles
 **/
int
fast_strstr_filter_one_hashed(char* haystack,uint32_t pkt_len){
  const char *needle = fast_strstr_strings[4];
  const char needle_first  = *needle;

  haystack = mystrchr(haystack, needle_first,pkt_len);
  if (haystack ==0 ) // First character of needle is not in the haystack.
    return 0;
  
  const char   *i_haystack    = haystack + 1
    ,   *i_needle      = needle   + 1;
  
  unsigned int  sums_diff     = *haystack;
  unsigned identical = 1;
  
  while (*i_haystack && *i_needle) {
    sums_diff += *i_haystack;
    //sums_diff -= *i_needle;
    identical &= *i_haystack++ == *i_needle++;
  }
  //sums_diff -= fast_strstr_hashes[0];
  /* gint intkey[1]; */
  /* intkey[0]=0; */
  /* int tmp=*(int*)(gint*) g_tree_lookup(fast_strstr_tree, intkey); */
  /* sums_diff -= tmp; */

  /* int c; */
  /* c = 1325; */
  
  /* int tmp = *(int*)(gint*)g_tree_lookup(fast_strstr_tree,&c); */
  /* fprintf(stdout, "check value %d:%d\n", strings,tmp ); */
  /* int intkey; */
  /* intkey = 0; */

  // printf("value : %d\n",strings_tree_t[0].key);
  sums_diff -= strings_tree_t[0].key;
  // i_haystack now references the (needle_len + 1)-th character.
  if (*i_needle){ // haystack is smaller than needle.
    return 0;
  } else if (identical){
    //return (char *) haystack;
    return 1;
  }
  
  size_t        needle_len    = i_needle - needle;
  size_t        needle_len_1  = needle_len - 1;
  
  // Loops for the remaining of the haystack, updating the sum iteratively.
  const char   *sub_start;
  for (sub_start = haystack; *i_haystack; i_haystack++) {
    sums_diff -= *sub_start++;
    sums_diff += *i_haystack;
    
    // Since the sum of the characters is already known to be equal at that
    // point, it is enough to check just needle_len-1 characters for
    // equality.
    if (
	sums_diff == 0
	&& needle_first == *sub_start // Avoids some calls to memcmp.
	&& memcmp(sub_start, needle, needle_len_1) == 0
	)
      //return (char *) sub_start;
      return 1;
    }
  
  return 0;
}


/**
 * Version of fast_strstr for multiple pattern
 * Modified to work with dpdk
 * search only one string = fast_strstr_strings[0]
 * with pre-calculated 
 **/
/*  1 function RabinKarpSet(string s[1..n], set of string subs, m): */
/*  2     set hsubs := emptySet */
/*  3     foreach sub in subs */
/*  4         insert hash(sub[1..m]) into hsubs */

/*  5     hs := hash(s[1..m]) */
/*  6     for i from 1 to n-m+1 */
/*  7         if hs ∈ hsubs and s[i..i+m-1] ∈ subs */
/*  8             return i */
/*  9         hs := hash(s[i+1..i+m]) */
/* 10     return not found */


/*int */
/* fast_strstr_filter_MP_hashed(char* haystack,uint32_t pkt_len){ */
  



/*   int i; */
/*   unsigned sum = 0; */

/*   const char *i_haystack= haystack; */
/*   i=1;     */
  
/*   int i_string=0; */
/*   while (i < strings_nb_t[strings_nb].key){ /\* while until max nb of char *\/ */
/*     sum += *i_haystack; */
/*     *i_haystack++; */
/*     i++; */
/*     if ( i == strings_nb_t[i_string].key){ //TODO unlikely */
      
/*     } */
/*   } */



/*   uint8_t i =0; */
/*   uint8_t j= 0; */
/*   unsigned result; */
/*   unsigned int  sums_strings[strings_nb]; */
/*   unsigned  sum; */

/*   const char *sub_start; */
/*   const char *i_haystack[strings_nb]  = haystack + 1; */
  
  
/*   /\* initialize hash *\/ */
/*   sums = *haystack; */
/*   const char * i_hay; */

/*   int i; */
  


/*   for (j = 0 ; j < fast_strstr_strings_nb_char_max ; j++){ */
/*     sums += *i_hay; */
/*     i_hay ++; */
/*     if ( */
/*   } */

/*   const char *i_haystack = haystack + 1; */

/*   for (j = 0 ; j < strings_nb ; j++){ */
    
/*     sums_diff[j] += *i_haystack; */
/*     *i_haystack++; */
    
/*     if (fast_strstr_strings_nb_char[j]>pkt_len){ //TODO unlikely */
/*       continue; */
/*     } */

/*     for (i = 0; i = fast_strstr_strings_nb_char[j] ;i++){ */
    

/*     } */
/*   }//endfor */
  
/*   /\* now we have all hashes for all strings *\/ */
/*   for ( */

/*   unsigned identical = 1; */
  
/*   while (*i_haystack && *i_needle) { */
/*     sums_diff += *i_haystack; */
/*     //sums_diff -= *i_needle; */
/*     identical &= *i_haystack++ == *i_needle++; */
/*   } */
/*   sums_diff -= fast_strstr_hashes[0]; */

/*   // i_haystack now references the (needle_len + 1)-th character. */
/*   if (*i_needle){ // haystack is smaller than needle. */
/*     return 0; */
/*   } else if (identical){ */
/*     //return (char *) haystack; */
/*     return 1; */
/*   } */
  
/*   size_t        needle_len    = i_needle - needle; */
/*   size_t        needle_len_1  = needle_len - 1; */
  
/*   // Loops for the remaining of the haystack, updating the sum iteratively. */
/*   const char   *sub_start; */
/*   for (sub_start = haystack; *i_haystack; i_haystack++) { */
/*     sums_diff -= *sub_start++; */
/*     sums_diff += *i_haystack; */
    
/*     // Since the sum of the characters is already known to be equal at that */
/*     // point, it is enough to check just needle_len-1 characters for */
/*     // equality. */
/*     if ( */
/* 	sums_diff == 0 */
/* 	&& needle_first == *sub_start // Avoids some calls to memcmp. */
/* 	&& memcmp(sub_start, needle, needle_len_1) == 0 */
/* 	) */
/*       //return (char *) sub_start; */
/*       return 1; */
/*     } */
  
/*   return 0; */
/* } */


/**
   *
   * Compilation of patterns
   *
   **/



static int 
fast_str_compile(char *file_path){

  FILE * fp;
  // ssize_t read;
  int lines_nb; /* number of strings */
  char buff[FAST_STR_MAX_NUM]; 
  char buff_tmp[FAST_STR_MAX_STRING_SIZE];
  size_t maximal = 0;
  memset(buff_tmp,0,FAST_STR_MAX_STRING_SIZE);


  fp = fopen(file_path, "r");
  
  if (fp == NULL){
    fprintf(stderr,"error cannot open config file (%s)\n",
	    file_path);
    return -1;
  } 
  


  /* Read the first line of the file  */
  if (fgets(buff,FAST_STR_MAX_NUM,fp)== NULL){
    fprintf(stderr,"The first line should indicate"
	    "the number of strings\n");
    return -1;
  }
 
  if (sscanf(buff,"%d",&lines_nb) == -1){
    fprintf(stderr,"The first line should indicate"
	    "the number of strings\n");
    return -1;
  }



  printf("%s nb of strings = %d\n",indentation,lines_nb);
  strings_nb=lines_nb;

  fast_strstr_strings = rte_malloc("fast_strstr_strings",
				   lines_nb * sizeof(*fast_strstr_strings),0);
  strings_nb_t = rte_malloc("strings_nb_t",
			    strings_nb*sizeof(struct strings_nb_s),0);

  // fast_strstr_strings_nb_char = malloc(strings_nb);
  //fast_strstr_hashes = malloc(lines_nb * sizeof(unsigned));
  //memset(fast_strstr_hashes,0,lines_nb * sizeof(unsigned));
  
  //strings_tree_t = malloc(sizeof(struct strings_tree_s)*strings_nb);
  strings_tree_t = rte_malloc("strings_tree_t",
			      sizeof(struct strings_tree_s)*strings_nb,0);
  fast_strstr_tree = g_tree_new_full((GCompareDataFunc)compare,
				     NULL,
				     (GDestroyNotify)free,
				     (GDestroyNotify)free);
  
  if (fast_strstr_strings == NULL){
    fprintf(stderr,"ERROR : malloc fast_strstr_strings");
    exit(1);
  }

  /* if (fast_strstr_hashes == NULL){ */
  /*   fprintf(stderr,"ERROR : malloc fast_strstr_hashes"); */
  /*   exit(1); */
  /* } */

  /**
   * each element is delimited by a "\n"
   **/
  int i =0;
  char tmp[FAST_STR_MAX_STRING_SIZE];
  
  while (i < lines_nb){
    size_t len = 0;
    memset(tmp,0,FAST_STR_MAX_STRING_SIZE);
    int hash=0;
    char c,d;
    while((c = fgetc(fp)) != '\n'){  
      
      if ((d = fgetc(fp)) == '\n'){
	fprintf(stderr,"ERROR : 2 digits hex\n");
	return -1;
      }
      int val = getVal(c) * 16 + getVal(d);
      if(val<31 || val > 127){
	printf("\\x%.2x",val);
      } else {
	printf("%c", val);
      }
      tmp[len] = val;
      len ++;
      if (len > (FAST_STR_MAX_STRING_SIZE)){
	fprintf(stderr,"ERROR: string (%s) too long\n",tmp);
	return -1;
      }
      //fast_strstr_hashes[i] += val;
      hash +=val;

      /* prevent buffer overflow*/
      if (addition_overflow(hash,val)==0){
	fprintf(stderr,"ERROR: string (%s : %d) too long\n",tmp,hash);
	return -1;
      }

    }
    
    
    /* key = hash value */
    strings_tree_t[i].key = hash;
    strings_tree_t[i].value = i;
    /* gint* intkey = (gint*)malloc(sizeof(gint)); */
    /* *intkey = hash; */
    
    /* /\* value = *\/ */
    /* gint* intval = (gint*)malloc(sizeof(gint)); */
    /* *intval = i; */

    /* add value to binary tree*/
    fprintf(stdout, "\nadd %d:%d\n",(int)strings_tree_t[i].key,
	    (int )strings_tree_t[i].value);
    
    g_tree_insert(fast_strstr_tree, &strings_tree_t[i].key, &strings_tree_t[i].value);
    
    tmp[len]= '\0';
    len++;

    strings_nb_t[i].key=len-1;
    strings_nb_t[i].value=i;
    //    fast_strstr_strings_nb_char[i] = len-1;
    
    //  fast_strstr_strings[i] =malloc("fast_strstr_strings",len*sizeof(char),0);
    fast_strstr_strings[i] = rte_malloc("fast_strstr_strings sub",
					len*sizeof(char),0);
    strncpy(fast_strstr_strings[i],tmp,len);
    fast_strstr_strings[i][len]='\0';

    if (len < maximal){ 
      maximal = len; 
    } 
    i++;
    printf("\n");
  } //endwhile

  gint nodes_nb = g_tree_nnodes(fast_strstr_tree);
  printf("number of nodes : %d\n",nodes_nb);

  qsort(strings_nb_t, strings_nb, sizeof(struct strings_nb_s), cmpfunc); 
 
  

  fclose(fp);

  return 0;
}
   
int 
fast_str_end(void __attribute__((unused)) *tmp){ 
  
  int i;
  for (i = 0; i < strings_nb; i++ ){
    free(fast_strstr_strings[i]); 
  }
  free(fast_strstr_strings);
  free(strings_nb_t);
  g_tree_destroy (fast_strstr_tree);
  
  //  free(hash_strings);
  return 0;
} 
     


/**
 * init
 **/
int 
fast_str_init(char *file_path){
  printf("%s",indentation);
  printf("Fast_strstr Engine\n");

  if (fast_str_compile(file_path) !=0){
    fprintf(stderr,"ERROR fast_strstr compilation\n");
    exit(1);
  }



  g_tree_foreach(fast_strstr_tree,(GTraverseFunc)TraverseFunc,NULL); 


  /*debug*/
  /* for (i=1;i<strings_nb;i++){ */
  /*   int c; */
  /*   c = 1325; */
    
  /*   int tmp = *(int*)(gint*)g_tree_lookup(fast_strstr_tree,&c); */
  /*   fprintf(stdout, "check value %d:%d\n", i,tmp ); */

  /* } */


  
  /* /\**  */
  /*  * debug quicksort */
  /*  **\/ */
  /* int n; */
  /* for( n = 0 ; n < strings_nb; n++ ){ */
  /*   printf("key/val :%d / %d \n", strings_nb_t[n].key,strings_nb_t[n].value); */
  /* } */
  
  /* fast_str_end(NULL); //TODO */
  /* exit(0); */
  return 0;
}

