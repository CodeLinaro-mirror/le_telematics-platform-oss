/*
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

/*
 * Internal context structure shared between clog.c and sink implementations.
 * Not installed; not part of the public API.
 */

#ifndef CLOG_INTERNAL_H
#define CLOG_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "clog.h"
#include <stddef.h>

#ifdef CLOG_WITH_DLT
#include <dlt/dlt.h>
#endif

#define CLOG_MAX_CTX_ID_LEN 16
#define CLOG_MAX_DESCRIPTION_LEN 256
#define CLOG_MAX_MESSAGE_LEN 4096
#define CLOG_MAX_DETAIL_LEN 64
#define CLOG_MAX_FORMATTED_MESSAGE_LEN \
    (CLOG_MAX_MESSAGE_LEN + (2 * CLOG_MAX_DETAIL_LEN) + 4)

/* GENIVI/COVESA DLT AppID and ContextID are each a 4-byte character array
 * (see DLT_ID_SIZE in dlt_common.h); app_id/context_id longer than this are
 * still accepted (truncated to CLOG_MAX_APP_ID_LEN/CLOG_MAX_CTX_ID_LEN as
 * before) but clog_warn_if_too_long() flags the overflow via clog_log_internal(). */
#define CLOG_DLT_ID_LEN 4

/**
 * @brief Internal context structure
 *
 * Backend-specific fields are always present (not #ifdef-guarded) so that
 * clog.c and every sink implementation agree on the same memory layout
 * regardless of which backends a given build enables.
 */
typedef struct {
    clog_context_id_t clog_context_id;    /* Numeric context ID used for optional backend detail formatting */
    char context_id[CLOG_MAX_CTX_ID_LEN];
    char description[CLOG_MAX_DESCRIPTION_LEN];
    int is_registered;

#ifdef CLOG_WITH_DLT
    DltContext *dlt_context;              /* DLT context handle */
#else
    void *dlt_context;
#endif
    uint16_t diag_ssid;                    /* DIAG Session ID */
    int diag_ssid_warned;                  /* 1 once the SSID-missing warning has fired for this context */
    char logcat_tag[CLOG_MAX_CTX_ID_LEN];  /* Logcat tag */
    char syslog_tag[CLOG_MAX_CTX_ID_LEN];  /* Syslog tag */
} clog_context_internal_t;

/**
 * @brief Library-internal diagnostic logger
 *
 * Used by clog.c and the sink implementations to report their own
 * init/deinit/error/trace diagnostics. Always writes to syslog, regardless
 * of which application-facing backend(s) are configured via configuration file.
 * Not part of the public API.
 */
void clog_log_internal(clog_level_t level, const char *format, ...) CLOG_PRINTF_FORMAT(2, 3);

/**
 * @brief Warn (via clog_log_internal) if value exceeds limit characters
 *
 * Non-fatal advisory used where a backend has a hard length convention
 * (e.g. DLT's 4-byte AppID/ContextID) but CLOG's own buffers are larger;
 * truncation still happens at the normal string copy all sites.
 */
void clog_warn_if_too_long(const char *label, const char *value, size_t limit);

/* Force the process to use syslog as a runtime fallback when the selected
 * backend cannot be initialized or loses its required registration state. */
void clog_enable_syslog_fallback(const char *reason);

/**
 * @brief Query whether runtime per-process debug tracing is enabled
 *
 * Reflects the CLOG_DEBUG_TRACE=1 environment variable, read once at
 * clog_register_app() time. Independent of the CLOG_WITH_DEBUG
 * compile-time flag - lets one app get full trace detail without
 * rebuilding libclog.so or affecting any other process linking it.
 */
int clog_debug_trace_enabled(void);

int clog_add_details_in_message_enabled(void);
const char *clog_get_registered_app_id(void);
clog_app_id_t clog_get_registered_numeric_app_id(void);
int clog_build_app_detail(char *buffer, size_t size);
int clog_build_app_tag(char *buffer, size_t size);
int clog_build_context_detail(const clog_context_internal_t *ctx,
                              clog_backend_t backend,
                              char *buffer,
                              size_t size);
int clog_build_backend_message(char *buffer,
                               size_t size,
                               clog_backend_t backend,
                               const clog_context_internal_t *ctx,
                               const char *message);

size_t clog_get_sink_file_max_size_bytes(void);
uint16_t clog_get_sink_file_rotate_count(void);

#ifdef __cplusplus
}
#endif

#endif /* CLOG_INTERNAL_H */
