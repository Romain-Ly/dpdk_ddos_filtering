
#include "fdir_yaml_parser.h"


int fdir_main(char * file_name);
int fdir_callback_add_rules(struct fdir_parsing* filter_tmp);
int fdir_init(uint8_t port_id);

/* old stuff */
/**
int fdir_add_rules(uint8_t port_id);
int fdir_set_masks(__attribute__((unused)) uint8_t port_id);
**/
