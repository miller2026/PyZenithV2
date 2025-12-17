/**
 * @file defs.h
 * @brief Global definitions, constants, and status codes.
 */

#ifndef PROJECT_HUB_DEFS_H
#define PROJECT_HUB_DEFS_H

#include <stdint.h>
#include <sys/types.h>

// --- Timing Configuration (ms) ---
#define TIMEOUT_IPC_MS      2000  /**< Max time to wait for module response on socket */
#define TIMEOUT_EXIT_MS     1000  /**< Max time to wait for process to exit naturally */
#define TIMEOUT_KILL_MS     500   /**< Max time to wait for process to die after SIGKILL */
#define POLL_INTERVAL_MS    50    /**< Sleep interval while polling waitpid */

// --- Module Identifiers ---
#define MOD_ID_IMEI         0
#define MOD_ID_PHONE        1
#define MOD_ID_MAC          2
#define MOD_ID_LOGGER       3
#define MOD_ID_SENDER       4
#define MOD_ID_DB_CLEANER   5

// --- Unified Status Codes ---
/**
 * @enum ProjectStatus
 * @brief Standardized return codes for all internal functions.
 */
typedef enum {
    // Success
    STATUS_SUCCESS          = 0,

    // Generic Errors
    STATUS_ERR_GENERIC      = -1,
    STATUS_ERR_INVALID_ARG  = -2,

    // System / OS Errors
    STATUS_ERR_FORK         = -10,
    STATUS_ERR_SOCKET       = -11,
    STATUS_ERR_TIMEOUT      = -12,
    STATUS_ERR_POLL         = -13, // Generic Poll/Select error
    STATUS_ERR_ZOMBIE       = -14,
    STATUS_ERR_EPOLL        = -15, // New: Specific Epoll setup/ctl error

    // IPC & Network Errors
    STATUS_ERR_IPC_SEND     = -20,
    STATUS_ERR_IPC_RECV     = -21,
    STATUS_ERR_IPC_PROTO    = -22,
    STATUS_ERR_NET_FAIL     = -23,
    
    // Logic Errors
    STATUS_ERR_MODULE_FAIL  = -30,
    STATUS_ERR_DB_LOAD      = -40,
    STATUS_ERR_DB_EXEC      = -41
} ProjectStatus;

#endif // PROJECT_HUB_DEFS_H