/**
 * @file sqlite_utils.h
 * @brief SQLite business logic.
 */

#ifndef PROJECT_HUB_SQLITE_UTILS_H
#define PROJECT_HUB_SQLITE_UTILS_H

#include "defs.h"

/**
 * @brief performs cleanup: TRANSACTION(Delete) -> VACUUM.
 */
ProjectStatus sqlite_perform_cleanup(const char* db_path);

#endif // PROJECT_HUB_SQLITE_UTILS_H