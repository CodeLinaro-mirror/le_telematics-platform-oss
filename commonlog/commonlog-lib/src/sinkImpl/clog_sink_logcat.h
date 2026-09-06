/*
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

/*
 * Logcat Sink Implementation Header
 */

#ifndef CLOG_SINK_LOGCAT_H
#define CLOG_SINK_LOGCAT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "clog.h"

/* ============================================================================
 * Logcat Level Conversion
 * ============================================================================ */

/**
 * @brief Convert CLOG level to Android logcat priority
 * 
 * @param level CLOG log level
 * @return Android log priority
 */
int clog_level_to_android_priority(clog_level_t level);

/**
 * @brief Convert Android logcat priority to CLOG level
 *
 * @param priority Android log priority
 * @return CLOG log level
 */
clog_level_t clog_level_from_android_priority(int priority);

/* ============================================================================
 * Logcat Backend Logging
 * ============================================================================ */

/**
 * @brief Log message to logcat backend
 * 
 * @param context_id Context identifier (used as logcat tag)
 * @param level Log level
 * @param message Formatted message
 * 
 * @return CLOG_RETURN_OK on success, CLOG_RETURN_WRONG_PARAMETER/CLOG_RETURN_ERROR on failure
 */
clog_return_t clog_log_logcat(const char *context_id, clog_level_t level, const char *message);

#ifdef __cplusplus
}
#endif

#endif /* CLOG_SINK_LOGCAT_H */
