/*
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

/*
 * Console Sink Implementation
 */

#include "clog_sink_console.h"
#include "clog_sinkImpl.h"
#include "clog_internal.h"
#include "clog_util.h"
#include <stdio.h>
#include <time.h>

#ifdef CLOG_WITH_CONSOLE

/* ============================================================================
 * Console Backend Logging
 * ============================================================================ */

static clog_return_t clog_log_console_internal(const clog_context_internal_t *ctx,
                                               const char *context_id,
                                               clog_level_t level,
                                               const char *message)
{
    char timestamp[32] = {0};
    char formatted_message[CLOG_MAX_FORMATTED_MESSAGE_LEN] = {0};
    const char *message_to_log = message;
    FILE *stream;

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
                                   CLOG_BACKEND_CONSOLE,
                                   ctx,
                                   message)!= 0)) {
        message_to_log = formatted_message;
    }

    /* Use stderr for errors and above, stdout for others */
    stream = (level >= CLOG_LEVEL_ERROR) ? stderr : stdout;
    timestamp[0] = '\0';
    clog_util_get_timestamp(timestamp, sizeof(timestamp));

    /* Print with timestamp, level, and optional backend-specific details. */
    fprintf(stream, "[%s] [%s] %s\n",
            timestamp,
            clog_level_to_string(level),
            message_to_log);

    fflush(stream);

    return CLOG_RETURN_OK;
}

clog_return_t clog_log_console(const char *context_id, clog_level_t level, const char *message)
{
    return clog_log_console_internal(NULL, context_id, level, message);
}

clog_return_t clog_log_console_ctx(void *ctx, clog_level_t level, const char *message)
{
    return clog_log_console_internal((const clog_context_internal_t *)ctx, NULL, level, message);
}
#else
clog_return_t clog_log_console(const char *context_id, clog_level_t level, const char *message)
{
    (void)context_id;
    (void)level;
    (void)message;
    clog_log_internal(CLOG_LEVEL_ERROR, "[CONSOLE Backend] ERROR: Console backend not enabled (CLOG_WITH_CONSOLE not defined)");
    return CLOG_RETURN_ERROR;
}

clog_return_t clog_log_console_ctx(void *ctx, clog_level_t level, const char *message)
{
    (void)ctx;
    return clog_log_console(NULL, level, message);
}
#endif
