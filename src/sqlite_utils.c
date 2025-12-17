#include "xml_utils.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

// 16KB stack buffer - safe for child process, robust for shared_prefs
#define MAX_XML_SIZE 16384 

/**
 * @brief Decodes basic XML entities in place/copy.
 * Handles &lt; &gt; &amp; &quot; &apos;
 */
static void decode_xml_entities(const char* src, char* dst, size_t max_dst) {
    size_t s = 0, d = 0;
    while (src[s] && d < max_dst - 1) {
        if (src[s] == '&') {
            if (strncmp(&src[s], "&lt;", 4) == 0)       { dst[d++] = '<'; s += 4; }
            else if (strncmp(&src[s], "&gt;", 4) == 0)  { dst[d++] = '>'; s += 4; }
            else if (strncmp(&src[s], "&amp;", 5) == 0) { dst[d++] = '&'; s += 5; }
            else if (strncmp(&src[s], "&quot;", 6) == 0){ dst[d++] = '"'; s += 6; }
            else if (strncmp(&src[s], "&apos;", 6) == 0){ dst[d++] = '\''; s += 6; }
            else { dst[d++] = src[s++]; } // Unknown entity, copy raw
        } else {
            dst[d++] = src[s++];
        }
    }
    dst[d] = '\0';
}

static const char* xml_find_next_tag(const char* cursor) { 
    return strchr(cursor, '<'); 
}

/**
 * @brief Robust attribute extractor.
 * Enforces word boundaries to prevent substring matches (e.g. finding "name" inside "filename").
 */
static int xml_get_attribute(const char* tag_start, const char* attr_name, char* out_val, size_t max_len) {
    size_t name_len = strlen(attr_name);
    const char* p = tag_start;

    // Iterate through the tag content looking for the attribute
    while (*p && *p != '>') {
        p = strstr(p, attr_name);
        if (!p) return -1;

        // 1. Check Preceding Character (Start of string or whitespace)
        char prev = (p == tag_start) ? ' ' : *(p - 1);
        if (!isspace((unsigned char)prev)) {
            p++; continue; // Partial match, keep searching
        }

        // 2. Check Succeeding Character (Must be '=' optionally surrounded by space)
        const char* cursor = p + name_len;
        while (isspace((unsigned char)*cursor)) cursor++;
        
        if (*cursor != '=') {
            p++; continue;
        }
        cursor++; // Skip '='
        while (isspace((unsigned char)*cursor)) cursor++;

        // 3. Extract Value
        char quote = *cursor;
        if (quote != '"' && quote != '\'') {
            p++; continue; // Malformed or unquoted
        }
        cursor++; // Skip quote

        const char* val_start = cursor;
        const char* val_end = strchr(val_start, quote);
        if (!val_end) return -1;

        // Decode entities into output buffer
        size_t raw_len = val_end - val_start;
        char temp_raw[1024]; // Temp buffer for raw string before decode
        
        if (raw_len >= sizeof(temp_raw)) raw_len = sizeof(temp_raw) - 1;
        memcpy(temp_raw, val_start, raw_len);
        temp_raw[raw_len] = '\0';

        decode_xml_entities(temp_raw, out_val, max_len);
        return 0;
    }
    return -1;
}

static int xml_extract_logic(const char* xml, const char* target_name, char* out_buf, size_t max_len) {
    const char* cursor = xml;
    while ((cursor = xml_find_next_tag(cursor)) != NULL) {
        // Skip comments/processing instructions
        if (cursor[1] == '/' || cursor[1] == '?' || cursor[1] == '!') { cursor++; continue; }

        const char* tag_end = strchr(cursor, '>'); 
        if (!tag_end) break; 

        // Extract tag content for attribute search
        // We limit search scope to the tag itself
        size_t tag_len = tag_end - cursor; 
        char tag_content[1024]; 
        if (tag_len >= sizeof(tag_content)) tag_len = sizeof(tag_content) - 1;
        memcpy(tag_content, cursor + 1, tag_len); // +1 to skip '<'
        tag_content[tag_len] = '\0';

        char name_val[128];
        if (xml_get_attribute(tag_content, "name", name_val, sizeof(name_val)) == 0) {
            if (strcmp(name_val, target_name) == 0) {
                // Strategy 1: value="..." attribute
                if (xml_get_attribute(tag_content, "value", out_buf, max_len) == 0) return 0; 
                
                // Strategy 2: Inner Text >...<
                const char* content_start = tag_end + 1;
                const char* content_end = strchr(content_start, '<');
                if (content_end) {
                    size_t len = content_end - content_start;
                    if (len >= sizeof(tag_content)) len = sizeof(tag_content) - 1; // Reuse temp buffer size logic
                    
                    char temp_raw[1024];
                    memcpy(temp_raw, content_start, len);
                    temp_raw[len] = '\0';
                    
                    decode_xml_entities(temp_raw, out_buf, max_len);
                    return 0;
                }
            }
        }
        cursor = tag_end + 1;
    }
    return -1; 
}

int xml_get_value(const char* path, const char* key, char* out_buf, size_t max_len) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;

    char file_buf[MAX_XML_SIZE];
    size_t total_read = 0;
    
    // Robust Read Loop
    while (total_read < sizeof(file_buf) - 1) {
        ssize_t r = read(fd, file_buf + total_read, sizeof(file_buf) - 1 - total_read);
        if (r < 0) {
            if (errno == EINTR) continue;
            close(fd); return -1;
        }
        if (r == 0) break;
        total_read += r;
    }
    close(fd);
    file_buf[total_read] = '\0';
    return xml_extract_logic(file_buf, key, out_buf, max_len);
}