/*
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

/*
 * DIAG Sink Implementation (Combined: Init + Logging)
 */

#include "clog_sink_diag.h"
#include "clog.h"
#include "clog_sinkImpl.h"
#include "clog_internal.h"
#include <stdio.h>
#include <errno.h>
#include <pthread.h>

/* DIAG includes */
#ifdef CLOG_WITH_DIAG
#include <comdef.h>
#include "diag_lsm.h"
#include "msg.h"
#include "event.h"
#include "log.h"
#ifndef MSG_SSID_DIAG
#define MSG_SSID_DIAG 7
#endif
#endif

/* ============================================================================
 * Internal State & Configuration
 * ============================================================================ */

#include <stdint.h>

/* ============================================================================
 * DIAG Backend State
 * ============================================================================ */

#ifdef CLOG_WITH_DIAG
typedef struct {
    int initialized;                    /* DIAG LSM initialization state */
    uint32_t total_messages;            /* Total messages logged */
    uint32_t failed_messages;           /* Failed message count */
    pthread_mutex_t lock;               /* Thread safety */
} clog_diag_state_t;

static clog_diag_state_t g_clog_diag_state = {
    .initialized = 0,
    .total_messages = 0,
    .failed_messages = 0,
    .lock = PTHREAD_MUTEX_INITIALIZER
};
#endif

/* ============================================================================
 * DIAG Backend Initialization & Cleanup
 * ============================================================================ */

clog_return_t clog_diag_init(void)
{
#ifdef CLOG_WITH_DIAG
    boolean ok = false;
    static int mutex_initialized = 0;

    if (!mutex_initialized) {
        pthread_mutex_init(&g_clog_diag_state.lock, NULL);
        mutex_initialized = 1;
    }

    pthread_mutex_lock(&g_clog_diag_state.lock);

    if (g_clog_diag_state.initialized) {
        pthread_mutex_unlock(&g_clog_diag_state.lock);
        return CLOG_RETURN_OK;
    }

    ok = Diag_LSM_Init(NULL);
    if (!ok) {
        clog_log_internal(CLOG_LEVEL_ERROR, "[DIAG Backend] Diag_LSM_Init failed (errno=%d)", errno);
        pthread_mutex_unlock(&g_clog_diag_state.lock);
        return CLOG_RETURN_ERROR;
    }

    g_clog_diag_state.initialized = 1;
    g_clog_diag_state.total_messages = 0;
    g_clog_diag_state.failed_messages = 0;

    clog_log_internal(CLOG_LEVEL_INFO, "[DIAG Backend] LSM initialized successfully");

    pthread_mutex_unlock(&g_clog_diag_state.lock);
    return CLOG_RETURN_OK;
#else
    clog_log_internal(CLOG_LEVEL_ERROR, "[DIAG Backend] ERROR: DIAG backend not enabled (CLOG_WITH_DIAG not defined)");
    return CLOG_RETURN_ERROR;
#endif
}

clog_return_t clog_diag_deinit(void)
{
#ifdef CLOG_WITH_DIAG
    static int mutex_initialized = 0;

    if (!mutex_initialized) {
        pthread_mutex_init(&g_clog_diag_state.lock, NULL);
        mutex_initialized = 1;
    }

    pthread_mutex_lock(&g_clog_diag_state.lock);

    if (!g_clog_diag_state.initialized) {
        pthread_mutex_unlock(&g_clog_diag_state.lock);
        return CLOG_RETURN_OK;
    }

    Diag_LSM_DeInit();
    g_clog_diag_state.initialized = 0;

    clog_log_internal(CLOG_LEVEL_INFO, "[DIAG Backend] LSM deinitialized");

    pthread_mutex_unlock(&g_clog_diag_state.lock);
    return CLOG_RETURN_OK;
#else
    return CLOG_RETURN_ERROR;
#endif
}

