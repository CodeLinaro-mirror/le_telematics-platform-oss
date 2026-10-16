/*
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

/*
 * DLT Sink Implementation
 */

#include "clog_sink_dlt.h"
#include "clog_sinkImpl.h"
#include "clog_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* DLT includes */
#ifdef CLOG_WITH_DLT
#include <dlt/dlt.h>
#endif

/* ============================================================================
 * DLT Backend Initialization & Cleanup
 * ============================================================================ */

#ifdef CLOG_WITH_DLT
/* Tracks whether CLOG itself registered the DLT AppID (vs. reused one that
 * was already registered by someone else) so that clog_dlt_deinit() only
 * unregisters an AppID that CLOG actually owns. */
static int g_clog_dlt_app_registered_by_clog = 0;

static void clog_dlt_on_level_changed(char context_id[DLT_ID_SIZE], uint8_t log_level, uint8_t trace_status)
{
    char ctx_id_str[DLT_ID_SIZE + 1] = {0};
    (void)trace_status;
    memcpy(ctx_id_str, context_id, DLT_ID_SIZE);
    (void)clog_notify_log_level_changed(ctx_id_str, clog_level_from_dlt_level((int)log_level), CLOG_BACKEND_DLT);
}
#endif

clog_return_t clog_dlt_init(const char *app_id, const char *app_description)
{
#ifdef CLOG_WITH_DLT
    char existing_appid[DLT_ID_SIZE] = {0};

    if (app_id == NULL || app_description == NULL) {
        return CLOG_RETURN_WRONG_PARAMETER;
    }

    /* Check whether a DLT AppID is already registered in this process before
     * registering our own - if so, reuse it instead of overwriting it. */
    (void)dlt_get_appid(existing_appid);
    if (existing_appid[0] != '\0' || existing_appid[1] != '\0' ||
        existing_appid[2] != '\0' || existing_appid[3] != '\0') {
        clog_log_internal(CLOG_LEVEL_INFO,
            "[DLT Backend] DLT AppID already registered (%.4s); CLOG will reuse it instead of app_id=%s",
            existing_appid, app_id);
        g_clog_dlt_app_registered_by_clog = 0;
        return CLOG_RETURN_OK;
    }

    if (dlt_register_app(app_id, app_description) != DLT_RETURN_OK) {
        clog_log_internal(CLOG_LEVEL_ERROR, "[DLT Backend] ERROR: dlt_register_app failed for app_id=%s", app_id);
        return CLOG_RETURN_ERROR;
    }

    g_clog_dlt_app_registered_by_clog = 1;
    clog_log_internal(CLOG_LEVEL_INFO, "[DLT Backend] Initialized with app_id=%s", app_id);
    return CLOG_RETURN_OK;
#else
    (void)app_id;
    (void)app_description;
    clog_log_internal(CLOG_LEVEL_ERROR, "[DLT Backend] ERROR: DLT backend not enabled (CLOG_WITH_DLT not defined)");
    return CLOG_RETURN_ERROR;
#endif
}

clog_return_t clog_dlt_deinit(void)
{
#ifdef CLOG_WITH_DLT
    if (!g_clog_dlt_app_registered_by_clog) {
        clog_log_internal(CLOG_LEVEL_DEBUG,
            "[DLT Backend] Skipping DLT AppID unregister; CLOG did not register it");
        return CLOG_RETURN_OK;
    }

    DLT_UNREGISTER_APP();
    g_clog_dlt_app_registered_by_clog = 0;
    clog_log_internal(CLOG_LEVEL_INFO, "[DLT Backend] Deinitialized");
    return CLOG_RETURN_OK;
#else
    return CLOG_RETURN_ERROR;
#endif
}

void* clog_dlt_register_context(const char *context_id, const char *description)
{
#ifdef CLOG_WITH_DLT
    if (context_id == NULL || description == NULL) {
        return NULL;
    }

    /* Dynamically allocate DltContext */
    DltContext *dlt_ctx = (DltContext *)malloc(sizeof(DltContext));
    if (dlt_ctx == NULL) {
        return NULL;
    }

    /* Register context with DLT, wiring in the log-level-changed callback so
     * externally-triggered level changes (dlt-control/DLT Viewer/daemon) reach
     * CLOG's own registered callback via clog_dlt_on_level_changed(). */
    DltReturnValue rc = dlt_register_context_llccb(dlt_ctx, context_id, description, clog_dlt_on_level_changed);
    if (rc != DLT_RETURN_OK) {
        clog_log_internal(CLOG_LEVEL_ERROR,
            "[DLT Backend] ERROR: dlt_register_context_llccb failed for context_id=%s (rc=%d)", context_id, (int)rc);
        free(dlt_ctx);
        return NULL;
    }
    clog_log_internal(CLOG_LEVEL_DEBUG, "[DLT Backend] Registered context: %s", context_id);

    return (void*)dlt_ctx;
#else
    (void)context_id;
    (void)description;
    return NULL;
#endif
}

