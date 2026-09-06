/*
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#ifndef CFG_H
#define CFG_H

#include <stdint.h>
#include "cfg_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * On-disk binary layout
 *
 * File = header, global, settings (v2+), app_count x app,
 * override_count x override, lctx_count x lctx, back-to-back with no gaps.
 * Host-endian, local-machine only tool -- no cross-endian concern.
 * ============================================================================ */

#define CFG_MAGIC   0x43464731u /* "CFG1" */
#define CFG_VERSION 2u

#define CFG_DEFAULT_ADD_CLOGDETAILS_IN_MSG 1u
#define CFG_DEFAULT_SINK_FILE_MAX_SIZE_MB 5u
#define CFG_DEFAULT_SINK_FILE_ROTATE_COUNT 2u

/* Max entries per section. Inserting a NEW record beyond these limits
 * fails (existing records can still be updated in place). */
#define CFG_MAX_APP_COUNT      16u
#define CFG_MAX_OVERRIDE_COUNT 16u
#define CFG_MAX_LCTX_COUNT     16u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t app_count;
    uint32_t override_count;
    uint32_t lctx_count;
} cfg_header_t;

typedef struct {
    uint8_t sink;
    uint8_t level;
} cfg_global_t;

/* add_clogdetails_in_msg controls whether backend-specific APPID/CTXID detail
 * prefixes are emitted alongside the caller's message. sink_file_max_size_mb is
 * interpreted as megabytes by cfg-lib/commonlog-lib. reserved0 preserves the
 * current settings layout after removing the old internal-log-to-file switch. */
typedef struct {
    uint8_t add_clogdetails_in_msg;
    uint8_t reserved0;
    uint16_t sink_file_max_size_mb;
    uint16_t sink_file_rotate_count;
} cfg_settings_t;

typedef struct {
    clog_app_id_t app_id;
    uint8_t sink;
    uint8_t level;
} cfg_app_t;

typedef struct {
    clog_app_id_t app_id;
    clog_context_id_t context_id;
    uint8_t level;
    uint8_t _pad;
} cfg_override_t;

typedef struct {
    clog_context_id_t context_id;
    uint8_t level;
    uint8_t _pad;
} cfg_lctx_t;

typedef enum {
    CFG_OK          = 0,
    CFG_OK_UPDATED  = 1,
    CFG_OK_INSERTED = 2,
    CFG_OK_REMOVED  = 3,
    CFG_ERR         = -1,
    CFG_ERR_PARAM   = -2,
    CFG_ERR_LIMIT   = -3,
    CFG_ERR_NOT_FOUND = -4
} cfg_status_t;

#define CFG_STATUS_IS_OK(status) ((status) >= 0)

const char *cfg_status_str(cfg_status_t status);

cfg_status_t cfg_get_global(clog_backend_t *sink, clog_level_t *level);
cfg_status_t cfg_set_global(clog_backend_t sink, clog_level_t level);

cfg_status_t cfg_get_settings(cfg_settings_t *settings);
cfg_status_t cfg_set_settings(const cfg_settings_t *settings);

/* Raw per-app record lookup only; returns CFG_ERR when app_id has no entry. */
cfg_status_t cfg_get_app(clog_app_id_t app_id, clog_backend_t *sink, clog_level_t *level);

/* Resolved app sink/defaults: start from the global sink+level, then override
 * them with the app record when app_id has one. */
cfg_status_t cfg_get_resolved_app_sink(clog_app_id_t app_id, clog_backend_t *sink, clog_level_t *level);
cfg_status_t cfg_set_app(clog_app_id_t app_id, clog_backend_t sink, clog_level_t level);
cfg_status_t cfg_remove_app(clog_app_id_t app_id);

/* Resolved context precedence: start from the global default, then override it
 * with the app default, then the lctx default (if context_id has one), then
 * finally the app+context override. */
cfg_status_t cfg_get_resolved_context_level(clog_app_id_t app_id, clog_context_id_t context_id, clog_level_t *level);
cfg_status_t cfg_set_app_context_override(clog_app_id_t app_id, clog_context_id_t context_id, clog_level_t level);
cfg_status_t cfg_remove_app_context_override(clog_app_id_t app_id, clog_context_id_t context_id);

cfg_status_t cfg_get_lctx(clog_context_id_t context_id, clog_level_t *level);
cfg_status_t cfg_set_lctx(clog_context_id_t context_id, clog_level_t level);
cfg_status_t cfg_remove_lctx(clog_context_id_t context_id);

cfg_status_t cfg_get_app_count(uint32_t *count);
cfg_status_t cfg_get_app_at(uint32_t index, clog_app_id_t *app_id, clog_backend_t *sink, clog_level_t *level);

cfg_status_t cfg_get_override_count(uint32_t *count);
cfg_status_t cfg_get_override_at(uint32_t index, clog_app_id_t *app_id, clog_context_id_t *context_id, clog_level_t *level);

cfg_status_t cfg_get_lctx_count(uint32_t *count);
cfg_status_t cfg_get_lctx_at(uint32_t index, clog_context_id_t *context_id, clog_level_t *level);

#ifdef __cplusplus
}
#endif

#endif /* CFG_H */
