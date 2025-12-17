
#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <sys/types.h>

#define TIMEOUT_IPC_MS      2000  /* Timeout: waiting for module response */
#define TIMEOUT_EXIT_MS     1000  /* Timeout: waiting for process exit */
#define POLL_INTERVAL_MS    50    /* Timeout: waitpid interval */

typedef enum module_id_s {
    MOD_ID_IMEI = 0,
    MOD_ID_PHONE,
    MOD_ID_MAC,
    MOD_ID_LOGGER,
    MOD_ID_SENDER,
    MOD_ID_DB_CLEANER,
    MODULE_COUNT
} module_id_e;

typedef enum {
    STATUS_SUCCESS = 0,
    STATUS_ERR_GENERIC,
    STATUS_ERR_INVALID_ARG,
    STATUS_ERR_FORK,
    STATUS_ERR_SOCKET,
    STATUS_ERR_TIMEOUT,
    STATUS_ERR_POLL,
    STATUS_ERR_ZOMBIE,
    STATUS_ERR_IPC_SEND,
    STATUS_ERR_IPC_RECV,
    STATUS_ERR_IPC_PROTO,
    STATUS_ERR_NET_FAIL,
    STATUS_ERR_MODULE_FAIL,
    STATUS_ERR_DB_LOAD,
    STATUS_ERR_DB_EXEC,
    STATUS_ERR_GET_PROP,
    STATUS_ERR_NETWORK_FAILURE,
    STATUS_ERR_OPEN_ERROR,
    STATUS_ERR_READ_ERROR,
    STATUS_ERR_XML_PARSER
} ProjectStatus;

typedef struct daemon_context_s {
    char imei[256];
    char phone[256];
    char mac[256];
    int has_imei;
    int has_phone;
    int has_mac;
    int db_cleaned;
} daemon_context_t;

#define DEBUG(...)
#define INFO(...)
#define ERROR(...)

#endif // COMMON_H