/*
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

/*
 * File Sink Implementation
 */

#include "clog_sink_file.h"
#include "clog_sinkImpl.h"
#include "clog_internal.h"
#include "clog_util.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifdef CLOG_WITH_FILE
#define CLOG_FILE_LOG_DIR "/data/common_logs"
#define CLOG_FILE_PATH_LEN 320

typedef enum {
    CLOG_MANAGED_LOG_FILE_LEGACY_ROTATED = 0,
    CLOG_MANAGED_LOG_FILE_LEGACY_ACTIVE = 1,
    CLOG_MANAGED_LOG_FILE_TIMESTAMPED = 2
} clog_managed_log_file_type_t;

typedef struct {
    char name[CLOG_FILE_PATH_LEN];
    clog_managed_log_file_type_t type;
    unsigned int legacy_index;
} clog_managed_log_file_t;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get timestamp string
 */
static const char *clog_get_file_log_app_id(void)
{
    const char *app_id = clog_get_registered_app_id();

    return (app_id != NULL && app_id[0] != '\0' && strcmp(app_id, "-") != 0) ? app_id : "unknown";
}

static const char *clog_get_error_string(int error_number, char *buffer, size_t buffer_size)
{
#if defined(__GLIBC__) && defined(_GNU_SOURCE)
    char *message = strerror_r(error_number, buffer, buffer_size);

    return message != NULL ? message : "Unknown error";
#else
    if (strerror_r(error_number, buffer, buffer_size) != 0) {
        (void)snprintf(buffer, buffer_size, "Unknown error %d", error_number);
    }

    return buffer;
#endif
}

static void clog_ensure_log_dir(const char *dir)
{
    struct stat st;

    if (mkdir(dir, 01777) != 0 && errno != EEXIST) {
        int saved_errno = errno;
        char error_message[128];
        clog_log_internal(CLOG_LEVEL_ERROR, "[FILE Backend] ERROR: Failed to create directory: %s (%s)", dir, clog_get_error_string(saved_errno, error_message, sizeof(error_message)));
        return;
    }

    if (stat(dir, &st) == 0 && (st.st_mode & 01777) == 01777) {
        return;
    }

    if (chmod(dir, 01777) != 0 && errno != EPERM) {
        int saved_errno = errno;
        char error_message[128];
        clog_log_internal(CLOG_LEVEL_ERROR, "[FILE Backend] ERROR: Failed to set permissions on directory: %s (%s)", dir, clog_get_error_string(saved_errno, error_message, sizeof(error_message)));
    }
}

static int clog_string_has_suffix(const char *value, const char *suffix)
{
    size_t value_len;
    size_t suffix_len;

    if (value == NULL || suffix == NULL) {
        return 0;
    }

    value_len = strlen(value);
    suffix_len = strlen(suffix);
    if (value_len < suffix_len) {
        return 0;
    }

    return strcmp(value + value_len - suffix_len, suffix) == 0 ? 1 : 0;
}

static int clog_is_legacy_active_log_file_name(const char *file_name, const char *app_id)
{
    size_t app_id_len;
    size_t file_name_len;

    if (file_name == NULL || app_id == NULL) {
        return 0;
    }

    app_id_len = strlen(app_id);
    file_name_len = strlen(file_name);
    if (file_name_len != app_id_len + 4u) {
        return 0;
    }

    return strncmp(file_name, app_id, app_id_len) == 0 &&
           strcmp(file_name + app_id_len, ".txt") == 0;
}

static int clog_parse_legacy_rotated_log_file_name(const char *file_name,
                                                   const char *app_id,
                                                   unsigned int *index)
{
    size_t app_id_len;
    size_t file_name_len;
    const char *cursor;
    unsigned int value;

    if (file_name == NULL || app_id == NULL || index == NULL) {
        return 0;
    }

    app_id_len = strlen(app_id);
    file_name_len = strlen(file_name);
    if (file_name_len <= app_id_len + 5u) {
        return 0;
    }

    if (strncmp(file_name, app_id, app_id_len) != 0) {
        return 0;
    }

    cursor = file_name + app_id_len;
    if (strncmp(cursor, ".txt.", 5) != 0) {
        return 0;
    }

    cursor += 5;
    if (*cursor == '\0') {
        return 0;
    }

    value = 0u;
    while (*cursor != '\0') {
        if (*cursor < '0' || *cursor > '9') {
            return 0;
        }

        value = (value * 10u) + (unsigned int)(*cursor - '0');
        cursor++;
    }

    *index = value;
    return 1;
}

