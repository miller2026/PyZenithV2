/**
 * @file symbol_resolver.c
 * @brief Implementation of the SAL.
 */

#include "symbol_resolver.h"
#include <dlfcn.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>

#define SQLITE_OPEN_READWRITE 0x00000002

typedef struct {
    void* h_log;
    void* h_c;
    void* h_sel;
    void* h_sql;

    // Function Pointers
    pfn_android_log_print   log;
    pfn_system_property_get prop;
    pfn_setcon              setcon;
    pfn_sqlite3_open_v2     sql_open;
    pfn_sqlite3_exec        sql_exec;
    pfn_sqlite3_close       sql_close;
    pfn_sqlite3_free        sql_free;
    
    int init;
} SalContext;

static SalContext g_ctx = {0};

int sal_init(void) {
    if (g_ctx.init) return 0;
    
    // 1. Logging
    g_ctx.h_log = dlopen("liblog.so", RTLD_LAZY);
    if (g_ctx.h_log) g_ctx.log = (pfn_android_log_print)dlsym(g_ctx.h_log, "__android_log_print");

    // 2. Libc
    g_ctx.h_c = dlopen("libc.so", RTLD_LAZY);
    if (g_ctx.h_c) g_ctx.prop = (pfn_system_property_get)dlsym(g_ctx.h_c, "__system_property_get");

    // 3. SELinux
    g_ctx.h_sel = dlopen("libselinux.so", RTLD_LAZY);
    if (g_ctx.h_sel) g_ctx.setcon = (pfn_setcon)dlsym(g_ctx.h_sel, "setcon");

    // 4. SQLite
    g_ctx.h_sql = dlopen("libsqlite.so", RTLD_LAZY);
    if (!g_ctx.h_sql) g_ctx.h_sql = dlopen("libsqlite3.so", RTLD_LAZY); // Fallback
    
    if (g_ctx.h_sql) {
        g_ctx.sql_open  = (pfn_sqlite3_open_v2)dlsym(g_ctx.h_sql, "sqlite3_open_v2");
        g_ctx.sql_exec  = (pfn_sqlite3_exec)dlsym(g_ctx.h_sql, "sqlite3_exec");
        g_ctx.sql_close = (pfn_sqlite3_close)dlsym(g_ctx.h_sql, "sqlite3_close");
        g_ctx.sql_free  = (pfn_sqlite3_free)dlsym(g_ctx.h_sql, "sqlite3_free");
    }

    g_ctx.init = 1;
    return 0;
}

void sal_cleanup(void) {
    // Note: System libraries are NOT dlclose()d to prevent crashes
    // involving atexit handlers in unmapped memory.
    g_ctx.init = 0;
}

// --- System Wrappers ---

void sal_log_info(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (g_ctx.log) g_ctx.log(4, "ProjectHub", "%s", buf);
    else printf("[INFO] %s\n", buf);
}

void sal_log_error(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (g_ctx.log) g_ctx.log(6, "ProjectHub", "%s", buf);
    else fprintf(stderr, "[ERROR] %s\n", buf);
}

int sal_get_property(const char* key, char* value) {
    return (g_ctx.prop) ? g_ctx.prop(key, value) : 0;
}

int sal_set_selinux_context(const char* context) {
    return (g_ctx.setcon) ? g_ctx.setcon(context) : -1;
}

// --- SQLite Wrappers ---

int sal_sqlite_open(const char* path, sqlite3** ppDb) {
    if (g_ctx.sql_open) return g_ctx.sql_open(path, ppDb, SQLITE_OPEN_READWRITE, NULL);
    return -1;
}

int sal_sqlite_exec(sqlite3* db, const char* sql, char** errmsg) {
    if (g_ctx.sql_exec) return g_ctx.sql_exec(db, sql, NULL, NULL, errmsg);
    return -1;
}

int sal_sqlite_close(sqlite3* db) {
    if (g_ctx.sql_close) return g_ctx.sql_close(db);
    return -1;
}

void sal_sqlite_free(void* ptr) {
    if (g_ctx.sql_free) g_ctx.sql_free(ptr);
}