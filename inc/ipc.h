/**
 * @file ipc.h
 * @brief Inter-Process Communication Protocol definitions.
 */

#ifndef PROJECT_HUB_IPC_H
#define PROJECT_HUB_IPC_H

#include <stdint.h>
#include "defs.h"

/** Packet size strictly aligned to 4KB Page size for atomic DGRAM ops */
#define IPC_PACKET_SIZE 4096
#define PAYLOAD_CAP     (IPC_PACKET_SIZE - sizeof(int32_t) - sizeof(int32_t))

/**
 * @brief The IPC Wire Format.
 * Must be aligned to ensure binary compatibility and kernel optimization.
 */
typedef struct __attribute__((aligned(4096))) {
    int32_t status_code;          /**< 0 = Success, Non-Zero = Error Logic */
    int32_t data_len;             /**< Length of valid bytes in payload */
    char    payload[PAYLOAD_CAP]; /**< Data buffer (Null-terminated) */
} IpcResponse;

// --- API ---
void ipc_init_response(IpcResponse* resp);
void ipc_set_error(IpcResponse* resp, int code, const char* msg);
void ipc_set_data(IpcResponse* resp, const char* data);

ProjectStatus ipc_send_packet(int socket_fd, const IpcResponse* resp);
ProjectStatus ipc_receive_packet(int socket_fd, IpcResponse* resp);

#endif // PROJECT_HUB_IPC_H