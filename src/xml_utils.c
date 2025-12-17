#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "xml_utils.h"

#define MAX_TAG_SIZE 1024
#define MAX_XML_SIZE 16384  // 16KB

static ProjectStatus xml_get_attribute(const char* tag_content, const char* attr_name, char* out_val, size_t max_len)
{
    char quote = 0;
    char search[64] = { 0 };
    size_t search_len = 0, i = 0;
    const char* p = tag_content;
    snprintf(search, sizeof(search), "%s=", attr_name);
    search_len = strlen(search);

    while ((p = strstr(p, search)) != NULL)
    {
        if (p == tag_content || isspace((unsigned char)*(p - 1)))
        {
            p += search_len;
            quote = *p++;
            if (quote != '"' && quote != '\'') {
                return -1;
            }

            i = 0;
            while (*p && *p != quote && i < max_len - 1) {
                out_val[i++] = *p++;
            }
            out_val[i] = '\0';
            return STATUS_SUCCESS;
        }
        p++;
    }
    return STATUS_ERR_XML_PARSER;
}

static ProjectStatus xml_extract_logic(const char* xml, const char* target_name, char* out_buf, size_t out_buf_len)
{
    char* tag_end = NULL;
    const char* cursor = xml;
    char name_val[128] = { 0 };
    size_t len = 0, tag_len = 0;
    char tag_content[MAX_TAG_SIZE] = { 0 };
    if (xml == NULL || target_name == NULL || out_buf == NULL || out_buf_len == 0) {
        return STATUS_ERR_INVALID_ARG;
    }

    /* Iterate each tag */
    while ((cursor = strchr(cursor, '<')) != NULL)
    {
        if (cursor[1] == '/' || cursor[1] == '?' || cursor[1] == '!') {
            cursor++;
            continue;
        }
        /* Get current tag content (up to MAX_TAG_SIZE bytes) */
        tag_end = strchr(cursor, '>');
        if (tag_end == NULL) {
            break;
        }
        tag_len = tag_end - cursor;
        if (tag_len >= sizeof(tag_content)) {   /* Truncate */
            tag_len = sizeof(tag_content) - 1;
        }
        memset(tag_content, 0, sizeof(tag_content));
        memcpy(tag_content, cursor, tag_len); 
        tag_content[tag_len] = '\0';

        /* Search for our key, and extract its content. Assumes it's stored as one of those options: */
        /* Option 1: 'name="my_key" value="my_value"/>' */
        /* Option 2: 'name="my_key">my_value<' */
        memset(name_val, 0, sizeof(name_val));
        if (xml_get_attribute(tag_content, "name", name_val, sizeof(name_val)) == STATUS_SUCCESS
            && strncmp(name_val, target_name, sizeof(name_val)) == 0)
        {
                /* Option 1*/
                if (xml_get_attribute(tag_content, "value", out_buf, out_buf_len) == STATUS_SUCCESS) {
                    return STATUS_SUCCESS;
                }
                /* Option 2 */
                const char* content_start = tag_end + 1;
                const char* content_end = strchr(content_start, '<');
                if (content_end != NULL) {
                    len = content_end - content_start;
                    if (len >= out_buf_len) {   /* Truncate */
                        len = out_buf_len - 1;
                    }
                    if (len > 0) {
                        memcpy(out_buf, content_start, len);
                        out_buf[len] = '\0';
                        return STATUS_SUCCESS;
                    }
                }
        }
        cursor = tag_end + 1;
    }

    return STATUS_ERR_XML_PARSER;
}

ProjectStatus xml_get_value(const char* file_path, const char* key, char* out_buf, size_t max_len)
{
    size_t total_read = 0;
    ssize_t bytes_read = 0;
    char file_buf[MAX_XML_SIZE] = { 0 };
    if (file_path == NULL || key == NULL || out_buf == NULL || max_len == 0) {
        DEBUG("xml_get_value: Invalid arguments");
        return STATUS_ERR_INVALID_ARG;
    }

    int shared_pref_fd = open(file_path, O_RDONLY);
    if (shared_pref_fd < 0) {
        DEBUG("xml_get_value: Failed to open %s", file_path);
        return STATUS_ERR_OPEN_ERROR;
    }

    while (total_read < sizeof(file_buf) - 1)
    {
        bytes_read = read(shared_pref_fd, file_buf + total_read, sizeof(file_buf) - total_read - 1);
        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                close(shared_pref_fd);
                return STATUS_ERR_READ_ERROR;
            }
        } else if (bytes_read == 0) {
            break; /* EOF */
        }
        total_read += bytes_read;
    }
    close(shared_pref_fd);
    file_buf[total_read] = '\0';
    return xml_extract_logic(file_buf, key, out_buf, max_len);
}