clog_return_t clog_diag_is_initialized(void)
{
#ifdef CLOG_WITH_DIAG
    int result = 0;
    static int mutex_initialized = 0;

    if (!mutex_initialized) {
        pthread_mutex_init(&g_clog_diag_state.lock, NULL);
        mutex_initialized = 1;
    }

    pthread_mutex_lock(&g_clog_diag_state.lock);
    result = g_clog_diag_state.initialized;
    pthread_mutex_unlock(&g_clog_diag_state.lock);
    return result ? CLOG_RETURN_TRUE : CLOG_RETURN_OK;
#else
    return CLOG_RETURN_OK;
#endif
}

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

int clog_diag_is_init_internal(void)
{
#ifdef CLOG_WITH_DIAG
    return g_clog_diag_state.initialized;
#else
    return 0;
#endif
}

void clog_diag_update_stats(int success)
{
#ifdef CLOG_WITH_DIAG
    static int mutex_initialized = 0;

    if (!mutex_initialized) {
        pthread_mutex_init(&g_clog_diag_state.lock, NULL);
        mutex_initialized = 1;
    }

    pthread_mutex_lock(&g_clog_diag_state.lock);
    g_clog_diag_state.total_messages++;
    if (!success) {
        g_clog_diag_state.failed_messages++;
    }
    pthread_mutex_unlock(&g_clog_diag_state.lock);
#else
    (void)success;
#endif
}

/* ============================================================================
 * DIAG Level Conversion
 * ============================================================================ */

int clog_level_to_diag_level(clog_level_t level)
{
#ifdef CLOG_WITH_DIAG
    switch (level) {
        case CLOG_LEVEL_OFF:
        case CLOG_LEVEL_DEBUG:
        case CLOG_LEVEL_VERBOSE:
            return CLOG_DIAG_LEVEL_LOW;

        case CLOG_LEVEL_INFO:
            return CLOG_DIAG_LEVEL_MED;

        case CLOG_LEVEL_WARN:
            return CLOG_DIAG_LEVEL_HIGH;

        case CLOG_LEVEL_ERROR:
            return CLOG_DIAG_LEVEL_ERROR;

        case CLOG_LEVEL_FATAL:
            return CLOG_DIAG_LEVEL_FATAL;

        default:
            return CLOG_DIAG_LEVEL_MED;
    }
#else
    (void)level;
    return 0;
#endif
}

clog_level_t clog_level_from_diag_level(int diag_level)
{
#ifdef CLOG_WITH_DIAG
    switch (diag_level) {
        case CLOG_DIAG_LEVEL_LOW:    return CLOG_LEVEL_DEBUG;
        case CLOG_DIAG_LEVEL_MED:    return CLOG_LEVEL_INFO;
        case CLOG_DIAG_LEVEL_HIGH:   return CLOG_LEVEL_WARN;
        case CLOG_DIAG_LEVEL_ERROR:  return CLOG_LEVEL_ERROR;
        case CLOG_DIAG_LEVEL_FATAL:  return CLOG_LEVEL_FATAL;
        default:                    return CLOG_LEVEL_INFO;
    }
#else
    (void)diag_level;
    return CLOG_LEVEL_INFO;
#endif
}

/* ============================================================================
 * DIAG Backend Logging
 * ============================================================================ */

