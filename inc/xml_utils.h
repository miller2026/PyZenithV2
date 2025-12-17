/**
 * @file xml_utils.h
 * @brief Lightweight, stack-based XML parser for Shared Preferences.
 */

#ifndef PROJECT_HUB_XML_UTILS_H
#define PROJECT_HUB_XML_UTILS_H

#include <stddef.h>

/**
 * @brief Reads an XML file and extracts the value for a specific key.
 * Uses a fixed 16KB stack buffer to avoid heap usage.
 */
int xml_get_value(const char* path, const char* key, char* out_buf, size_t max_len);

#endif // PROJECT_HUB_XML_UTILS_H