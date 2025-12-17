/**
 * @file symbol_resolver.h
 * @brief System Abstraction Layer (SAL).
 *
 * Wraps dynamic loading of Android system libraries and SQLite.
 * Ensures the binary does not have build-time dependencies on the NDK.
 */

#ifndef PROJECT_HUB_SAL_H
#define PROJECT_HUB_SAL_H

#include <stddef.h>

// --- System Types ---
typedef int (*pfn_android_log_print)(int prio, const char* tag, const char* fmt, ...);
typedef int (*pfn_system_property_get)(const char* key, char* value);
typedef int (*pfn_setcon)(const char* context);

// --- SQLite Types ---
typedef struct sqlite3 sqlite3;
typedef int (*pfn_sqlite3_open_v2)(const char *filename, sqlite3 **ppDb, int flags, const char *zVfs);
typedef int (*pfn_sqlite3_exec)(sqlite3*, const char *sql, int (*callback)(void*,int,char**,char**), void *, char **errmsg);
typedef int (*pfn_sqlite3_close)(sqlite3*);
typedef void (*pfn_sqlite3_free)(void*);

// --- Lifecycle ---
int sal_init(void);
void sal_cleanup(void);

// --- System Wrappers ---
void sal_log_info(const char* fmt, ...);
void sal_log_error(const char* fmt, ...);
int sal_get_property(const char* key, char* value);
int sal_set_selinux_context(const char* context);

// --- SQLite Wrappers ---
int sal_sqlite_open(const char* path, sqlite3** ppDb);
int sal_sqlite_exec(sqlite3* db, const char* sql, char** errmsg);
int sal_sqlite_close(sqlite3* db);
void sal_sqlite_free(void* ptr);

#endif // PROJECT_HUB_SAL_H