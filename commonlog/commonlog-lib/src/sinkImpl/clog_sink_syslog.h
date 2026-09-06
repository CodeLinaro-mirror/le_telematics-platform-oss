/*
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

/*
 * Syslog Sink Implementation Header
 */

#ifndef CLOG_SINK_SYSLOG_H
#define CLOG_SINK_SYSLOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "clog.h"

/* ============================================================================
 * Syslog Level Conversion
 * ============================================================================ */

/**
 * @brief Convert CLOG level to syslog priority
 * 
 * @param level CLOG log level
 * @return Syslog priority
 */
int clog_level_to_syslog_priority(clog_level_t level);

/**
 * @brief Convert syslog priority to CLOG level
 *
 * @param priority Syslog priority
 * @return CLOG log level
 */
clog_level_t clog_level_from_syslog_priority(int priority);

/* ============================================================================
 * Syslog Backend Logging
 * ============================================================================ */

/**
 * @brief Log message to syslog backend
 * 
 * @param context_id Context identifier (used as syslog tag)
 * @param level Log level
 * @param message Formatted message
 * 
 * @return CLOG_RETURN_OK on success, CLOG_RETURN_WRONG_PARAMETER/CLOG_RETURN_ERROR on failure
 */
clog_return_t clog_log_syslog(const char *context_id, clog_level_t level, const char *message);

/**
 * @brief Reset the cached syslog ident/connection state
 *
 * Call after closelog() (e.g. on app unregister) so the next log call
 * re-opens the connection instead of assuming the cached tag is still active.
 */
void clog_syslog_reset_ident(void);

#ifdef __cplusplus
}
#endif

#endif /* CLOG_SINK_SYSLOG_H */
