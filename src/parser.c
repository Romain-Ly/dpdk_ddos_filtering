/**
 * \file parser.c
 * \brief ddos parser
 * \author R. Ly
 * 
 */



#include "main.h"
#include "parser.h"
#include "debug.h"


/***********************************************************************/
/* static and global def*/

/* args option ong */
#define PARAM_PROC_ID "proc_id"
#define PARAM_PROC_NB "procs_nb"
#define PARAM_CONF_MASTER "config-master"
#define PARAM_CONF_FDIR "config-fdir"
#define PARAM_HELP "help"
#define PARAM_STATS_FILE "stats_f"
#define PARAM_STATS_TIMER "stats_t"

const char *warning_border = "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";
const char *border = "==============================";
const char *singleborder = "----------------------------";
const char *indentation = "    ";

/************************ parser config file (text) ************************/

static int
parse_configfile(const char *file_path){

  FILE * fp;
  char * line = NULL;
  size_t len = 0;
  ssize_t read;
  struct proc_config_s *proc_config_t;
  struct port_queue_conf_s *port_qconf;

  int proc_config_nb =0;//nb of config process

  
  printf("%s%s\n",border,border);
  printf("Parsing config files : \n");
  printf("config. file = %s\n",file_path);

  memset(proc_config, 0, sizeof(proc_config));
  fp = fopen(file_path, "r");
  
  if (fp == NULL){
    fprintf(stderr,"error cannot open config file\n");
    return -1;
  } 

  int lines_nb;
  char buff[30];

  /* Read the first line of the file and skipped lines_nb */
  if (fgets(buff,29,fp)== NULL){
    fprintf(stderr,"The first line should indicate"
	    "the number of skipped lines\n");
    return -1;
  }
 
  if (sscanf(buff,"%d",&lines_nb) == -1){
    fprintf(stderr,"The first line should indicate"
	    "the number of skipped lines\n");
    return -1;
  }

  printf("%s nb of skipped lines = %d\n",indentation,lines_nb);
  
  if (lines_nb > 200){
    fprintf(stderr,"Too many comment lines\n");
    return -1;
  }

  rewind(fp);
  while ( lines_nb != 0){ 
    if((read = getline(&line, &len, fp)) == -1){
      fprintf(stderr,"Something is missing in the config file\n");
      free(line);
      return -1;
    }
    lines_nb-=1;
  }//endwhile
  
  printf("%s Parsed args :\n",indentation);
  printf("%s %s\n",indentation,singleborder);
  
  /* read configuration file */

  while ((read = getline(&line, &len, fp)) != -1) {

    int config_id = 0;
    uint32_t pRx,pTx = 0;
    uint32_t qRx,qTx = 0;
    char engine[CONFIG_MAX_ENGINE_LEN];
    char args[CONFIG_MAX_ARGS_LEN];

    memset(engine,0,CONFIG_MAX_ENGINE_LEN);
    memset(args,0,CONFIG_MAX_ARGS_LEN);
    
    if (len > (IPC_MSG_SIZE-1)){
      fprintf(stderr,"config line too long");
      return -1;
    }
    
    if(sscanf(line,"%d %d %d %d %d %s %s",
	      &config_id,
	      &pRx, &pTx,
	      &qRx, &qTx,
	      engine,args) < 6){
      fprintf(stderr,"Need at least 6 arguments in config file\n");
      return -1;
    }


    printf("%s %d\t%d\t%d\t%d\t%d\t%s\t%s\n",
	   indentation,
	   config_id,
	   pRx, pTx,
	   qRx, qTx,
	   engine,args);
    
    if (((main_information.port_mask & (1 << (pRx))) == 0)|
	(((main_information.port_mask & (1 << (pTx))) == 0))){
      RTE_LOG(WARNING, BRIDGE, "%s\n",warning_border);
      RTE_LOG(WARNING, BRIDGE, "    port %d deactivated !\n",pRx);
      RTE_LOG(WARNING, BRIDGE, "%s\n",warning_border);
      continue;	  
    }

    proc_config_t = &proc_config[config_id];


    if (proc_config_t->enabled ==0 ){
      
      proc_config_t->enabled = 1;
      
      proc_config_t->port_tx = pTx;
      proc_config_t->port_rx = pRx;
      proc_config_t->queue_tx = qTx;
      proc_config_t->queue_rx = qRx;

      strncpy(proc_config_t->engine,engine,CONFIG_MAX_ENGINE_LEN);
      strncpy(proc_config_t->args,args,CONFIG_MAX_ARGS_LEN);

      /* update nb of queues in port_queue_conf struct */
      port_qconf = &port_queue_conf[pTx];
      port_qconf->queue_tx_nb++;

      port_qconf = &port_queue_conf[pRx];
      port_qconf->queue_rx_nb++;
      
      
    } else {
      fprintf(stderr,"Proc : %d can be defined only once \n",config_id);
      return -1;   
    } 
    proc_config_nb++;
  }//endwhile
  
  main_information.proc_config_nb = proc_config_nb;
  


  fclose(fp);
  if (line){
    free(line);
  }
  printf("%s%s\n",border,border);

  return 0;
}

/**
 * help function
 * Adapted from dpdk
 *
 **/