static int clog_is_timestamped_log_file_name(const char *file_name, const char *app_id)
{
    char suffix[CLOG_FILE_PATH_LEN] = {0};
    size_t file_name_len;
    int suffix_len;

    if (file_name == NULL || app_id == NULL) {
        return 0;
    }

    file_name_len = strlen(file_name);
    suffix_len = snprintf(suffix, sizeof(suffix), "_%s.txt", app_id);
    if (suffix_len < 0 || (size_t)suffix_len >= sizeof(suffix)) {
        clog_log_internal(CLOG_LEVEL_ERROR,
                "[FILE Backend] ERROR: Timestamp match suffix is too long for app: %s",
                app_id);
        return 0;
    }

    if (file_name_len <= (size_t)suffix_len) {
        return 0;
    }

    return clog_string_has_suffix(file_name, suffix);
}

static int clog_classify_managed_log_file(clog_managed_log_file_t *file,
                                          const char *file_name,
                                          const char *app_id)
{
    unsigned int legacy_index;
    int copy_rc;

    if (file == NULL || file_name == NULL || app_id == NULL) {
        return 0;
    }

    if (clog_parse_legacy_rotated_log_file_name(file_name, app_id, &legacy_index)) {
        file->type = CLOG_MANAGED_LOG_FILE_LEGACY_ROTATED;
        file->legacy_index = legacy_index;
    } else if (clog_is_legacy_active_log_file_name(file_name, app_id)) {
        file->type = CLOG_MANAGED_LOG_FILE_LEGACY_ACTIVE;
        file->legacy_index = 0u;
    } else if (clog_is_timestamped_log_file_name(file_name, app_id)) {
        file->type = CLOG_MANAGED_LOG_FILE_TIMESTAMPED;
        file->legacy_index = 0u;
    } else {
        return 0;
    }

    copy_rc = snprintf(file->name, sizeof(file->name), "%s", file_name);
    if (copy_rc < 0 || (size_t)copy_rc >= sizeof(file->name)) {
        clog_log_internal(CLOG_LEVEL_ERROR,
                "[FILE Backend] ERROR: Managed file name is too long for app: %s",
                app_id);
        return 0;
    }

    return 1;
}

static int clog_build_log_file_path(char *buffer, size_t size, const char *file_name)
{
    int path_len;

    if (buffer == NULL || file_name == NULL) {
        return 0;
    }

    path_len = snprintf(buffer, size, CLOG_FILE_LOG_DIR "/%s", file_name);
    if (path_len < 0 || (size_t)path_len >= size) {
        clog_log_internal(CLOG_LEVEL_ERROR,
                "[FILE Backend] ERROR: Log file path is too long for file: %s",
                file_name);
        return 0;
    }

    return 1;
}

static off_t clog_get_file_size_bytes(const char *path)
{
    struct stat st;

    if (stat(path, &st) != 0) {
        return (off_t)-1;
    }
    return st.st_size;
}

static int clog_compare_managed_log_files(const void *lhs, const void *rhs)
{
    const clog_managed_log_file_t *left = (const clog_managed_log_file_t *)lhs;
    const clog_managed_log_file_t *right = (const clog_managed_log_file_t *)rhs;

    if (left->type != right->type) {
        return (int)left->type - (int)right->type;
    }

    if (left->type == CLOG_MANAGED_LOG_FILE_LEGACY_ROTATED) {
        if (left->legacy_index < right->legacy_index) {
            return 1;
        }
        if (left->legacy_index > right->legacy_index) {
            return -1;
        }
    }

    return strcmp(left->name, right->name);
}

static void clog_add_microseconds(struct timeval *value, unsigned int additional_usec)
{
    long usec;

    if (value == NULL || additional_usec == 0u) {
        return;
    }

    value->tv_sec += (time_t)(additional_usec / 1000000u);
    usec = (long)value->tv_usec + (long)(additional_usec % 1000000u);
    if (usec >= 1000000L) {
        value->tv_sec += 1;
        usec -= 1000000L;
    }

    value->tv_usec = (suseconds_t)usec;
}

