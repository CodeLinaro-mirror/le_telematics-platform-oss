/*
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

/*
 * File Sink Implementation Header
 */

#ifndef CLOG_SINK_FILE_H
#define CLOG_SINK_FILE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "clog.h"

/* ============================================================================
 * File Backend Logging
 * ============================================================================ */

/**
 * @brief Log message to file backend
 * 
 * @param context_id Context identifier
 * @param level Log level
 * @param message Formatted message
 * 
 * @return CLOG_RETURN_OK on success, CLOG_RETURN_WRONG_PARAMETER/CLOG_RETURN_ERROR on failure
 */
clog_return_t clog_log_file(const char *context_id, clog_level_t level, const char *message);

#ifdef __cplusplus
}
#endif

#endif /* CLOG_SINK_FILE_H */