static void
args_usage(const char *prgname)
{
  printf("\n%s\n",border);
  printf("%s [EAL options] --\n"
	 "  -p PORTMASK: hexadecimal bitmask of ports to configure\n"
	 "\n"
	 "  --config-master FILE : configuration of processsors\n"
	 "  --config-fdir   FILE.yaml : configuration of Flow Directory\n"
	 "\n"
	 "  -T PERIOD: printed statistics will be refreshed each PERIOD seconds"
	 " (0 to disable, 10 default, 86400 maximum)\n"
	 "\n"

	 "  --stats_f FILE : output file for stats\n"
	 "  --stats_t PERIOD : stats X-axis resolution (ms)\n"
	 ,prgname);
  printf("\n%s\n",border);
}






/************************  main parsing functions  ************************/

/**
 * parse portmask from args
 * set of port used by the application
 * portmask = 3 <=> port 1 et 2 = up
 **/
static int
parse_portmask(const char *portmask){
  char *end = NULL;
  unsigned long pm;
  /* parse hexadecimal string */
  /* strtoul(src,dst,base) */
  pm = strtoul(portmask, &end, 16);
  if ((portmask[0] == '\0') || (end == NULL) || (*end != '\0')){
    return -1;
  }
  if (pm == 0){
    return -1;
  }
  return pm;
}


/**
 * Set timer period for statistics printing
 * 
 **/
static int
parse_timer_period(const char *q_arg){
  char *end = NULL;
  int n;
  
  /* parse number string */
  n = strtol(q_arg, &end, 10);
  if ((q_arg[0] == '\0') || (end == NULL) || (*end != '\0'))
    return -1;
  if (n >= MAX_TIMER_PERIOD)
    return -1;

  return n;
}
    

/**
 *  Parse the arguments given in the command line of the application 
 *
 **/
int
parse_args(int argc, char **argv){
  int options, return_value;
  char **argv_options;
  int option_index;
  char *program_name = argv[0];

  /* struct option {
   *  const char * name; // name of argument 
   *  int has_arg;       // 1 if require an argument
   *  int *flag;         // how results are returned
   *  int val;           // is the value to return
   * }
   */
  static struct option options_long[] = {
    /* {PARAM_PROC_NB,1,0,0}, */
    /*    {PARAM_PROC_ID,1,0,0},*/
    {PARAM_CONF_MASTER,1,0,0},
    {PARAM_CONF_FDIR,1,0,0},
    {PARAM_HELP,0,0,0},
    {PARAM_STATS_FILE,1,0,0},
    {PARAM_STATS_TIMER,1,0,0},
    {NULL, 0, 0, 0}
  };


  argv_options = argv;

  while ((options = getopt_long(argc, argv_options, "p:T:", 
				options_long, &option_index)) != EOF) {

    switch (options) {
      
      /**
       * short options 
       **/
      /* portmask */
    case 'p':
      main_information.port_mask = parse_portmask(optarg);
      if ( main_information.port_mask == 0) {
	RTE_LOG(CRIT,PORT,"Parse args : Invalid portmask\n");
	args_usage(program_name);
	return -1;
      }
      break;

      /* timer period */
    case 'T':
      timer_period = parse_timer_period(optarg) * 1000 * TIMER_MILLISECOND;
      if (timer_period < 0) {
	printf("invalid timer period\n");
	args_usage(program_name);
	return -1;
      }
      break;
			
    /*   /\* use floating process *\/ */
    /* case 'f': */
    /*   main_information.float_proc = 1; */
    /*   break; */
      
      /**
       * long options 
       **/
    case 0:
      
      /* PROC */
      if (strncmp(options_long[option_index].name, 
		  PARAM_CONF_MASTER, 14) == 0){
	strncpy(main_information.config_file_name,optarg,MAX_FILE_NAME);
	parse_configfile(optarg);
	
	/* FDIR */
      } else if (strncmp(options_long[option_index].name,
			 PARAM_CONF_FDIR, 12) == 0) {
	
	strncpy(main_information.fdir_file_name,optarg,MAX_FILE_NAME);
	main_information.fdir = 1;
	/* help */
      } else if (strncmp(options_long[option_index].name,
			 PARAM_HELP, 4) == 0) {
	args_usage(program_name);
	exit(0);
	/* stats_f */
      } else if (strncmp(options_long[option_index].name,
			 PARAM_STATS_FILE, 7) == 0) {
	strncpy(timer_file,optarg,MAX_FILE_NAME);
      } else if (strncmp(options_long[option_index].name,
			 PARAM_STATS_TIMER, 7) == 0) {
	timer_stats = parse_timer_period(optarg) * TIMER_MILLISECOND;
	if (timer_stats< 0) {
	  printf("invalid timer statsperiod\n");
	  args_usage(program_name);
	  return -1;
	}
      }

      /* if (strncmp(options_long[option_index].name, PARAM_PROC_NB, 8) == 0){ */
      /* 	main_information.proc_nb = atoi(optarg); */
      /* } else if (strncmp(options_long[option_index].name,PARAM_PROC_ID, 7) == 0){ */
      /* 	main_information.proc_id = atoi(optarg); */
      /* } */
      break;
      
    default:
      args_usage(program_name);
      return -1;
    }
  }

  if (optind >= 0)
    argv[optind-1] = program_name;

  return_value = optind-1;
  optind = 0; /* reset getopt lib */
  return return_value;
}
