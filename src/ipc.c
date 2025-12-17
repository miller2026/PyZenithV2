/**
 * @file ipc.c
 * @brief Implementation of robust atomic IPC I/O.
 */

#include "ipc.h"
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>

void ipc_init_response(IpcResponse* resp) {
    if (!resp) return;
    // Security: Zero out entire struct to prevent leak of uninitialized stack data
    memset(resp, 0, sizeof(IpcResponse));
}

void ipc_set_error(IpcResponse* resp, int code, const char* msg) {
    if (!resp) return;
    resp->status_code = code;
    resp->data_len = 0;
    if (msg) {
        strncpy(resp->payload, msg, PAYLOAD_CAP - 1);
        resp->payload[PAYLOAD_CAP - 1] = '\0';
        resp->data_len = (int32_t)strlen(resp->payload);
    }
}

void ipc_set_data(IpcResponse* resp, const char* data) {
    if (!resp) return;
    resp->status_code = 0;
    if (data) {
        strncpy(resp->payload, data, PAYLOAD_CAP - 1);
        resp->payload[PAYLOAD_CAP - 1] = '\0';
        resp->data_len = (int32_t)strlen(resp->payload);
    } else {
        resp->data_len = 0;
    }
}

ProjectStatus ipc_send_packet(int socket_fd, const IpcResponse* resp) {
    if (socket_fd < 0 || !resp) return STATUS_ERR_INVALID_ARG;
    
    // SOCK_DGRAM guarantees atomic message boundaries.
    ssize_t sent = send(socket_fd, resp, sizeof(IpcResponse), 0);
    
    if (sent != sizeof(IpcResponse)) {
        return STATUS_ERR_IPC_SEND;
    }
    return STATUS_SUCCESS;
}

ProjectStatus ipc_receive_packet(int socket_fd, IpcResponse* resp) {
    if (socket_fd < 0 || !resp) return STATUS_ERR_INVALID_ARG;

    ssize_t received = recv(socket_fd, resp, sizeof(IpcResponse), 0);
    
    // Strict Protocol: We do not handle partials/fragmentation.
    if (received != sizeof(IpcResponse)) {
        return STATUS_ERR_IPC_PROTO;
    }
    
    // Security: Force null termination regardless of untrusted input
    if (resp->data_len < 0 || resp->data_len >= PAYLOAD_CAP) {
        resp->data_len = PAYLOAD_CAP - 1;
    }
    resp->payload[resp->data_len] = '\0';
    
    return STATUS_SUCCESS;
}