clog_return_t clog_log_diag(void *ctx, const char *file, int line, clog_level_t level, const char *message)
{
#ifdef CLOG_WITH_DIAG
    int diag_level = 0;
    uint16_t diag_ssid_to_use = 0U;
    int success = 0;
    char diag_message[CLOG_MAX_FORMATTED_MESSAGE_LEN] = {0};
    const char *message_to_diag = message;
    clog_context_internal_t *internal_ctx = NULL;

    if (ctx == NULL || message == NULL) {
        return CLOG_RETURN_WRONG_PARAMETER;
    }

    internal_ctx = (clog_context_internal_t*)ctx;

    /* Check if DIAG backend is initialized */
    if (!clog_diag_is_init_internal()) {
        clog_log_internal(CLOG_LEVEL_ERROR,
                "[DIAG Backend] DIAG app not initialized for context '%s'; falling back to syslog",
                internal_ctx->context_id);
        clog_diag_update_stats(0);
        clog_enable_syslog_fallback("DIAG initialization failed");
        return clog_log_syslog_ctx(internal_ctx, level, message);
    }

    diag_message[0] = '\0';
    if (clog_build_backend_message(diag_message,
                                   sizeof(diag_message),
                                   CLOG_BACKEND_DIAG,
                                   internal_ctx,
                                   message)) {
        message_to_diag = diag_message;
    }

    /* Convert CLOG level to DIAG level */
    diag_level = clog_level_to_diag_level(level);
    diag_ssid_to_use = internal_ctx->diag_ssid;

    if (internal_ctx->diag_ssid == CLOG_DIAG_SSID_NOT_AVAILABLE) {
        if (!internal_ctx->diag_ssid_warned) {
            clog_log_internal(CLOG_LEVEL_WARN,
                    "[DIAG Backend] WARNING: DIAG SSID not specified for context '%s'; using default SSID %u",
                    internal_ctx->context_id,
                    (unsigned int)MSG_SSID_DIAG);
            internal_ctx->diag_ssid_warned = 1;
        }

        diag_ssid_to_use = (uint16_t)MSG_SSID_DIAG;
    }

#ifdef CLOG_WITH_DEBUG
    /* Trace DIAG delivery details */
    clog_log_internal(CLOG_LEVEL_DEBUG, "[DIAG Backend] Context: %s | SSID: %u | Level: %d | Message: %s",
            internal_ctx->context_id,
            (unsigned int)diag_ssid_to_use,
            diag_level,
            message_to_diag);
#else
    if (clog_debug_trace_enabled()) {
        clog_log_internal(CLOG_LEVEL_DEBUG, "[DIAG Backend] Context: %s | SSID: %u | Level: %d | Message: %s",
                internal_ctx->context_id,
                (unsigned int)diag_ssid_to_use,
                diag_level,
                message_to_diag);
    }
#endif

    /* Log to DIAG using the caller-registered SSID (msg_sprintf bypasses
     * MSG_SPRINTF_1's compile-time SSID requirement - see msg_const_type) */
    {
        static const uint32 diag_level_mask[] = {
            MSG_LEGACY_LOW, MSG_LEGACY_MED, MSG_LEGACY_HIGH,
            MSG_LEGACY_ERROR, MSG_LEGACY_FATAL
        };
        msg_const_type msg_const = {0};

        if (diag_level < 0 || (size_t)diag_level >= (sizeof(diag_level_mask) / sizeof(diag_level_mask[0]))) {
            clog_log_internal(CLOG_LEVEL_ERROR, "[DIAG Backend] Unknown DIAG level: %d", diag_level);
            success = 0;
        } else {
            msg_const.desc.line = (uint16)line;
            msg_const.desc.ss_id = diag_ssid_to_use;
            msg_const.desc.ss_mask = diag_level_mask[diag_level];
            msg_const.fmt = "%s";
            msg_const.fname = file;

            msg_sprintf(&msg_const, message_to_diag);
            success = 1;
        }
    }

    clog_diag_update_stats(success);
    if (!success) {
        clog_log_internal(CLOG_LEVEL_ERROR,
                "[DIAG Backend] Failed to emit DIAG log for context '%s'; falling back to syslog",
                internal_ctx->context_id);
        clog_enable_syslog_fallback("DIAG logging failed");
        return clog_log_syslog_ctx(internal_ctx, level, message);
    }

    return CLOG_RETURN_OK;
#else
    (void)ctx;
    (void)file;
    (void)line;
    (void)level;
    (void)message;
    clog_log_internal(CLOG_LEVEL_ERROR, "[DIAG Backend] ERROR: DIAG backend not enabled (CLOG_WITH_DIAG not defined)");
    return CLOG_RETURN_ERROR;
#endif
}
