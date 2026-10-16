/*
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

/*
 * Sink Implementation Master Header
 *
 * This header is installed for external users that only need utility helpers
 * such as clog_level_to_syslog_priority(). Keep it self-contained: recipe
 * sysroots do not ship the private sink-specific headers that live beside the
 * implementation sources.
 */

#ifndef CLOG_SINKIMPL_H
#define CLOG_SINKIMPL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "clog.h"

/* ============================================================================
 * Backend Entry Points
 * ============================================================================ */

clog_return_t clog_dlt_init(const char *app_id, const char *app_description);
clog_return_t clog_dlt_deinit(void);
void* clog_dlt_register_context(const char *context_id, const char *description);
clog_return_t clog_dlt_unregister_context(void *dlt_context);
void* clog_dlt_context_register_if_needed(clog_backend_t backend, const char *context_id, const char *description);
clog_return_t clog_dlt_context_unregister_if_needed(clog_backend_t backend, void *dlt_context);
int clog_level_to_dlt_level(clog_level_t level);
clog_level_t clog_level_from_dlt_level(int dlt_level);
clog_return_t clog_log_dlt(void *ctx, clog_level_t level, const char *message);

clog_return_t clog_diag_init(void);
clog_return_t clog_diag_deinit(void);
clog_return_t clog_diag_is_initialized(void);
int clog_level_to_diag_level(clog_level_t level);
clog_level_t clog_level_from_diag_level(int diag_level);
clog_return_t clog_log_diag(void *ctx, const char *file, int line, clog_level_t level, const char *message);

int clog_level_to_syslog_priority(clog_level_t level);
clog_level_t clog_level_from_syslog_priority(int priority);
clog_return_t clog_log_syslog(const char *context_id, clog_level_t level, const char *message);
clog_return_t clog_log_syslog_ctx(void *ctx, clog_level_t level, const char *message);
void clog_syslog_reset_ident(void);

int clog_level_to_android_priority(clog_level_t level);
clog_level_t clog_level_from_android_priority(int priority);
clog_return_t clog_log_logcat(const char *context_id, clog_level_t level, const char *message);
clog_return_t clog_log_logcat_ctx(void *ctx, clog_level_t level, const char *message);

clog_return_t clog_log_file(const char *context_id, clog_level_t level, const char *message);
clog_return_t clog_log_console(const char *context_id, clog_level_t level, const char *message);
clog_return_t clog_log_file_ctx(void *ctx, clog_level_t level, const char *message);
clog_return_t clog_log_console_ctx(void *ctx, clog_level_t level, const char *message);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get string representation of log level
 * 
 * @param level Log level
 * @return String representation (e.g., "INFO", "ERROR")
 */
const char* clog_level_to_string(clog_level_t level);

/**
 * @brief Get string representation of backend type
 * 
 * @param backend Backend type
 * @return String representation (e.g., "DLT", "syslog")
 */
const char* clog_backend_to_string(clog_backend_t backend);

/**
 * @brief Get string representation of a clog_return_t value
 *
 * @param ret Return value
 * @return String representation (e.g., "CLOG_RETURN_OK", "CLOG_RETURN_ERROR")
 */
const char* clog_return_to_string(clog_return_t ret);

/**
 * @brief Flush any pending log messages
 *
 * @return 0 on success, -1 on failure
 */
int clog_flush(void);

#ifdef __cplusplus
}
#endif

#endif /* CLOG_SINKIMPL_H */
