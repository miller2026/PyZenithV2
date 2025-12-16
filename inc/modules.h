#ifndef MODULES_H
#define MODULES_H

#include <sys/types.h>
#include "common.h"

/* Entrypoint function signature for each module */
typedef void (*module_entry_fn)(int socket_fd, const char* input_arg);

typedef struct module_config_s {
    int id;
    const char* name;

    /* We must change uid,gid,selinux to specified values for the code to work */
    uid_t uid;
    gid_t gid;
    const char* selinux_context;

    module_entry_fn entry_point;
} module_config_t;

const module_config_t* get_module_config(int module_id);

#endif // MODULES_H