static int clog_build_timestamped_log_file_path(char *buffer,
                                                size_t size,
                                                const char *app_id,
                                                const struct timeval *timestamp)
{
    struct tm tm_info;
    char time_part[32];
    int path_len;

    if (buffer == NULL || app_id == NULL || timestamp == NULL) {
        return 0;
    }

    if (localtime_r(&timestamp->tv_sec, &tm_info) == NULL ||
        strftime(time_part, sizeof(time_part), "%Y%m%d-%H%M%S", &tm_info) == 0) {
        path_len = snprintf(buffer,
                            size,
                            CLOG_FILE_LOG_DIR "/%ld_%06ld_%s.txt",
                            (long)timestamp->tv_sec,
                            (long)timestamp->tv_usec,
                            app_id);
    } else {
        path_len = snprintf(buffer,
                            size,
                            CLOG_FILE_LOG_DIR "/%s_%06ld_%s.txt",
                            time_part,
                            (long)timestamp->tv_usec,
                            app_id);
    }

    if (path_len < 0 || (size_t)path_len >= size) {
        clog_log_internal(CLOG_LEVEL_ERROR,
                "[FILE Backend] ERROR: Timestamped log file path is too long for app: %s",
                app_id);
        return 0;
    }

    return 1;
}

static int clog_build_new_log_file_path(char *buffer, size_t size, const char *app_id)
{
    struct timeval now;
    unsigned int attempt;

    if (gettimeofday(&now, NULL) != 0) {
        now.tv_sec = time(NULL);
        now.tv_usec = 0;
    }

    for (attempt = 0u; attempt < 1024u; attempt++) {
        struct timeval candidate;

        candidate = now;
        clog_add_microseconds(&candidate, attempt);
        if (!clog_build_timestamped_log_file_path(buffer, size, app_id, &candidate)) {
            return 0;
        }

        if (access(buffer, F_OK) != 0) {
            if (errno == ENOENT) {
                return 1;
            }

            {
                int saved_errno = errno;
                char error_message[128];
                clog_log_internal(CLOG_LEVEL_ERROR,
                        "[FILE Backend] ERROR: Failed to probe file path: %s (%s)",
                        buffer,
                        clog_get_error_string(saved_errno, error_message, sizeof(error_message)));
            }
            return 0;
        }
    }

    clog_log_internal(CLOG_LEVEL_ERROR,
            "[FILE Backend] ERROR: Failed to allocate a unique timestamped file for app: %s",
            app_id);
    return 0;
}

static int clog_find_active_log_file_path(char *buffer, size_t size, const char *app_id)
{
    DIR *dir;
    struct dirent *entry;
    char newest_name[CLOG_FILE_PATH_LEN];
    int found;

    if (buffer == NULL || app_id == NULL) {
        return 0;
    }

    dir = opendir(CLOG_FILE_LOG_DIR);
    if (dir == NULL) {
        if (errno != ENOENT) {
            int saved_errno = errno;
            char error_message[128];
            clog_log_internal(CLOG_LEVEL_ERROR,
                    "[FILE Backend] ERROR: Failed to scan log directory: %s (%s)",
                    CLOG_FILE_LOG_DIR,
                    clog_get_error_string(saved_errno, error_message, sizeof(error_message)));
        }
        return 0;
    }

    found = 0;
    newest_name[0] = '\0';
    while ((entry = readdir(dir)) != NULL) {
        int copy_rc;

        if (!clog_is_timestamped_log_file_name(entry->d_name, app_id)) {
            continue;
        }

        if (found && strcmp(entry->d_name, newest_name) <= 0) {
            continue;
        }

        copy_rc = snprintf(newest_name, sizeof(newest_name), "%s", entry->d_name);
        if (copy_rc < 0 || (size_t)copy_rc >= sizeof(newest_name)) {
            clog_log_internal(CLOG_LEVEL_ERROR,
                    "[FILE Backend] ERROR: Active file name is too long for app: %s",
                    app_id);
            continue;
        }

        found = 1;
    }

    (void)closedir(dir);
    if (!found) {
        return 0;
    }

    return clog_build_log_file_path(buffer, size, newest_name);
}

