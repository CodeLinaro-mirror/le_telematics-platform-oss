/*
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

/*
 * DLT Sink Implementation Header
 */

#ifndef CLOG_SINK_DLT_H
#define CLOG_SINK_DLT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "clog.h"

/* ============================================================================
 * DLT Backend Initialization & Cleanup
 * ============================================================================ */

/**
 * @brief Initialize DLT backend
 *
 * @param app_id Application ID
 * @param app_description Application description
 * @return CLOG_RETURN_OK on success, CLOG_RETURN_WRONG_PARAMETER/CLOG_RETURN_ERROR on failure
 */
clog_return_t clog_dlt_init(const char *app_id, const char *app_description);

/**
 * @brief Deinitialize DLT backend
 *
 * @return CLOG_RETURN_OK on success, CLOG_RETURN_ERROR on failure
 */
clog_return_t clog_dlt_deinit(void);

/**
 * @brief Register DLT context
 *
 * @param context_id Context ID
 * @param description Context description
 * @return Pointer to DltContext on success, NULL on failure
 */
void* clog_dlt_register_context(const char *context_id, const char *description);

/**
 * @brief Unregister DLT context
 *
 * @param dlt_context DLT context pointer
 * @return CLOG_RETURN_OK on success, CLOG_RETURN_WRONG_PARAMETER/CLOG_RETURN_ERROR on failure
 */
clog_return_t clog_dlt_unregister_context(void *dlt_context);

/**
 * @brief Register DLT context only if the given backend is CLOG_BACKEND_DLT
 *
 * @param backend Currently active backend
 * @param context_id Context ID
 * @param description Context description
 * @return Pointer to DltContext on success, NULL if backend isn't DLT or on failure
 */
void* clog_dlt_context_register_if_needed(clog_backend_t backend, const char *context_id, const char *description);

/**
 * @brief Unregister DLT context only if the given backend is CLOG_BACKEND_DLT
 *
 * @param backend Currently active backend
 * @param dlt_context DLT context pointer
 * @return CLOG_RETURN_OK on success, CLOG_RETURN_WRONG_PARAMETER/CLOG_RETURN_ERROR if backend isn't DLT, dlt_context is NULL, or on failure
 */
clog_return_t clog_dlt_context_unregister_if_needed(clog_backend_t backend, void *dlt_context);

/* ============================================================================
 * DLT Level Conversion
 * ============================================================================ */

/**
 * @brief Convert CLOG level to DLT level
 * 
 * @param level CLOG log level
 * @return DLT log level
 */
int clog_level_to_dlt_level(clog_level_t level);

/**
 * @brief Convert DLT log level to CLOG level
 *
 * @param dlt_level DLT log level
 * @return CLOG log level
 */
clog_level_t clog_level_from_dlt_level(int dlt_level);

/* ============================================================================
 * DLT Backend Logging
 * ============================================================================ */

/**
 * @brief Log message to DLT backend
 * 
 * @param ctx Internal context pointer
 * @param level Log level
 * @param message Formatted message
 * 
 * @return CLOG_RETURN_OK on success, CLOG_RETURN_WRONG_PARAMETER/CLOG_RETURN_ERROR on failure
 */
clog_return_t clog_log_dlt(void *ctx, clog_level_t level, const char *message);

#ifdef __cplusplus
}
#endif

#endif /* CLOG_SINK_DLT_H */
