/*
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

/*
 * Syslog Sink Implementation
 */

#include "clog_sink_syslog.h"
#include "clog_sinkImpl.h"
#include "clog_internal.h"
#include <stdio.h>
#include <string.h>

/* syslog includes */
#ifdef CLOG_WITH_SYSLOG
#include <syslog.h>
#endif

/* ============================================================================
 * Syslog Level Conversion
 * ============================================================================ */

int clog_level_to_syslog_priority(clog_level_t level)
{
#ifdef CLOG_WITH_SYSLOG
    switch (level) {
        case CLOG_LEVEL_FATAL:    return LOG_CRIT;
        case CLOG_LEVEL_ERROR:    return LOG_ERR;
        case CLOG_LEVEL_WARN:     return LOG_WARNING;
        case CLOG_LEVEL_INFO:     return LOG_INFO;
        case CLOG_LEVEL_DEBUG:    return LOG_DEBUG;
        case CLOG_LEVEL_VERBOSE:  return LOG_DEBUG;
        case CLOG_LEVEL_OFF:
        default:                  return LOG_INFO;
    }
#else
    (void)level;
    return 0;
#endif
}

clog_level_t clog_level_from_syslog_priority(int priority)
{
#ifdef CLOG_WITH_SYSLOG
    switch (priority) {
        case LOG_CRIT:      return CLOG_LEVEL_FATAL;
        case LOG_ERR:        return CLOG_LEVEL_ERROR;
        case LOG_WARNING:    return CLOG_LEVEL_WARN;
        case LOG_INFO:       return CLOG_LEVEL_INFO;
        case LOG_DEBUG:      return CLOG_LEVEL_DEBUG;
        default:             return CLOG_LEVEL_INFO;
    }
#else
    (void)priority;
    return CLOG_LEVEL_INFO;
#endif
}

/* ============================================================================
 * Syslog Ident Management
 * ============================================================================ */

#ifdef CLOG_WITH_SYSLOG
/* openlog(3) does not copy the ident string it is given - it only stores the
 * pointer, so the buffer backing it must outlive every subsequent syslog()
 * call in the process. Keep it here with static storage duration (never a
 * stack buffer) and only re-open when the tag actually changes, so we are
 * not repeating the syscall on every single message either. */
static char g_clog_syslog_ident[CLOG_MAX_DETAIL_LEN] = {0};
static int g_clog_syslog_ident_open = 0;

static void clog_syslog_apply_ident(const char *tag)
{
    if (tag == NULL || tag[0] == '\0') {
        tag = "-";
    }

    if (g_clog_syslog_ident_open && strcmp(g_clog_syslog_ident, tag) == 0) {
        return;
    }

    (void)snprintf(g_clog_syslog_ident, sizeof(g_clog_syslog_ident), "%s", tag);
    openlog(g_clog_syslog_ident, LOG_PID, LOG_USER);
    g_clog_syslog_ident_open = 1;
}
#endif

/* ============================================================================
 * Syslog Backend Logging
 * ============================================================================ */

static clog_return_t clog_log_syslog_internal(const clog_context_internal_t *ctx,
                                              const char *context_id,
                                              clog_level_t level,
                                              const char *message)
{
#ifdef CLOG_WITH_SYSLOG
    int priority = clog_level_to_syslog_priority(level);
    char app_tag[CLOG_MAX_DETAIL_LEN]= {0};
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
        context_tag = (ctx->syslog_tag[0] != '\0') ? ctx->syslog_tag : ctx->context_id;
        if (clog_build_backend_message(formatted_message,
                                       sizeof(formatted_message),
                                       CLOG_BACKEND_SYSLOG,
                                       ctx,
                                       message) != 0) {
            message_to_log = formatted_message;
        }
    }

    if (clog_build_app_tag(app_tag, sizeof(app_tag))== 0) {
        return CLOG_RETURN_ERROR;
    }

    clog_syslog_apply_ident(app_tag);

    /* Call actual syslog API */
    syslog(priority, "%s", message_to_log);

#ifdef CLOG_WITH_DEBUG
    /* Trace syslog delivery details */
    clog_log_internal(CLOG_LEVEL_DEBUG, "[SYSLOG Backend] App: %s | Tag: %s | Priority: %d | Level: %s | Message: %s",
            app_tag,
            context_tag,
            priority,
            clog_level_to_string(level),
            message_to_log);
#else
    if (clog_debug_trace_enabled()) {
        clog_log_internal(CLOG_LEVEL_DEBUG, "[SYSLOG Backend] App: %s | Tag: %s | Priority: %d | Level: %s | Message: %s",
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
    clog_log_internal(CLOG_LEVEL_ERROR, "[SYSLOG Backend] ERROR: Syslog backend not enabled (CLOG_WITH_SYSLOG not defined)");
    return CLOG_RETURN_ERROR;
#endif
}

clog_return_t clog_log_syslog(const char *context_id, clog_level_t level, const char *message)
{
    return clog_log_syslog_internal(NULL, context_id, level, message);
}

clog_return_t clog_log_syslog_ctx(void *ctx, clog_level_t level, const char *message)
{
    return clog_log_syslog_internal((const clog_context_internal_t *)ctx, NULL, level, message);
}

void clog_syslog_reset_ident(void)
{
#ifdef CLOG_WITH_SYSLOG
    g_clog_syslog_ident[0] = '\0';
    g_clog_syslog_ident_open = 0;
#endif
}
