/*
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

/*
 * Sink Implementation Utilities
 */

#include "clog_sinkImpl.h"
#include "clog_internal.h"
#include <stdio.h>

static clog_return_t clog_backend_disabled(const char *backend)
{
    clog_log_internal(CLOG_LEVEL_ERROR,
            "[%s Backend] ERROR: %s backend not enabled (backend source excluded from build)",
            backend, backend);
    return CLOG_RETURN_ERROR;
}

#ifndef CLOG_WITH_DLT
clog_return_t clog_dlt_init(const char *app_id, const char *app_description)
{
    (void)app_id;
    (void)app_description;
    return clog_backend_disabled("DLT");
}

clog_return_t clog_dlt_deinit(void)
{
    return CLOG_RETURN_ERROR;
}

void* clog_dlt_register_context(const char *context_id, const char *description)
{
    (void)context_id;
    (void)description;
    return NULL;
}

clog_return_t clog_dlt_unregister_context(void *dlt_context)
{
    (void)dlt_context;
    return CLOG_RETURN_ERROR;
}

void* clog_dlt_context_register_if_needed(clog_backend_t backend, const char *context_id, const char *description)
{
    (void)backend;
    (void)context_id;
    (void)description;
    return NULL;
}

clog_return_t clog_dlt_context_unregister_if_needed(clog_backend_t backend, void *dlt_context)
{
    (void)backend;
    (void)dlt_context;
    return CLOG_RETURN_ERROR;
}

int clog_level_to_dlt_level(clog_level_t level)
{
    (void)level;
    return 0;
}

clog_level_t clog_level_from_dlt_level(int dlt_level)
{
    (void)dlt_level;
    return CLOG_LEVEL_INFO;
}

clog_return_t clog_log_dlt(void *ctx, clog_level_t level, const char *message)
{
    (void)ctx;
    (void)level;
    (void)message;
    return clog_backend_disabled("DLT");
}
#endif

#ifndef CLOG_WITH_DIAG
clog_return_t clog_diag_init(void)
{
    return clog_backend_disabled("DIAG");
}

clog_return_t clog_diag_deinit(void)
{
    return CLOG_RETURN_ERROR;
}

clog_return_t clog_diag_is_initialized(void)
{
    return CLOG_RETURN_OK;
}

int clog_level_to_diag_level(clog_level_t level)
{
    (void)level;
    return 0;
}

clog_level_t clog_level_from_diag_level(int diag_level)
{
    (void)diag_level;
    return CLOG_LEVEL_INFO;
}

clog_return_t clog_log_diag(void *ctx, const char *file, int line, clog_level_t level, const char *message)
{
    (void)ctx;
    (void)file;
    (void)line;
    (void)level;
    (void)message;
    return clog_backend_disabled("DIAG");
}
#endif

#ifndef CLOG_WITH_SYSLOG
int clog_level_to_syslog_priority(clog_level_t level)
{
    (void)level;
    return 0;
}

clog_level_t clog_level_from_syslog_priority(int priority)
{
    (void)priority;
    return CLOG_LEVEL_INFO;
}

clog_return_t clog_log_syslog(const char *context_id, clog_level_t level, const char *message)
{
    (void)context_id;
    (void)level;
    (void)message;
    return clog_backend_disabled("SYSLOG");
}

clog_return_t clog_log_syslog_ctx(void *ctx, clog_level_t level, const char *message)
{
    (void)ctx;
    (void)level;
    (void)message;
    return clog_backend_disabled("SYSLOG");
}

void clog_syslog_reset_ident(void)
{
}
#endif

#ifndef CLOG_WITH_LOGCAT
int clog_level_to_android_priority(clog_level_t level)
{
    (void)level;
    return 0;
}

clog_level_t clog_level_from_android_priority(int priority)
{
    (void)priority;
    return CLOG_LEVEL_INFO;
}

clog_return_t clog_log_logcat(const char *context_id, clog_level_t level, const char *message)
{
    (void)context_id;
    (void)level;
    (void)message;
    return clog_backend_disabled("LOGCAT");
}

clog_return_t clog_log_logcat_ctx(void *ctx, clog_level_t level, const char *message)
{
    (void)ctx;
    return clog_log_logcat(NULL, level, message);
}
#endif

#ifndef CLOG_WITH_FILE
clog_return_t clog_log_file(const char *context_id, clog_level_t level, const char *message)
{
    (void)context_id;
    (void)level;
    (void)message;
    return clog_backend_disabled("FILE");
}

clog_return_t clog_log_file_ctx(void *ctx, clog_level_t level, const char *message)
{
    (void)ctx;
    return clog_log_file(NULL, level, message);
}
#endif

#ifndef CLOG_WITH_CONSOLE
clog_return_t clog_log_console(const char *context_id, clog_level_t level, const char *message)
{
    (void)context_id;
    (void)level;
    (void)message;
    return clog_backend_disabled("CONSOLE");
}

clog_return_t clog_log_console_ctx(void *ctx, clog_level_t level, const char *message)
{
    (void)ctx;
    return clog_log_console(NULL, level, message);
}
#endif

/* ============================================================================
 * Utility Functions Implementation
 * ============================================================================ */

const char* clog_level_to_string(clog_level_t level)
{
    switch (level) {
        case CLOG_LEVEL_OFF:     return "OFF";
        case CLOG_LEVEL_FATAL:   return "FATAL";
        case CLOG_LEVEL_ERROR:   return "ERROR";
        case CLOG_LEVEL_WARN:    return "WARN";
        case CLOG_LEVEL_INFO:    return "INFO";
        case CLOG_LEVEL_DEBUG:   return "DEBUG";
        case CLOG_LEVEL_VERBOSE: return "VERBOSE";
        default:                 return "UNKNOWN";
    }
}

const char* clog_backend_to_string(clog_backend_t backend)
{
    switch (backend) {
        case CLOG_BACKEND_DLT:     return "DLT";
        case CLOG_BACKEND_DIAG:    return "DIAG";
        case CLOG_BACKEND_SYSLOG:  return "syslog";
        case CLOG_BACKEND_LOGCAT:  return "logcat";
        case CLOG_BACKEND_CONSOLE: return "console";
        case CLOG_BACKEND_FILE:    return "file";
        default:                   return "unknown";
    }
}

const char* clog_return_to_string(clog_return_t ret)
{
    switch (ret) {
        case CLOG_RETURN_LOGGING_DISABLED: return "CLOG_RETURN_LOGGING_DISABLED";
        case CLOG_RETURN_WRONG_PARAMETER:  return "CLOG_RETURN_WRONG_PARAMETER";
        case CLOG_RETURN_BUFFER_FULL:      return "CLOG_RETURN_BUFFER_FULL";
        case CLOG_RETURN_ERROR:            return "CLOG_RETURN_ERROR";
        case CLOG_RETURN_OK:               return "CLOG_RETURN_OK";
        case CLOG_RETURN_TRUE:             return "CLOG_RETURN_TRUE";
        default:                           return "CLOG_RETURN_UNKNOWN";
    }
}

int clog_flush(void)
{
    /* Flush stdout and stderr */
    fflush(stdout);
    fflush(stderr);
    
    return 0;
}