static void clog_prune_log_files(const char *app_id, uint16_t rotate_count)
{
    DIR *dir;
    struct dirent *entry;
    clog_managed_log_file_t *files;
    size_t count;
    size_t capacity;
    size_t max_files;
    size_t idx;

    if (app_id == NULL) {
        return;
    }

    dir = opendir(CLOG_FILE_LOG_DIR);
    if (dir == NULL) {
        if (errno != ENOENT) {
            int saved_errno = errno;
            char error_message[128];
            clog_log_internal(CLOG_LEVEL_ERROR,
                    "[FILE Backend] ERROR: Failed to prune log directory: %s (%s)",
                    CLOG_FILE_LOG_DIR,
                    clog_get_error_string(saved_errno, error_message, sizeof(error_message)));
        }
        return;
    }

    files = NULL;
    count = 0u;
    capacity = 0u;
    max_files = (size_t)rotate_count + 1u;

    while ((entry = readdir(dir)) != NULL) {
        clog_managed_log_file_t file;

        if (!clog_classify_managed_log_file(&file, entry->d_name, app_id)) {
            continue;
        }

        if (count == capacity) {
            size_t new_capacity;
            clog_managed_log_file_t *new_files;

            new_capacity = capacity == 0u ? 8u : capacity * 2u;
            new_files = (clog_managed_log_file_t *)realloc(files,
                    new_capacity * sizeof(clog_managed_log_file_t));
            if (new_files == NULL) {
                clog_log_internal(CLOG_LEVEL_ERROR,
                        "[FILE Backend] ERROR: Failed to allocate prune list for app: %s",
                        app_id);
                free(files);
                (void)closedir(dir);
                return;
            }

            files = new_files;
            capacity = new_capacity;
        }

        files[count] = file;
        count++;
    }

    (void)closedir(dir);
    if (count <= max_files) {
        free(files);
        return;
    }

    qsort(files, count, sizeof(clog_managed_log_file_t), clog_compare_managed_log_files);
    for (idx = 0u; idx < count - max_files; idx++) {
        char file_path[CLOG_FILE_PATH_LEN];

        if (!clog_build_log_file_path(file_path, sizeof(file_path), files[idx].name)) {
            continue;
        }

        if (unlink(file_path) != 0 && errno != ENOENT) {
            int saved_errno = errno;
            char error_message[128];
            clog_log_internal(CLOG_LEVEL_ERROR,
                    "[FILE Backend] ERROR: Failed to prune file: %s (%s)",
                    file_path,
                    clog_get_error_string(saved_errno, error_message, sizeof(error_message)));
        }
    }

    free(files);
}

static int clog_should_roll_to_new_file(const char *file_path, size_t entry_size)
{
    size_t max_size;
    off_t current_size;

    max_size = clog_get_sink_file_max_size_bytes();
    if (max_size == 0u) {
        return 0;
    }

    current_size = clog_get_file_size_bytes(file_path);
    if (current_size < 0) {
        return 0;
    }

    return (size_t)current_size + entry_size > max_size ? 1 : 0;
}

/* ============================================================================
 * File Backend Logging
 * ============================================================================ */

