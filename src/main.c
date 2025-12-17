/**
 * @file main.c
 * @brief Project Hub Daemon - Event Driven Orchestrator.
 * Optimized to reuse system resources (epoll/signalfd) across stages.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/prctl.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <signal.h>
#include <errno.h>

#include "defs.h"
#include "ipc.h"
#include "symbol_resolver.h"
#include "modules.h"

#define MAX_EVENTS 4

// --- Persistent System Resources ---
typedef struct {
    int epoll_fd;
    int signal_fd;
} SystemResources;

// --- Stage Execution Context ---
typedef struct {
    // Inputs
    const ModuleConfig* config;
    const char* arg;
    char* out_buf;
    size_t out_buf_size;
    SystemResources* sys; // Pointer to shared resources

    // Runtime
    pid_t pid;
    int ipc_fd;
    
    // State
    int process_exited;
    ProjectStatus final_status;
} StageContext;

// --- Global Context ---
typedef struct {
    char imei[256];
    char phone[256];
    char mac[256];
    int has_imei;
    int has_phone;
    int has_mac;
    int db_cleaned;
} GlobalContext;

// ============================================================================
// Resource Setup (Run Once)
// ============================================================================

static ProjectStatus setup_resources(SystemResources* sys) {
    // 1. Create SignalFD for SIGCHLD
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    
    sys->signal_fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (sys->signal_fd < 0) {
        sal_log_error("signalfd failed");
        return STATUS_ERR_GENERIC;
    }

    // 2. Create Epoll Instance
    sys->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (sys->epoll_fd < 0) {
        sal_log_error("epoll_create1 failed");
        close(sys->signal_fd);
        return STATUS_ERR_EPOLL;
    }

    // 3. Register SignalFD with Epoll
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = sys->signal_fd;

    if (epoll_ctl(sys->epoll_fd, EPOLL_CTL_ADD, sys->signal_fd, &ev) < 0) {
        sal_log_error("epoll_ctl (Signal) failed");
        close(sys->epoll_fd);
        close(sys->signal_fd);
        return STATUS_ERR_EPOLL;
    }

    return STATUS_SUCCESS;
}

static void cleanup_resources(SystemResources* sys) {
    if (sys->epoll_fd >= 0) close(sys->epoll_fd);
    if (sys->signal_fd >= 0) close(sys->signal_fd);
}

// ============================================================================
// Process Spawning
// ============================================================================

static pid_t spawn_module_process(const ModuleConfig* config, int* parent_fd, const char* arg) {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, sv) < 0) return -1;

    pid_t pid = fork();
    if (pid < 0) {
        close(sv[0]); close(sv[1]);
        return -1;
    }

    if (pid == 0) {
        // --- Child Process ---
        close(sv[0]);
        int child_fd = sv[1];

        // Restore default signal handling
        sigset_t mask;
        sigemptyset(&mask);
        sigprocmask(SIG_SETMASK, &mask, NULL);

        prctl(PR_SET_PDEATHSIG, SIGKILL);

        sal_set_selinux_context(config->selinux_context);
        if (setresgid(config->gid, config->gid, config->gid) < 0) _exit(EXIT_FAILURE);
        if (setresuid(config->uid, config->uid, config->uid) < 0) _exit(EXIT_FAILURE);

        if (config->entry_point) config->entry_point(child_fd, arg);
        _exit(EXIT_SUCCESS);
    }

    // --- Parent Process ---
    close(sv[1]);
    *parent_fd = sv[0];
    return pid;
}

// ============================================================================
// Event Loop Logic
// ============================================================================

static void handle_ipc_event(StageContext* ctx) {
    IpcResponse resp;
    ipc_init_response(&resp);
    
    ProjectStatus res = ipc_receive_packet(ctx->ipc_fd, &resp);
    
    if (res == STATUS_SUCCESS && resp.status_code == 0) {
        if (ctx->out_buf && ctx->out_buf_size > 0 && resp.data_len > 0) {
            strncpy(ctx->out_buf, resp.payload, ctx->out_buf_size - 1);
            ctx->out_buf[ctx->out_buf_size - 1] = '\0';
        }
        ctx->final_status = STATUS_SUCCESS;
    } else {
        sal_log_error("Module %s Error: %d", ctx->config->name, res);
        ctx->final_status = (res != STATUS_SUCCESS) ? res : STATUS_ERR_MODULE_FAIL;
    }

    // Stop listening to IPC (Oneshot). 
    // Not strictly necessary as we close fd soon, but good practice.
    epoll_ctl(ctx->sys->epoll_fd, EPOLL_CTL_DEL, ctx->ipc_fd, NULL);
}

static void handle_signal_event(StageContext* ctx) {
    struct signalfd_siginfo fdsi;
    ssize_t s = read(ctx->sys->signal_fd, &fdsi, sizeof(struct signalfd_siginfo));
    
    if (s == sizeof(struct signalfd_siginfo)) {
        if ((pid_t)fdsi.ssi_pid == ctx->pid) {
            waitpid(ctx->pid, NULL, 0); // Reap our child
            ctx->process_exited = 1;
        } else {
            // Reap foreign zombies (from previous failed stages)
            waitpid((pid_t)fdsi.ssi_pid, NULL, WNOHANG);
        }
    }
}

static void run_event_loop(StageContext* ctx) {
    struct epoll_event events[MAX_EVENTS];
    int timeout_ms = TIMEOUT_IPC_MS + TIMEOUT_EXIT_MS;

    while (!ctx->process_exited) {
        int n = epoll_wait(ctx->sys->epoll_fd, events, MAX_EVENTS, timeout_ms);

        if (n < 0) {
            if (errno == EINTR) continue;
            ctx->final_status = STATUS_ERR_POLL;
            sal_log_error("epoll_wait failed");
            break;
        }

        if (n == 0) {
            ctx->final_status = STATUS_ERR_TIMEOUT;
            sal_log_error("Module %s Timeout", ctx->config->name);
            break;
        }

        for (int i = 0; i < n; i++) {
            if (events[i].data.fd == ctx->ipc_fd) {
                handle_ipc_event(ctx);
            }
            else if (events[i].data.fd == ctx->sys->signal_fd) {
                handle_signal_event(ctx);
            }
        }
    }
}

static void ensure_process_cleanup(StageContext* ctx) {
    if (!ctx->process_exited) {
        sal_log_error("Force killing PID %d", ctx->pid);
        kill(ctx->pid, SIGKILL);
        waitpid(ctx->pid, NULL, 0); // Blocking reap
        
        if (ctx->final_status == STATUS_SUCCESS) {
            ctx->final_status = STATUS_ERR_ZOMBIE;
        }
    }
}

// ============================================================================
// Orchestrator
// ============================================================================

static ProjectStatus execute_stage(SystemResources* sys, int mod_id, const char* arg, char* out_buf, size_t size) {
    const ModuleConfig* config = get_module_config(mod_id);
    if (!config) return STATUS_ERR_INVALID_ARG;

    // 1. Init Context
    StageContext ctx = {
        .config = config, .arg = arg, .out_buf = out_buf, .out_buf_size = size,
        .sys = sys, .pid = -1, .ipc_fd = -1,
        .process_exited = 0, .final_status = STATUS_ERR_GENERIC
    };

    // 2. Spawn
    ctx.pid = spawn_module_process(config, &ctx.ipc_fd, arg);
    if (ctx.pid < 0) return STATUS_ERR_FORK;

    // 3. Register IPC Socket with Shared Epoll
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = ctx.ipc_fd;

    if (epoll_ctl(sys->epoll_fd, EPOLL_CTL_ADD, ctx.ipc_fd, &ev) == 0) {
        // 4. Run Loop
        run_event_loop(&ctx);
    } else {
        ctx.final_status = STATUS_ERR_EPOLL;
        sal_log_error("epoll_ctl (IPC) failed");
    }

    // 5. Cleanup IPC (Closing FD automatically removes from Epoll)
    close(ctx.ipc_fd);
    
    // 6. Final Reap check
    ensure_process_cleanup(&ctx);

    return ctx.final_status;
}

// ============================================================================
// Main
// ============================================================================

int main() {
    signal(SIGPIPE, SIG_IGN);
    
    // Block SIGCHLD globally for signalfd
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) return EXIT_FAILURE;

    sal_init();
    sal_log_info("Orchestrator V12 (Shared Resources) Started.");

    // Setup Shared Resources (Epoll & SignalFD)
    SystemResources sys;
    if (setup_resources(&sys) != STATUS_SUCCESS) {
        sal_log_error("Failed to setup system resources");
        return EXIT_FAILURE;
    }

    GlobalContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    char log_buf[512];

    // --- IMEI ---
    if (execute_stage(&sys, MOD_ID_IMEI, NULL, ctx.imei, sizeof(ctx.imei)) == STATUS_SUCCESS) {
        ctx.has_imei = 1;
        snprintf(log_buf, sizeof(log_buf), "IMEI: Success (%s)", ctx.imei);
    } else snprintf(log_buf, sizeof(log_buf), "IMEI: Failed");
    execute_stage(&sys, MOD_ID_LOGGER, log_buf, NULL, 0);

    // --- Phone ---
    if (execute_stage(&sys, MOD_ID_PHONE, NULL, ctx.phone, sizeof(ctx.phone)) == STATUS_SUCCESS) {
        ctx.has_phone = 1;
        snprintf(log_buf, sizeof(log_buf), "Phone: Success");
    } else snprintf(log_buf, sizeof(log_buf), "Phone: Failed");
    execute_stage(&sys, MOD_ID_LOGGER, log_buf, NULL, 0);

    // --- MAC ---
    if (execute_stage(&sys, MOD_ID_MAC, NULL, ctx.mac, sizeof(ctx.mac)) == STATUS_SUCCESS) {
        ctx.has_mac = 1;
        snprintf(log_buf, sizeof(log_buf), "MAC: Success");
    } else snprintf(log_buf, sizeof(log_buf), "MAC: Failed");
    execute_stage(&sys, MOD_ID_LOGGER, log_buf, NULL, 0);

    // --- DB Cleaner ---
    sal_log_info("Cleaning DB...");
    char db_resp[128];
    if (execute_stage(&sys, MOD_ID_DB_CLEANER, NULL, db_resp, sizeof(db_resp)) == STATUS_SUCCESS) {
        ctx.db_cleaned = 1;
        snprintf(log_buf, sizeof(log_buf), "DB: Cleaned");
    } else snprintf(log_buf, sizeof(log_buf), "DB: Failed");
    execute_stage(&sys, MOD_ID_LOGGER, log_buf, NULL, 0);

    // --- Upload ---
    sal_log_info("Uploading Payload...");
    char payload[4096];
    snprintf(payload, sizeof(payload), "IMEI:%s|PHONE:%s|MAC:%s|DB:%d",
        ctx.has_imei ? ctx.imei : "N/A",
        ctx.has_phone ? ctx.phone : "N/A",
        ctx.has_mac ? ctx.mac : "N/A",
        ctx.db_cleaned
    );

    char response[256] = {0};
    if (execute_stage(&sys, MOD_ID_SENDER, payload, response, sizeof(response)) == STATUS_SUCCESS) {
        sal_log_info("Upload Done. Server: %s", response);
    } else {
        sal_log_error("CRITICAL: Upload Failed.");
        cleanup_resources(&sys);
        sal_cleanup();
        return EXIT_FAILURE;
    }

    cleanup_resources(&sys);
    sal_cleanup();
    return EXIT_SUCCESS;
}