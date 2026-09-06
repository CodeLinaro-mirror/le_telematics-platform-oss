/*
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

/*
 * Logcat Sink Implementation
 */

#include "clog_sink_logcat.h"
#include "clog_sinkImpl.h"
#include "clog_internal.h"
#include <stdio.h>

/* Android logcat includes */
#ifdef CLOG_WITH_LOGCAT
#include <android/log.h>
#endif

/* ============================================================================
 * Logcat Level Conversion
 * ============================================================================ */

int clog_level_to_android_priority(clog_level_t level)
{
#ifdef CLOG_WITH_LOGCAT
    switch (level) {
        case CLOG_LEVEL_FATAL:       return ANDROID_LOG_FATAL;
        case CLOG_LEVEL_ERROR:       return ANDROID_LOG_ERROR;
        case CLOG_LEVEL_WARN:        return ANDROID_LOG_WARN;
        case CLOG_LEVEL_INFO:        return ANDROID_LOG_INFO;
        case CLOG_LEVEL_DEBUG:       return ANDROID_LOG_DEBUG;
        case CLOG_LEVEL_VERBOSE:     return ANDROID_LOG_VERBOSE;
        case CLOG_LEVEL_OFF:      
        default:                     return ANDROID_LOG_INFO;
    }
#else
    (void)level;
    return 0;
#endif
}

clog_level_t clog_level_from_android_priority(int priority)
{
#ifdef CLOG_WITH_LOGCAT
    switch (priority) {
        case ANDROID_LOG_FATAL:      return CLOG_LEVEL_FATAL;
        case ANDROID_LOG_ERROR:      return CLOG_LEVEL_ERROR;
        case ANDROID_LOG_WARN:       return CLOG_LEVEL_WARN;
        case ANDROID_LOG_INFO:       return CLOG_LEVEL_INFO;
        case ANDROID_LOG_DEBUG:      return CLOG_LEVEL_DEBUG;
        case ANDROID_LOG_VERBOSE:    return CLOG_LEVEL_VERBOSE;
        default:                     return CLOG_LEVEL_INFO;
    }
#else
    (void)priority;
    return CLOG_LEVEL_INFO;
#endif
}

/* ============================================================================
 * Logcat Backend Logging
 * ============================================================================ */

static clog_return_t clog_log_logcat_internal(const clog_context_internal_t *ctx,
                                              const char *context_id,
                                              clog_level_t level,
                                              const char *message)
{
#ifdef CLOG_WITH_LOGCAT
    int priority = clog_level_to_android_priority(level);
    char app_tag[CLOG_MAX_DETAIL_LEN] = {0};
    char formatted_message[CLOG_MAX_FORMATTED_MESSAGE_LEN] = {0};
    const char *context_tag = context_id != NULL ? context_id : "-";
    const char *message_to_log = message;

    /* Filter: skip if backend level is OFF */
    if (level == CLOG_LEVEL_OFF) {
        return CLOG_RETURN_OK;
    }

    if (message == NULL) {
        return CLOG_RETURN_WRONG_PARAMETER;
    }

    if (ctx != NULL) {
        context_tag = (ctx->logcat_tag[0] != '\0') ? ctx->logcat_tag : ctx->context_id;
        if (clog_build_backend_message(formatted_message,
                                       sizeof(formatted_message),
                                       CLOG_BACKEND_LOGCAT,
                                       ctx,
                                       message) != 0 ) {
            message_to_log = formatted_message;
        }
    }

    if (clog_build_app_tag(app_tag, sizeof(app_tag)) == 0 ) {
        return CLOG_RETURN_ERROR;
    }

    /* Call actual Android logcat API */
    /* __android_log_print() is the standard Android logging function */
    __android_log_print(priority, app_tag, "%s", message_to_log);

#ifdef CLOG_WITH_DEBUG
    /* Trace logcat delivery details */
    clog_log_internal(CLOG_LEVEL_DEBUG, "[LOGCAT Backend] Tag: %s | Context: %s | Priority: %d | Level: %s | Message: %s",
            app_tag,
            context_tag,
            priority,
            clog_level_to_string(level),
            message_to_log);
#else
    if (clog_debug_trace_enabled()) {
        clog_log_internal(CLOG_LEVEL_DEBUG, "[LOGCAT Backend] Tag: %s | Context: %s | Priority: %d | Level: %s | Message: %s",
                app_tag,
                context_tag,
                priority,
                clog_level_to_string(level),
                message_to_log);
    }
#endif

    return CLOG_RETURN_OK;
#else
    (void)ctx;
    (void)context_id;
    (void)level;
    (void)message;
    clog_log_internal(CLOG_LEVEL_ERROR, "[LOGCAT Backend] ERROR: Logcat backend not enabled (CLOG_WITH_LOGCAT not defined)");
    return CLOG_RETURN_ERROR;
#endif
}

clog_return_t clog_log_logcat(const char *context_id, clog_level_t level, const char *message)
{
    return clog_log_logcat_internal(NULL, context_id, level, message);
}

clog_return_t clog_log_logcat_ctx(void *ctx, clog_level_t level, const char *message)
{
    return clog_log_logcat_internal((const clog_context_internal_t *)ctx, NULL, level, message);
}