static clog_return_t clog_log_file_internal(const clog_context_internal_t *ctx,
                                            const char *context_id,
                                            clog_level_t level,
                                            const char *message)
{
    char timestamp[32] = {0};
    char file_path[CLOG_FILE_PATH_LEN] = {0};
    char formatted_message[CLOG_MAX_FORMATTED_MESSAGE_LEN] = {0};
    const char *message_to_log = message;
    const char *app_id;
    FILE *fp;
    int entry_size;

    /* Filter: skip if backend level is OFF */
    if (level == CLOG_LEVEL_OFF) {
        return CLOG_RETURN_OK;
    }

    if (message == NULL) {
        return CLOG_RETURN_WRONG_PARAMETER;
    }

    (void)context_id;

    if ((ctx != NULL) &&
        (clog_build_backend_message(formatted_message,
                                   sizeof(formatted_message),
                                   CLOG_BACKEND_FILE,
                                   ctx,
                                   message)!= 0)) {
        message_to_log = formatted_message;
    }

    app_id = clog_get_file_log_app_id();
    clog_ensure_log_dir(CLOG_FILE_LOG_DIR);

    if (!clog_find_active_log_file_path(file_path, sizeof(file_path), app_id) &&
        !clog_build_new_log_file_path(file_path, sizeof(file_path), app_id)) {
        return CLOG_RETURN_ERROR;
    }

    clog_util_get_timestamp(timestamp, sizeof(timestamp));

    entry_size = snprintf(NULL, 0, "[%s] [%s] %s\n",
                          timestamp,
                          clog_level_to_string(level),
                          message_to_log);
    if (entry_size < 0) {
        clog_log_internal(CLOG_LEVEL_ERROR, "[FILE Backend] ERROR: Failed to size log entry for %s", file_path);
        return CLOG_RETURN_ERROR;
    }

    if (clog_should_roll_to_new_file(file_path, (size_t)entry_size) &&
        !clog_build_new_log_file_path(file_path, sizeof(file_path), app_id)) {
        return CLOG_RETURN_ERROR;
    }

    fp = fopen(file_path, "a");
    if (fp == NULL) {
        int saved_errno = errno;
        char error_message[128];
        clog_log_internal(CLOG_LEVEL_ERROR, "[FILE Backend] ERROR: Failed to open file: %s (%s)", file_path, clog_get_error_string(saved_errno, error_message, sizeof(error_message)));
        return CLOG_RETURN_ERROR;
    }

    if (fprintf(fp,
                "[%s] [%s] %s\n",
                timestamp,
                clog_level_to_string(level),
                message_to_log) < 0) {
        int saved_errno = errno;
        char error_message[128];
        clog_log_internal(CLOG_LEVEL_ERROR,
                "[FILE Backend] ERROR: Failed to write file: %s (%s)",
                file_path,
                clog_get_error_string(saved_errno, error_message, sizeof(error_message)));
        (void)fclose(fp);
        return CLOG_RETURN_ERROR;
    }

    if (fclose(fp) != 0) {
        int saved_errno = errno;
        char error_message[128];
        clog_log_internal(CLOG_LEVEL_ERROR,
                "[FILE Backend] ERROR: Failed to close file: %s (%s)",
                file_path,
                clog_get_error_string(saved_errno, error_message, sizeof(error_message)));
        return CLOG_RETURN_ERROR;
    }

    clog_prune_log_files(app_id, clog_get_sink_file_rotate_count());

#ifdef CLOG_WITH_DEBUG
    /* Trace file delivery details */
    clog_log_internal(CLOG_LEVEL_DEBUG, "[FILE Backend] File: %s | App: %s | Tag: %s | Level: %s | Message: %s",
            file_path,
            app_id,
            ctx != NULL ? ctx->context_id : "-",
            clog_level_to_string(level),
            message_to_log);
#else
    if (clog_debug_trace_enabled()) {
        clog_log_internal(CLOG_LEVEL_DEBUG, "[FILE Backend] File: %s | App: %s | Tag: %s | Level: %s | Message: %s",
                file_path,
                app_id,
                ctx != NULL ? ctx->context_id : "-",
                clog_level_to_string(level),
                message_to_log);
    }
#endif

    return CLOG_RETURN_OK;
}

clog_return_t clog_log_file(const char *context_id, clog_level_t level, const char *message)
{
    return clog_log_file_internal(NULL, context_id, level, message);
}

clog_return_t clog_log_file_ctx(void *ctx, clog_level_t level, const char *message)
{
    return clog_log_file_internal((const clog_context_internal_t *)ctx, NULL, level, message);
}
#else
clog_return_t clog_log_file(const char *context_id, clog_level_t level, const char *message)
{
    (void)context_id;
    (void)level;
    (void)message;
    clog_log_internal(CLOG_LEVEL_ERROR, "[FILE Backend] ERROR: File backend not enabled (CLOG_WITH_FILE not defined)");
    return CLOG_RETURN_ERROR;
}

clog_return_t clog_log_file_ctx(void *ctx, clog_level_t level, const char *message)
{
    (void)ctx;
    return clog_log_file(NULL, level, message);
}
#endif
