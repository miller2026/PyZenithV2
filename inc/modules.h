/**
 * @file modules.h
 * @brief Module Configuration and Registry.
 */

#ifndef PROJECT_HUB_MODULES_H
#define PROJECT_HUB_MODULES_H

#include <sys/types.h>
#include "defs.h"

typedef void (*module_entry_fn)(int socket_fd, const char* input_arg);

typedef struct {
    int id;
    const char* name;
    uid_t uid;
    gid_t gid;
    const char* selinux_context;
    module_entry_fn entry_point;
} ModuleConfig;

const ModuleConfig* get_module_config(int module_id);

#endif // PROJECT_HUB_MODULES_H