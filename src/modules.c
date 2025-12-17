/**
 * @file modules.c
 * @brief Entry points for all execution modules.
 */

#include "modules.h"
#include "ipc.h"
#include "symbol_resolver.h"
#include "network_utils.h"
#include "xml_utils.h"
#include "sqlite_utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ============================================================================
// Entry Points
// ============================================================================

static void mod_imei(int fd, const char* arg) {
    (void)arg;
    IpcResponse resp;
    ipc_init_response(&resp);
    char val[256] = {0};
    if (sal_get_property("ro.id.imei", val) > 0) ipc_set_data(&resp, val);
    else ipc_set_error(&resp, 1, "N/A");
    ipc_send_packet(fd, &resp);
}

static void mod_phone(int fd, const char* arg) {
    (void)arg;
    IpcResponse resp;
    ipc_init_response(&resp);
    char val[256];
    if (xml_get_value("/data/local/tmp/prefs.xml", "number", val, sizeof(val)) == 0) {
        ipc_set_data(&resp, val);
    } else {
        ipc_set_error(&resp, 1, "N/A");
    }
    ipc_send_packet(fd, &resp);
}

static void mod_mac(int fd, const char* arg) {
    (void)arg;
    IpcResponse resp;
    ipc_init_response(&resp);
    char buf[128];
    FILE* f = fopen("/sys/class/net/wlan0/address", "r");
    if (f) {
        if (fgets(buf, sizeof(buf), f)) {
            buf[strcspn(buf, "\n")] = 0;
            ipc_set_data(&resp, buf);
        } else ipc_set_error(&resp, 1, "Read Error");
        fclose(f);
    } else ipc_set_error(&resp, 1, "N/A");
    ipc_send_packet(fd, &resp);
}

static void mod_logger(int fd, const char* arg) {
    (void)fd;
    if (arg) network_send_log(arg);
}

static void mod_sender(int fd, const char* arg) {
    IpcResponse resp;
    ipc_init_response(&resp);
    if (arg) {
        char response[128] = {0};
        if (network_send_payload(arg, response, sizeof(response)) == STATUS_SUCCESS) {
            ipc_set_data(&resp, response);
        } else {
            ipc_set_error(&resp, 1, "Network Failure");
        }
    } else ipc_set_error(&resp, 1, "Empty Payload");
    ipc_send_packet(fd, &resp);
}

static void mod_db_cleaner(int fd, const char* arg) {
    (void)arg;
    IpcResponse resp;
    ipc_init_response(&resp);
    
    // Call business logic (uses SAL)
    ProjectStatus status = sqlite_perform_cleanup("/data/data/com.android.phone/databases/test.db");
    
    if (status == STATUS_SUCCESS) ipc_set_data(&resp, "Cleaned");
    else ipc_set_error(&resp, (int)status, "DB Error"); 
    ipc_send_packet(fd, &resp);
}

// ============================================================================
// Registry
// ============================================================================

static const ModuleConfig MODULE_REGISTRY[] = {
    // ID              Name         UID   GID   Context                Entry Point
    { MOD_ID_IMEI,     "IMEI",      1001, 1001, "u:r:isolated_imei:s0", mod_imei },
    { MOD_ID_PHONE,    "Phone",     1002, 1002, "u:r:isolated_app:s0",  mod_phone },
    { MOD_ID_MAC,      "MAC",       1003, 1003, "u:r:isolated_net:s0",  mod_mac },
    { MOD_ID_LOGGER,   "Logger",    1004, 1004, "u:r:isolated_net:s0",  mod_logger },
    { MOD_ID_SENDER,   "Sender",    1004, 1004, "u:r:isolated_net:s0",  mod_sender },
    { MOD_ID_DB_CLEANER, "DBCleaner", 1001, 1001, "u:r:isolated_app:s0", mod_db_cleaner }, 
};

const ModuleConfig* get_module_config(int module_id) {
    size_t count = sizeof(MODULE_REGISTRY) / sizeof(ModuleConfig);
    for (size_t i = 0; i < count; i++) {
        if (MODULE_REGISTRY[i].id == module_id) return &MODULE_REGISTRY[i];
    }
    return NULL;
}