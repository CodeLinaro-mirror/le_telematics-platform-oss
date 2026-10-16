/*
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

/*
 * Console Sink Implementation Header
 */

#ifndef CLOG_SINK_CONSOLE_H
#define CLOG_SINK_CONSOLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "clog.h"

/* ============================================================================
 * Console Backend Logging
 * ============================================================================ */

/**
 * @brief Log message to console backend (stdout/stderr)
 * 
 * @param context_id Context identifier
 * @param level Log level
 * @param message Formatted message
 * 
 * @return CLOG_RETURN_OK on success, CLOG_RETURN_WRONG_PARAMETER on failure
 */
clog_return_t clog_log_console(const char *context_id, clog_level_t level, const char *message);

#ifdef __cplusplus
}
#endif

#endif /* CLOG_SINK_CONSOLE_H */