clog_return_t clog_dlt_unregister_context(void *dlt_context)
{
#ifdef CLOG_WITH_DLT
    if (dlt_context == NULL) {
        return CLOG_RETURN_WRONG_PARAMETER;
    }

    DltContext *dlt_ctx = (DltContext *)dlt_context;
    DLT_UNREGISTER_CONTEXT(*dlt_ctx);
    free(dlt_ctx);
    clog_log_internal(CLOG_LEVEL_DEBUG, "[DLT Backend] Unregistered context");
    return CLOG_RETURN_OK;
#else
    (void)dlt_context;
    return CLOG_RETURN_ERROR;
#endif
}

void* clog_dlt_context_register_if_needed(clog_backend_t backend, const char *context_id, const char *description)
{
#ifdef CLOG_WITH_DLT
    if (backend != CLOG_BACKEND_DLT) {
        return NULL;
    }
    return clog_dlt_register_context(context_id, description);
#else
    (void)backend;
    (void)context_id;
    (void)description;
    return NULL;
#endif
}

clog_return_t clog_dlt_context_unregister_if_needed(clog_backend_t backend, void *dlt_context)
{
#ifdef CLOG_WITH_DLT
    if (backend != CLOG_BACKEND_DLT || dlt_context == NULL) {
        return CLOG_RETURN_WRONG_PARAMETER;
    }
    return clog_dlt_unregister_context(dlt_context);
#else
    (void)backend;
    (void)dlt_context;
    return CLOG_RETURN_ERROR;
#endif
}

/* ============================================================================
 * DLT Level Conversion
 * ============================================================================ */

int clog_level_to_dlt_level(clog_level_t level)
{
#ifdef CLOG_WITH_DLT
    switch (level) {
        case CLOG_LEVEL_FATAL:      return DLT_LOG_FATAL;
        case CLOG_LEVEL_ERROR:      return DLT_LOG_ERROR;
        case CLOG_LEVEL_WARN:       return DLT_LOG_WARN;
        case CLOG_LEVEL_INFO:       return DLT_LOG_INFO;
        case CLOG_LEVEL_DEBUG:      return DLT_LOG_DEBUG;
        case CLOG_LEVEL_VERBOSE:    return DLT_LOG_VERBOSE;
        case CLOG_LEVEL_OFF:        
        default:                    return DLT_LOG_INFO;
    }
#else
    (void)level;
    return 0;
#endif
}

/* Convert a DLT log level back to a CLOG level (inverse of clog_level_to_dlt_level) */
clog_level_t clog_level_from_dlt_level(int dlt_level)
{
#ifdef CLOG_WITH_DLT
    switch (dlt_level) {
        case DLT_LOG_OFF:     return CLOG_LEVEL_OFF;
        case DLT_LOG_FATAL:   return CLOG_LEVEL_FATAL;
        case DLT_LOG_ERROR:   return CLOG_LEVEL_ERROR;
        case DLT_LOG_WARN:    return CLOG_LEVEL_WARN;
        case DLT_LOG_INFO:    return CLOG_LEVEL_INFO;
        case DLT_LOG_DEBUG:   return CLOG_LEVEL_DEBUG;
        case DLT_LOG_VERBOSE: return CLOG_LEVEL_VERBOSE;
        default:              return CLOG_LEVEL_INFO;
    }
#else
    (void)dlt_level;
    return CLOG_LEVEL_INFO;
#endif
}

/* ============================================================================
 * DLT Backend Logging
 * ============================================================================ */

clog_return_t clog_log_dlt(void *ctx, clog_level_t level, const char *message)
{
#ifdef CLOG_WITH_DLT
    if (ctx == NULL || message == NULL) {
        return CLOG_RETURN_WRONG_PARAMETER;
    }

    int dlt_level = clog_level_to_dlt_level(level);

    clog_context_internal_t *internal_ctx = (clog_context_internal_t*)ctx;

    /* Check if dlt_context pointer is valid */
    if (internal_ctx->dlt_context == NULL) {
        clog_log_internal(CLOG_LEVEL_ERROR,
                "[DLT Backend] DLT app/context not initialized for context '%s'; falling back to syslog",
                internal_ctx->context_id);
        clog_enable_syslog_fallback("DLT app or context initialization failed");
        return clog_log_syslog_ctx(internal_ctx, level, message);
    }

#ifdef CLOG_WITH_DEBUG
    /* Trace which DLT Context ID is being used */
    clog_log_internal(CLOG_LEVEL_DEBUG, "[DLT Backend] Using DLT Context ID: %s | Level: %s | Message: %s",
                       internal_ctx->context_id,
                       clog_level_to_string(level),
                       message);
#else
    if (clog_debug_trace_enabled()) {
        clog_log_internal(CLOG_LEVEL_DEBUG, "[DLT Backend] Using DLT Context ID: %s | Level: %s | Message: %s",
                           internal_ctx->context_id,
                           clog_level_to_string(level),
                           message);
    }
#endif

    /* Use the DltContext pointer with DLT_LOG macro */
    DLT_LOG(*(DltContext*)internal_ctx->dlt_context, (DltLogLevelType)dlt_level, DLT_STRING(message));
    return CLOG_RETURN_OK;
#else
    (void)ctx;
    (void)level;
    (void)message;
    clog_log_internal(CLOG_LEVEL_ERROR, "[DLT Backend] ERROR: DLT backend not enabled (CLOG_WITH_DLT not defined)");
    return CLOG_RETURN_ERROR;
#endif
}
