/*
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

/*
 * DIAG Sink Implementation Header
 */

#ifndef CLOG_SINK_DIAG_H
#define CLOG_SINK_DIAG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "clog.h"
#include <stdint.h>

/* ============================================================================
 * DIAG Level Constants
 * ============================================================================ */

#define CLOG_DIAG_LEVEL_LOW    0
#define CLOG_DIAG_LEVEL_MED    1
#define CLOG_DIAG_LEVEL_HIGH   2
#define CLOG_DIAG_LEVEL_ERROR  3
#define CLOG_DIAG_LEVEL_FATAL  4

/* ============================================================================
 * DIAG Backend Initialization & Cleanup
 * ============================================================================ */

/**
 * @brief Initialize DIAG backend
 *
 * @return CLOG_RETURN_OK on success, CLOG_RETURN_ERROR on failure
 */
clog_return_t clog_diag_init(void);

/**
 * @brief Deinitialize DIAG backend
 *
 * @return CLOG_RETURN_OK on success, CLOG_RETURN_ERROR on failure
 */
clog_return_t clog_diag_deinit(void);

/**
 * @brief Check if DIAG backend is initialized
 *
 * @return CLOG_RETURN_TRUE if initialized, CLOG_RETURN_OK if not
 */
clog_return_t clog_diag_is_initialized(void);

/* ============================================================================
 * DIAG Level Conversion
 * ============================================================================ */

/**
 * @brief Convert CLOG level to DIAG level
 * 
 * @param level CLOG log level
 * @return DIAG severity level
 */
int clog_level_to_diag_level(clog_level_t level);

/**
 * @brief Convert DIAG level to CLOG level
 *
 * @param diag_level DIAG severity level
 * @return CLOG log level
 */
clog_level_t clog_level_from_diag_level(int diag_level);

/* ============================================================================
 * DIAG Backend Logging
 * ============================================================================ */

/**
 * @brief Log message to DIAG backend
 *
 * @param ctx Internal context pointer
 * @param file Caller's source file name (__FILE__)
 * @param line Caller's source line number (__LINE__)
 * @param level Log level
 * @param message Formatted message
 *
 * @return CLOG_RETURN_OK on success, CLOG_RETURN_WRONG_PARAMETER/CLOG_RETURN_ERROR on failure
 */
clog_return_t clog_log_diag(void *ctx, const char *file, int line, clog_level_t level, const char *message);

#ifdef __cplusplus
}
#endif

#endif /* CLOG_SINK_DIAG_H */
