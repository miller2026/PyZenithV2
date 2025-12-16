#ifndef IPC_H
#define IPC_H

#include <stdint.h>
#include "common.h"

/* Simpler code decision: every IPC message is IPC_PACKET_SIZE bytes. */
#define IPC_PACKET_SIZE     4096
#define PAYLOAD_MAX_SIZE    (IPC_PACKET_SIZE - sizeof(int32_t) - sizeof(int32_t))

 /* Aligned to ensure IPC_PACKET_SIZE is enforced */
__attribute__((aligned(IPC_PACKET_SIZE)))
typedef struct ipc_response_s {
    /* Status of the IPC operation */
    int32_t status_code;

    /* The response payload from the module + its length */
    uint32_t data_len;
    char payload[PAYLOAD_MAX_SIZE];
} ipc_response_t;

/* Exported Methods */
void ipc_set_data(ipc_response_t* resp, const char* data);
void ipc_set_error(ipc_response_t* resp, int code, const char* msg);
ProjectStatus ipc_receive_packet(int socket_fd, ipc_response_t* resp);
ProjectStatus ipc_send_packet(int socket_fd, const ipc_response_t* resp);

#endif // IPC_H