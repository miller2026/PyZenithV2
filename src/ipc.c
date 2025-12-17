#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>

#include "ipc.h"

void ipc_set_error(ipc_response_t* resp, int code, const char* msg)
{
    if (resp == NULL) {
        return;
    }
    memset(resp, 0, sizeof(*resp));
    resp->status_code = code;
    if (msg != NULL) {
        strncpy(resp->payload, msg, PAYLOAD_MAX_SIZE - 1);
        resp->payload[PAYLOAD_MAX_SIZE - 1] = '\0';
        resp->data_len = (int32_t)strlen(resp->payload);
    }
}

void ipc_set_data(ipc_response_t* resp, const char* data)
{
    if (resp == NULL) {
        return;
    }
    memset(resp, 0, sizeof(*resp));
    resp->status_code = STATUS_SUCCESS;
    if (data != NULL) {
        strncpy(resp->payload, data, PAYLOAD_MAX_SIZE - 1);
        resp->payload[PAYLOAD_MAX_SIZE - 1] = '\0';
        resp->data_len = (int32_t)strlen(resp->payload);
    }
}

ProjectStatus ipc_send_packet(int socket_fd, const ipc_response_t* resp)
{
    if (socket_fd < 0 || resp == NULL) {
        return STATUS_ERR_INVALID_ARG;
    }

    /* TODO: Maybe add support for partial write? */
    /* Not really supposed to happen thanks to SOCK_DGRAM + strict packet size... */
    if (send(socket_fd, resp, sizeof(ipc_response_t), 0) != sizeof(ipc_response_t)) {
        return STATUS_ERR_IPC_SEND;
    }
    return STATUS_SUCCESS;
}

ProjectStatus ipc_receive_packet(int socket_fd, ipc_response_t* resp)
{
    if (socket_fd < 0 || resp == NULL) {
        return STATUS_ERR_INVALID_ARG;
    }

    /* TODO: Maybe add support for partial read? */
    /* Not really supposed to happen thanks to SOCK_DGRAM + strict packet size... */
    if (recv(socket_fd, resp, sizeof(ipc_response_t), 0) != sizeof(ipc_response_t)) {
        return STATUS_ERR_IPC_PROTO;
    }
    if (resp->data_len < 0) {
        resp->data_len = 0;
    } else if  (resp->data_len >= PAYLOAD_MAX_SIZE) {
        resp->data_len = PAYLOAD_MAX_SIZE - 1;
    }
    resp->payload[resp->data_len] = '\0';

    return STATUS_SUCCESS;
}