#ifndef XML_UTILS_H
#define XML_UTILS_H

#include <stddef.h>

/**
 * Reads an XML file and extracts the value for a specific key.
 * This is not a robust parser! It's used for simple shared pref files.
 * Assumptions: file must not be larger than 16KB.
 */
ProjectStatus xml_get_value(const char* file_path, const char* key, char* out_buf, size_t max_len);

#endif // XML_UTILS_H