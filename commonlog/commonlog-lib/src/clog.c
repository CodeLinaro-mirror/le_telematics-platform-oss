/*
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include "clog.h"
#include "cfg.h"
#include "clog_internal.h"
#include "clog_sinkImpl.h"
#include "clog_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include <syslog.h>
#ifdef USE_GLIB
#define __packed __attribute__((packed))
#include <glib.h>
#define strlcpy g_strlcpy
#endif

/* DLT includes */
#ifdef CLOG_WITH_DLT
#include <dlt/dlt.h>
#endif

/* Android logcat includes */
#ifdef CLOG_WITH_LOGCAT
#include <android/log.h>
#endif

/* DIAG includes - now handled by sink implementation */

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define CLOG_MAX_CONTEXTS 100
#define CLOG_INITIAL_CONTEXT_CAPACITY 8
#define CLOG_MAX_APP_ID_LEN 16

/* ============================================================================
 * Compiler Optimization Macros
 * ============================================================================ */

/**
 * @brief Branch prediction hints for compiler optimization
 *
 * These macros help the CPU predict branches correctly:
 * - likely(x): Condition is expected to be true (common case)
 * - unlikely(x): Condition is expected to be false (rare case)
 *
 * Improves performance by reducing pipeline stalls and mispredictions.
 * Supported on GCC/Clang; no-op on other compilers.
 */
#ifdef __GNUC__
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x)   (x)
#define unlikely(x) (x)
#endif

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/* clog_context_internal_t is shared with the sink implementations via
 * clog_internal.h so that all translation units agree on its layout
 * regardless of which backends are compiled in. */

/**
 * @brief Internal CLOG state
 */
typedef struct {
    int initialized;           /* App registration completed */
    int backends_initialized;  /* Backend API actually initialized */
    clog_backend_t backend;
    int fallback_to_syslog;
    char app_id[CLOG_MAX_APP_ID_LEN];
    char app_description[CLOG_MAX_DESCRIPTION_LEN];
    clog_context_internal_t **contexts;  /* array of heap-allocated context pointers */
    int num_contexts;          /* number of live entries */
    int contexts_capacity;     /* allocated slots in the `contexts` array */
    clog_level_t global_log_level;
    int add_clogdetails_in_msg;
    int enabled;
    pthread_mutex_t lock;
    size_t sink_file_max_size_bytes;
    uint16_t sink_file_rotate_count;
    clog_app_id_t clog_app_id;  /* Numeric application ID used for optional backend detail formatting */
    int callback_enabled;  /* Log-level-changed callback registration state */
} clog_state_t;

/* ============================================================================
 * Global State
 * ============================================================================ */

static clog_state_t g_clog_state = {
    .initialized = 0,
    .backends_initialized = 0,
    .backend = CLOG_BACKEND_CONSOLE,
    .fallback_to_syslog = 0,
    .app_id = {0},
    .app_description = {0},
    .contexts = NULL,
    .num_contexts = 0,
    .contexts_capacity = 0,
    .global_log_level = CLOG_LEVEL_INFO,
    .add_clogdetails_in_msg = (int)CFG_DEFAULT_ADD_CLOGDETAILS_IN_MSG,
    .enabled = 1,
    .sink_file_max_size_bytes = (size_t)CFG_DEFAULT_SINK_FILE_MAX_SIZE_MB * 1024u * 1024u,
    .sink_file_rotate_count = (uint16_t)CFG_DEFAULT_SINK_FILE_ROTATE_COUNT,
    .clog_app_id = 0,  /* No numeric app ID set */
    .callback_enabled = 0  /* Log-level-changed callback not registered */
};

/* Auto-cleanup state */
static int g_auto_cleanup_enabled = 0;
static int g_auto_cleanup_registered = 0;

/* Runtime opt-in for verbose per-message trace logging (CLOG_DEBUG_TRACE=1
 * in this process's environment). Independent of the CLOG_WITH_DEBUG
 * compile-time flag - lets one misbehaving app get full trace detail
 * without rebuilding libclog.so or affecting any other linked process. */
static int g_clog_debug_trace_enabled = 0;

/* clog_register_library() state - tracks whether CLOG itself auto-registered
 * the app (vs. reusing an app-level registration made by someone else), and
 * how many libraries currently depend on that registration. Guarded by its
 * own lock, usable before g_clog_state.lock is initialized. */
static pthread_mutex_t g_clog_library_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_clog_app_registered_by_library = 0;
static int g_clog_library_refcount = 0;

/* Log-level-changed callback state */
static clog_log_level_changed_callback_t g_log_level_changed_callback = NULL;
static int g_callback_registered = 0;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/* Global variables exposed for sink implementation */
const char *g_clog_app_id;

/**
 * @brief Library-internal diagnostic logger (declared in clog_internal.h)
 *
 * Reports the library's own init/deinit/error/trace diagnostics via syslog
 * only, always prefixed with CLOG-INTERNAL.
 */
static const char *clog_internal_level_to_str(clog_level_t level)
{
    switch (level) {
        case CLOG_LEVEL_FATAL:   return "FATAL";
        case CLOG_LEVEL_ERROR:   return "ERROR";
        case CLOG_LEVEL_WARN:    return "WARN";
        case CLOG_LEVEL_INFO:    return "INFO";
        case CLOG_LEVEL_DEBUG:   return "DEBUG";
        case CLOG_LEVEL_VERBOSE: return "VERBOSE";
        case CLOG_LEVEL_OFF:
        default:                 return "INFO";
    }
}

void clog_log_internal(clog_level_t level, const char *format, ...)
{
    va_list args;
    char message[CLOG_MAX_MESSAGE_LEN] = {0};
    char prefixed_message[CLOG_MAX_MESSAGE_LEN + 64] = {0};
    int priority;
    int rc;

    if (format == NULL) {
        return;
    }

    va_start(args, format);
    rc = vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    if (rc < 0) {
        return;
    }

    switch (level) {
        case CLOG_LEVEL_FATAL:   priority = LOG_CRIT;    break;
        case CLOG_LEVEL_ERROR:   priority = LOG_ERR;     break;
        case CLOG_LEVEL_WARN:    priority = LOG_WARNING; break;
        case CLOG_LEVEL_INFO:    priority = LOG_INFO;    break;
        case CLOG_LEVEL_DEBUG:   priority = LOG_DEBUG;   break;
        case CLOG_LEVEL_VERBOSE: priority = LOG_DEBUG;   break;
        case CLOG_LEVEL_OFF:
        default:                 priority = LOG_INFO;    break;
    }

    if (snprintf(prefixed_message,
                 sizeof(prefixed_message),
                 "CLOG-INTERNAL [%s] %s",
                 clog_internal_level_to_str(level),
                 message) < 0) {
        return;
    }

    syslog(priority, "%s", prefixed_message);
}

/**
 * @brief Warn (via clog_log_internal) if value exceeds limit characters
 *        (declared in clog_internal.h)
 */
void clog_warn_if_too_long(const char *label, const char *value, size_t limit)
{
    if (label == NULL || value == NULL) {
        return;
    }

    if (strlen(value) > limit) {
        clog_log_internal(CLOG_LEVEL_WARN,
            "CLOG: %s '%s' exceeds %zu-character limit; will be truncated where used",
            label, value, limit);
    }
}

void clog_enable_syslog_fallback(const char *reason)
{
    if (!g_clog_state.fallback_to_syslog) {
        if (reason != NULL) {
            clog_log_internal(CLOG_LEVEL_ERROR,
                "CLOG: %s; switching to syslog fallback sink", reason);
        } else {
            clog_log_internal(CLOG_LEVEL_ERROR,
                "CLOG: switching to syslog fallback sink");
        }
    }

    g_clog_state.fallback_to_syslog = 1;
}

static clog_backend_t clog_get_active_backend(void)
{
    return g_clog_state.fallback_to_syslog ? CLOG_BACKEND_SYSLOG : g_clog_state.backend;
}

int clog_add_details_in_message_enabled(void)
{
    return g_clog_state.add_clogdetails_in_msg;
}

const char *clog_get_registered_app_id(void)
{
    if (g_clog_state.app_id[0] == '\0') {
        return "-";
    }

    return g_clog_state.app_id;
}

clog_app_id_t clog_get_registered_numeric_app_id(void)
{
    return g_clog_state.clog_app_id;
}

int clog_build_app_detail(char *buffer, size_t size)
{
    int rc = 0;

    if (buffer == NULL || size == 0u) {
        return 0;
    }

    rc = snprintf(buffer,
                  size,
                  "[%u:%s]",
                  (unsigned int)clog_get_registered_numeric_app_id(),
                  clog_get_registered_app_id());
    if (rc < 0) {
        buffer[0] = '\0';
        return 0;
    }

    return 1;
}

int clog_build_app_tag(char *buffer, size_t size)
{
    int rc = 0;

    if (buffer == NULL || size == 0u) {
        return 0;
    }

    if (clog_add_details_in_message_enabled()) {
        return clog_build_app_detail(buffer, size);
    }

    rc = snprintf(buffer, size, "%s", clog_get_registered_app_id());
    if (rc < 0) {
        buffer[0] = '\0';
        return 0;
    }

    return 1;
}

static const char *clog_get_context_tag_for_backend(const clog_context_internal_t *ctx,
                                                    clog_backend_t backend)
{
    if (ctx == NULL) {
        return NULL;
    }

    switch (backend) {
        case CLOG_BACKEND_SYSLOG:
            if (ctx->syslog_tag[0] != '\0') {
                return ctx->syslog_tag;
            }
            break;

        case CLOG_BACKEND_LOGCAT:
            if (ctx->logcat_tag[0] != '\0') {
                return ctx->logcat_tag;
            }
            break;

        case CLOG_BACKEND_DIAG:
        case CLOG_BACKEND_CONSOLE:
        case CLOG_BACKEND_FILE:
        case CLOG_BACKEND_DLT:
        default:
            break;
    }

    return ctx->context_id;
}

int clog_build_context_detail(const clog_context_internal_t *ctx,
                              clog_backend_t backend,
                              char *buffer,
                              size_t size)
{
    const char *tag = NULL;
    int rc = 0;

    if (ctx == NULL || buffer == NULL || size == 0u) {
        return 0;
    }

    tag = clog_get_context_tag_for_backend(ctx, backend);
    if (tag == NULL) {
        buffer[0] = '\0';
        return 0;
    }

    rc = snprintf(buffer,
                  size,
                  "[%u:%s]",
                  (unsigned int)ctx->clog_context_id,
                  tag);
    if (rc < 0) {
        buffer[0] = '\0';
        return 0;
    }

    return 1;
}

int clog_build_backend_message(char *buffer,
                               size_t size,
                               clog_backend_t backend,
                               const clog_context_internal_t *ctx,
                               const char *message)
{
    int rc = 0;

    if (buffer == NULL || size == 0u || message == NULL) {
        return 0;
    }

    if (message[0] == '\0') {
        buffer[0] = '\0';
        return 1;
    }

    if (!clog_add_details_in_message_enabled() || ctx == NULL || backend == CLOG_BACKEND_DLT) {
        rc = snprintf(buffer, size, "%s", message);
        if (rc < 0) {
            buffer[0] = '\0';
            return 0;
        }
        return 1;
    }

    {
        char context_detail[CLOG_MAX_DETAIL_LEN] = {0};

        if (!clog_build_context_detail(ctx, backend, context_detail, sizeof(context_detail))) {
            rc = snprintf(buffer, size, "%s", message);
            if (rc < 0) {
                buffer[0] = '\0';
                return 0;
            }
            return 1;
        }

        switch (backend) {
            case CLOG_BACKEND_SYSLOG:
            case CLOG_BACKEND_LOGCAT:
                rc = snprintf(buffer, size, "%s %s", context_detail, message);
                break;

            case CLOG_BACKEND_DIAG:
            case CLOG_BACKEND_CONSOLE:
            case CLOG_BACKEND_FILE:
            {
                char app_detail[CLOG_MAX_DETAIL_LEN] = {0};

                if (!clog_build_app_detail(app_detail, sizeof(app_detail))) {
                    rc = snprintf(buffer, size, "%s", message);
                    break;
                }

                rc = snprintf(buffer, size, "%s %s %s", app_detail, context_detail, message);
                break;
            }

            case CLOG_BACKEND_DLT:
            default:
                rc = snprintf(buffer, size, "%s", message);
                break;
        }
    }

    if (rc < 0) {
        buffer[0] = '\0';
        return 0;
    }

    return 1;
}

/**
 * @brief Find context by ID
 */
static clog_context_internal_t* clog_find_context(const char *context_id)
{
    int i;

    for (i = 0; i < g_clog_state.num_contexts; i++) {
        if (g_clog_state.contexts[i]->is_registered &&
            strcmp(g_clog_state.contexts[i]->context_id, context_id) == 0) {
            return g_clog_state.contexts[i];
        }
    }

    return NULL;
}

/**
 * @brief Check if message should be logged based on level
 *
 * Resolved thresholding is handled later in clog_log_dispatch().
 */
static int clog_should_log(clog_level_t level)
{
    if (!g_clog_state.enabled) {
        return 0;
    }

    if (level == CLOG_LEVEL_OFF) {
        return 0;
    }

    return 1;  /* Level filtering is done per-backend in clog_log_dispatch */
}

/**
 * @brief Get the resolved level for a context, falling back to the app level
 */
static clog_level_t clog_get_log_level(const clog_context_internal_t *ctx)
{
    clog_level_t level = g_clog_state.global_log_level;

    if (ctx == NULL) {
        return level;
    }

    if (cfg_get_resolved_context_level(g_clog_state.clog_app_id,
                                       ctx->clog_context_id,
                                       &level) != CFG_OK) {
        return g_clog_state.global_log_level;
    }

    return level;
}

static void clog_apply_runtime_settings(const cfg_settings_t *settings)
{
    g_clog_state.add_clogdetails_in_msg = settings->add_clogdetails_in_msg ? 1 : 0;
    g_clog_state.sink_file_max_size_bytes =
        (size_t)settings->sink_file_max_size_mb * 1024u * 1024u;
    g_clog_state.sink_file_rotate_count = settings->sink_file_rotate_count;
}

size_t clog_get_sink_file_max_size_bytes(void)
{
    return g_clog_state.sink_file_max_size_bytes;
}

uint16_t clog_get_sink_file_rotate_count(void)
{
    return g_clog_state.sink_file_rotate_count;
}

/**
 * @brief Auto-cleanup handler called by atexit()
 * Forward declaration
 */
static void clog_auto_cleanup_handler(void);

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

/**
 * @brief Internal initialization function
 */
static clog_return_t clog_init_internal(clog_backend_t backend,
                              const char *app_id, const char *app_description)
{
    if (g_clog_state.initialized) {
        return CLOG_RETURN_ERROR;  /* Already initialized */
    }

    if (app_id == NULL || app_description == NULL) {
        return CLOG_RETURN_WRONG_PARAMETER;
    }

    pthread_mutex_init(&g_clog_state.lock, NULL);

    g_clog_state.backend = backend;
    g_clog_state.fallback_to_syslog = 0;

    (void)strlcpy(g_clog_state.app_id, app_id, sizeof(g_clog_state.app_id));
    (void)strlcpy(g_clog_state.app_description, app_description, sizeof(g_clog_state.app_description));

    /* Initialize the backend synchronously, in the same call as app registration */
    switch (backend) {
        case CLOG_BACKEND_DLT:
#ifdef CLOG_WITH_DLT
        {
            char dlt_app_id[CLOG_DLT_ID_LEN + 1] = {0};
            const char *dlt_app_id_to_use;

            dlt_app_id_to_use = g_clog_state.app_id;
            if (clog_util_build_dlt_app_id(g_clog_state.app_id,
                                           dlt_app_id,
                                           sizeof(dlt_app_id)) == CLOG_RETURN_OK &&
                dlt_app_id[0] != '\0') {
                dlt_app_id_to_use = dlt_app_id;
            }

            /* Initialize DLT backend via sink implementation using a 4-character DLT AppID. */
            if (clog_dlt_init(dlt_app_id_to_use, g_clog_state.app_description) != 0) {
                clog_enable_syslog_fallback("Failed to initialize DLT backend");
            }
        }
#else
            clog_enable_syslog_fallback("DLT backend requested but not built in");
#endif
            break;

        case CLOG_BACKEND_DIAG:
#ifdef CLOG_WITH_DIAG
            /* Initialize DIAG backend via sink implementation */
            if (clog_diag_init() != 0) {
                clog_enable_syslog_fallback("Failed to initialize DIAG backend");
            }
#else
            clog_enable_syslog_fallback("DIAG backend requested but not built in");
#endif
            break;

        case CLOG_BACKEND_SYSLOG:
#ifdef CLOG_WITH_SYSLOG
            /* openlog() ident depends on add_clogdetails_in_msg, so the
             * syslog sink selects it at emit time after runtime settings load. */
#endif
            break;

        case CLOG_BACKEND_LOGCAT:
#ifdef CLOG_WITH_LOGCAT
            /* Android logcat initialization (if needed) */
#endif
            break;

        case CLOG_BACKEND_CONSOLE:
        case CLOG_BACKEND_FILE:
            /* No special initialization needed */
            break;

        default:
            break;
    }

    g_clog_state.initialized = 1;
    g_clog_state.backends_initialized = 1;

    {
        const char *trace_env = getenv("CLOG_DEBUG_TRACE");
        g_clog_debug_trace_enabled = (trace_env != NULL && strcmp(trace_env, "1") == 0);
    }

    return CLOG_RETURN_OK;
}

clog_return_t clog_register_app(clog_app_id_t clog_app_id, const char *app_id, const char *app_description)
{
    clog_backend_t log_sink = CLOG_BACKEND_CONSOLE;
    clog_level_t log_level = CLOG_LEVEL_INFO;
    cfg_settings_t settings = {0};
    clog_return_t result;

    if (app_id == NULL || app_description == NULL) {
        return CLOG_RETURN_WRONG_PARAMETER;
    }

    if (cfg_get_settings(&settings) != CFG_OK) {
        clog_log_internal(CLOG_LEVEL_ERROR, "CLOG: Failed to resolve runtime settings");
        return CLOG_RETURN_ERROR;
    }

    if (cfg_get_resolved_app_sink(clog_app_id, &log_sink, &log_level) != CFG_OK) {
        clog_log_internal(CLOG_LEVEL_ERROR, "CLOG: Failed to resolve app sink/level for app_id=%u", clog_app_id);
        return CLOG_RETURN_ERROR;
    }

    result = clog_init_internal(log_sink, app_id, app_description);
    if (result == CLOG_RETURN_OK) {
        g_clog_state.clog_app_id = clog_app_id;
        g_clog_state.global_log_level = log_level;
        clog_apply_runtime_settings(&settings);

        if (log_sink == CLOG_BACKEND_DLT || log_sink == CLOG_BACKEND_DIAG) {
            clog_log_internal(CLOG_LEVEL_INFO,
                               "CLOG: Registered app (app_id=%s, description=%s, log_sink=%d)",
                               app_id,
                               app_description,
                               log_sink);
        } else {
            clog_log_internal(CLOG_LEVEL_INFO,
                               "CLOG: Registered app (app_id=%s, description=%s, log_sink=%d, log_level=%d)",
                               app_id,
                               app_description,
                               log_sink,
                               log_level);
        }

        /* Automatically enable auto-cleanup on initialization */
        if (!g_auto_cleanup_registered) {
            if (atexit(clog_auto_cleanup_handler) == 0) {
                g_auto_cleanup_registered = 1;
                g_auto_cleanup_enabled = 1;
            }
        }
    }
    return result;
}

clog_return_t clog_register_library(const char *caller_tag)
{
    char auto_app_id[CLOG_MAX_APP_ID_LEN] = {0};
    char auto_app_description[CLOG_MAX_DESCRIPTION_LEN] = {0};
    clog_return_t result;
    const char *tag_str = (caller_tag != NULL) ? caller_tag : "unknown";

    pthread_mutex_lock(&g_clog_library_lock);

    if (g_clog_state.initialized) {
        /* App-level registration already done - by the application itself,
         * or by another library sharing this process. Just track our own
         * dependency on it and reuse it as-is. */
        g_clog_library_refcount++;
        pthread_mutex_unlock(&g_clog_library_lock);
        clog_log_internal(CLOG_LEVEL_INFO,
            "CLOG: clog_register_library() called by %s; reusing existing registration",
            tag_str);
        return CLOG_RETURN_OK;
    }

    if (clog_util_build_auto_identity(auto_app_id, sizeof(auto_app_id),
                                       auto_app_description, sizeof(auto_app_description)) != CLOG_RETURN_OK) {
        pthread_mutex_unlock(&g_clog_library_lock);
        return CLOG_RETURN_ERROR;
    }

    result = clog_register_app((clog_app_id_t)0, auto_app_id, auto_app_description);
    if (result == CLOG_RETURN_OK) {
        g_clog_app_registered_by_library = 1;
        g_clog_library_refcount++;
        clog_log_internal(CLOG_LEVEL_INFO,
            "CLOG: clog_register_library() called by %s; auto-registered app_id=%s (%s)",
            tag_str, auto_app_id, auto_app_description);
    }

    pthread_mutex_unlock(&g_clog_library_lock);
    return result;
}

clog_return_t clog_unregister_library(void)
{
    pthread_mutex_lock(&g_clog_library_lock);

    if (g_clog_library_refcount <= 0) {
        pthread_mutex_unlock(&g_clog_library_lock);
        return CLOG_RETURN_OK;
    }

    g_clog_library_refcount--;

    if (g_clog_library_refcount == 0 && g_clog_app_registered_by_library) {
        g_clog_app_registered_by_library = 0;
        pthread_mutex_unlock(&g_clog_library_lock);
        clog_log_internal(CLOG_LEVEL_INFO,
            "CLOG: clog_unregister_library() tearing down auto-registered app (last library detached)");
        clog_unregister_app();
        return CLOG_RETURN_OK;
    }

    pthread_mutex_unlock(&g_clog_library_lock);
    return CLOG_RETURN_OK;
}

clog_return_t clog_unregister_app(void)
{
    int i;
    clog_log_internal(CLOG_LEVEL_DEBUG, "CLOG: Cleanup called");
    if (!g_clog_state.initialized) {
        return CLOG_RETURN_ERROR;
    }

    pthread_mutex_lock(&g_clog_state.lock);

    /* Unregister all contexts and free their heap-allocated storage */
    for (i = 0; i < g_clog_state.num_contexts; i++) {
        if (g_clog_state.contexts[i]->is_registered) {
            clog_dlt_context_unregister_if_needed(g_clog_state.backend, g_clog_state.contexts[i]->dlt_context);
            g_clog_state.contexts[i]->dlt_context = NULL;
            g_clog_state.contexts[i]->is_registered = 0;
        }
        free(g_clog_state.contexts[i]);
    }
    free(g_clog_state.contexts);
    g_clog_state.contexts = NULL;
    g_clog_state.contexts_capacity = 0;

    /* Cleanup backend (only if it was actually initialized) */
    if (g_clog_state.backends_initialized) {
        switch (g_clog_state.backend) {
            case CLOG_BACKEND_DLT:
#ifdef CLOG_WITH_DLT
                /* Deinitialize DLT backend via sink implementation */
                clog_dlt_deinit();
#endif
                break;

            case CLOG_BACKEND_DIAG:
#ifdef CLOG_WITH_DIAG
                /* Deinitialize DIAG backend via sink implementation */
                clog_diag_deinit();
#endif
                break;

            case CLOG_BACKEND_SYSLOG:
#ifdef CLOG_WITH_SYSLOG
                closelog();
                clog_syslog_reset_ident();
#endif
                break;

            case CLOG_BACKEND_LOGCAT:
            case CLOG_BACKEND_CONSOLE:
            case CLOG_BACKEND_FILE:
                /* No special cleanup needed */
                break;

            default:
                break;
        }
        g_clog_state.backends_initialized = 0;
    }

    g_clog_state.num_contexts = 0;
    g_clog_state.initialized = 0;
    g_clog_state.backend = CLOG_BACKEND_CONSOLE;
    g_clog_state.fallback_to_syslog = 0;
    g_clog_state.global_log_level = CLOG_LEVEL_INFO;
    g_clog_state.add_clogdetails_in_msg = (int)CFG_DEFAULT_ADD_CLOGDETAILS_IN_MSG;
    g_clog_state.sink_file_max_size_bytes =
        (size_t)CFG_DEFAULT_SINK_FILE_MAX_SIZE_MB * 1024u * 1024u;
    g_clog_state.sink_file_rotate_count = (uint16_t)CFG_DEFAULT_SINK_FILE_ROTATE_COUNT;
    g_clog_state.clog_app_id = 0;
    g_clog_state.app_id[0] = '\0';
    g_clog_state.app_description[0] = '\0';
    g_clog_app_id = NULL;

    pthread_mutex_unlock(&g_clog_state.lock);
    pthread_mutex_destroy(&g_clog_state.lock);

    return CLOG_RETURN_OK;
}

clog_return_t clog_is_initialized(void)
{
    return g_clog_state.initialized ? CLOG_RETURN_TRUE : CLOG_RETURN_OK;
}

int clog_debug_trace_enabled(void)
{
    return g_clog_debug_trace_enabled;
}

/**
 * @brief Internal helper to register context with configuration
 *
 * This is the core implementation for context registration.
 * Config must not be NULL - all backend identifiers must be provided.
 * Uses dlt_context_id as context_id and dlt_description as context_description.
 *
 * Returns the context handle on success, NULL on any failure (bad
 * argument, CLOG not initialized, context table full, allocation
 * failure) - the public clog_register_context() wrapper does not
 * distinguish the cause.
 */
static clog_context_internal_t* clog_register_context_internal(
    clog_context_id_t clog_context_id, const clog_context_config_t *config)
{
    clog_context_internal_t *ctx;
    clog_context_internal_t *new_ctx;
    const char *context_id;
    const char *context_description;
    clog_backend_t active_backend;

    if (!g_clog_state.initialized) {
        return NULL;
    }

    if (config == NULL) {
        return NULL;
    }

    /* Extract context_id and description from config */
    context_id = config->dlt_context_id;
    context_description = config->dlt_description;

    if (context_id == NULL || context_description == NULL) {
        return NULL;
    }

    pthread_mutex_lock(&g_clog_state.lock);

    /* Check if context already exists */
    ctx = clog_find_context(context_id);
    if (ctx != NULL) {
        pthread_mutex_unlock(&g_clog_state.lock);
        return ctx;
    }

    if (g_clog_state.num_contexts >= CLOG_MAX_CONTEXTS) {
        pthread_mutex_unlock(&g_clog_state.lock);
        return NULL;
    }

    /* Grow the pointer array if needed - realloc() never moves the
     * individual context blocks it points to, so handles already
     * returned to callers remain valid across this growth. Capped at
     * CLOG_MAX_CONTEXTS by the check above, so this never grows past it. */
    if (g_clog_state.num_contexts >= g_clog_state.contexts_capacity) {
        int new_capacity = (g_clog_state.contexts_capacity == 0)
            ? CLOG_INITIAL_CONTEXT_CAPACITY : (g_clog_state.contexts_capacity * 2);
        if (new_capacity > CLOG_MAX_CONTEXTS) {
            new_capacity = CLOG_MAX_CONTEXTS;
        }
        clog_context_internal_t **new_array = (clog_context_internal_t **)realloc(
            g_clog_state.contexts, (size_t)new_capacity * sizeof(clog_context_internal_t *));
        if (new_array == NULL) {
            pthread_mutex_unlock(&g_clog_state.lock);
            return NULL;
        }
        g_clog_state.contexts = new_array;
        g_clog_state.contexts_capacity = new_capacity;
    }

    new_ctx = (clog_context_internal_t *)calloc(1u, sizeof(clog_context_internal_t));
    if (new_ctx == NULL) {
        pthread_mutex_unlock(&g_clog_state.lock);
        return NULL;
    }

    ctx = new_ctx;
    g_clog_state.contexts[g_clog_state.num_contexts] = ctx;

    ctx->clog_context_id = clog_context_id;

    (void)strlcpy(ctx->context_id, context_id, sizeof(ctx->context_id));

#ifdef CLOG_WITH_DLT
    clog_warn_if_too_long("context_id", ctx->context_id, CLOG_DLT_ID_LEN);
#endif

    (void)strlcpy(ctx->description, context_description, sizeof(ctx->description));
    /* Initialize backend-specific identifiers from config (required) */
#ifdef CLOG_WITH_DIAG
    ctx->diag_ssid = config->diag_ssid;  /* CLOG_DIAG_SSID_NOT_AVAILABLE (0) is a valid value, not an error */
#endif

#ifdef CLOG_WITH_LOGCAT
    if (config->logcat_tag != NULL) {
	(void)strlcpy(ctx->logcat_tag, config->logcat_tag, sizeof(ctx->logcat_tag));
    } else {
        free(new_ctx);
        pthread_mutex_unlock(&g_clog_state.lock);
        return NULL;  /* Config must provide all identifiers */
    }
    ctx->logcat_tag[CLOG_MAX_CTX_ID_LEN - 1] = '\0';
#endif

#ifdef CLOG_WITH_SYSLOG
    if (config->syslog_tag != NULL) {
	(void)strlcpy(ctx->syslog_tag, config->syslog_tag, sizeof(ctx->syslog_tag));
    } else {
        free(new_ctx);
        pthread_mutex_unlock(&g_clog_state.lock);
        return NULL;  /* Config must provide all identifiers */
    }
    ctx->syslog_tag[CLOG_MAX_CTX_ID_LEN - 1] = '\0';
#endif

    ctx->is_registered = 1;

    /* Register with DLT synchronously, in the same call as context registration */
    active_backend = clog_get_active_backend();
    ctx->dlt_context = clog_dlt_context_register_if_needed(active_backend, ctx->context_id, ctx->description);
    if (ctx->dlt_context == NULL && active_backend == CLOG_BACKEND_DLT) {
        clog_log_internal(CLOG_LEVEL_ERROR, "CLOG: Failed to register DLT context '%s'", ctx->context_id);
        clog_enable_syslog_fallback("DLT context registration failed");
    }

    ctx->diag_ssid_warned = 0;

    g_clog_state.num_contexts++;

    pthread_mutex_unlock(&g_clog_state.lock);

    return ctx;
}

clog_context_t clog_register_context(clog_context_id_t clog_context_id, const clog_context_config_t *config)
{
    return (clog_context_t)clog_register_context_internal(clog_context_id, config);
}

clog_return_t clog_unregister_context(clog_context_t context)
{
    clog_context_internal_t *ctx = (clog_context_internal_t *)context;

    if (!g_clog_state.initialized || ctx == NULL) {
        return CLOG_RETURN_WRONG_PARAMETER;
    }

    pthread_mutex_lock(&g_clog_state.lock);

    if (!ctx->is_registered) {
        pthread_mutex_unlock(&g_clog_state.lock);
        return CLOG_RETURN_ERROR;
    }

    /* Unregister from DLT if enabled */
    clog_dlt_context_unregister_if_needed(g_clog_state.backend, ctx->dlt_context);
    ctx->dlt_context = NULL;

    ctx->is_registered = 0;

    pthread_mutex_unlock(&g_clog_state.lock);

    return CLOG_RETURN_OK;
}

/**
 * @brief Internal logging implementation (printf-style)
 *
 * This is the core logging function that handles all printf-style logging.
 * All level-specific functions (clogd, cloge, etc.) delegate to this function.
 * Log levels are retrieved from cfg-lib resolved app/context state.
 */
static clog_return_t clog_log_dispatch(clog_context_t context, const char *file, int line, clog_level_t level, const char *format, va_list args)
{
    clog_context_internal_t *ctx = (clog_context_internal_t *)context;
    char message[CLOG_MAX_MESSAGE_LEN] = {0};
    clog_backend_t active_backend;
    clog_level_t backend_level;
    clog_return_t result;

    if (unlikely(!g_clog_state.initialized || ctx == NULL || format == NULL)) {
        return CLOG_RETURN_WRONG_PARAMETER;
    }

    if (unlikely(!clog_should_log(level))) {
        return CLOG_RETURN_LOGGING_DISABLED;
    }

    pthread_mutex_lock(&g_clog_state.lock);

    /* Format message */
    if (vsnprintf(message, sizeof(message), format, args) < 0) {
        pthread_mutex_unlock(&g_clog_state.lock);
        return CLOG_RETURN_ERROR;
    }

    /* After context unregister, keep the message visible via syslog fallback. */
    if (unlikely(!ctx->is_registered)) {
        result = clog_log_syslog_ctx(ctx, level, message);
        pthread_mutex_unlock(&g_clog_state.lock);
        return result;
    }

    /* Get the resolved log level for this context. */
    backend_level = clog_get_log_level(ctx);
    active_backend = clog_get_active_backend();

    result = CLOG_RETURN_OK;

    /* DLT and DIAG do not get CLOG-level filtering: DLT relies on the DLT
     * daemon's own runtime level filter, and DIAG's MSG_SPRINTF_x macros have
     * no CLOG-level awareness at all - forward every message to them and let
     * them decide. Other backends keep the threshold check since they have
     * no filtering of their own. */
    if (active_backend == CLOG_BACKEND_DLT || active_backend == CLOG_BACKEND_DIAG ||
        (backend_level != CLOG_LEVEL_OFF && level <= backend_level)) {
        /* Call backend sink function */
        switch (active_backend) {
            case CLOG_BACKEND_DLT:
                /* No CLOG-level filtering - always forwarded, see guard above */
                clog_log_dlt(ctx, level, message);
                break;

            case CLOG_BACKEND_DIAG:
                /* No CLOG-level filtering - always forwarded, see guard above */
                clog_log_diag(ctx, file, line, level, message);
                break;

            case CLOG_BACKEND_LOGCAT:
                clog_log_logcat_ctx(ctx, level, message);
                break;

            case CLOG_BACKEND_SYSLOG:
                clog_log_syslog_ctx(ctx, level, message);
                break;

            case CLOG_BACKEND_CONSOLE:
                clog_log_console_ctx(ctx, level, message);
                break;

            case CLOG_BACKEND_FILE:
                clog_log_file_ctx(ctx, level, message);
                break;

            default:
                break;
        }
    }

    pthread_mutex_unlock(&g_clog_state.lock);

    return CLOG_RETURN_OK;
}

/**
 * @brief Log a message at specified level (printf-style)
 *
 * @param context Context handle
 * @param level Log level
 * @param format Format string (printf-style)
 * @param ... Variable arguments
 *
 * @return CLOG_RETURN_OK on success, CLOG_RETURN_LOGGING_DISABLED if filtered, CLOG_RETURN_ERROR/CLOG_RETURN_WRONG_PARAMETER on failure
 */
clog_return_t clog_log(clog_context_t context, const char *file, int line, clog_level_t level, const char *format, ...)
{
    va_list args;
    clog_return_t result;

    va_start(args, format);
    result = clog_log_dispatch(context, file, line, level, format, args);
    va_end(args);

    return result;
}

/**
 * @brief Log a message at ERROR level (printf-style)
 *
 * @param context Context handle
 * @param format Format string (printf-style)
 * @param ... Variable arguments
 *
 * @return CLOG_RETURN_OK on success, CLOG_RETURN_LOGGING_DISABLED if filtered, CLOG_RETURN_ERROR/CLOG_RETURN_WRONG_PARAMETER on failure
 */
clog_return_t cloge(clog_context_t context, const char *file, int line, const char *format, ...)
{
    va_list args;
    clog_return_t result;

    va_start(args, format);
    result = clog_log_dispatch(context, file, line, CLOG_LEVEL_ERROR, format, args);
    va_end(args);

    return result;
}

/**
 * @brief Log a message at WARN level (printf-style)
 *
 * @param context Context handle
 * @param format Format string (printf-style)
 * @param ... Variable arguments
 *
 * @return CLOG_RETURN_OK on success, CLOG_RETURN_LOGGING_DISABLED if filtered, CLOG_RETURN_ERROR/CLOG_RETURN_WRONG_PARAMETER on failure
 */
clog_return_t clogw(clog_context_t context, const char *file, int line, const char *format, ...)
{
    va_list args;
    clog_return_t result;

    va_start(args, format);
    result = clog_log_dispatch(context, file, line, CLOG_LEVEL_WARN, format, args);
    va_end(args);

    return result;
}

/**
 * @brief Log a message at INFO level (printf-style)
 *
 * @param context Context handle
 * @param format Format string (printf-style)
 * @param ... Variable arguments
 *
 * @return CLOG_RETURN_OK on success, CLOG_RETURN_LOGGING_DISABLED if filtered, CLOG_RETURN_ERROR/CLOG_RETURN_WRONG_PARAMETER on failure
 */
clog_return_t clogi(clog_context_t context, const char *file, int line, const char *format, ...)
{
    va_list args;
    clog_return_t result;

    va_start(args, format);
    result = clog_log_dispatch(context, file, line, CLOG_LEVEL_INFO, format, args);
    va_end(args);

    return result;
}

/**
 * @brief Log a message at DEBUG level (printf-style)
 *
 * @param context Context handle
 * @param format Format string (printf-style)
 * @param ... Variable arguments
 *
 * @return CLOG_RETURN_OK on success, CLOG_RETURN_LOGGING_DISABLED if filtered, CLOG_RETURN_ERROR/CLOG_RETURN_WRONG_PARAMETER on failure
 */
clog_return_t clogd(clog_context_t context, const char *file, int line, const char *format, ...)
{
    va_list args;
    clog_return_t result;

    va_start(args, format);
    result = clog_log_dispatch(context, file, line, CLOG_LEVEL_DEBUG, format, args);
    va_end(args);

    return result;
}

/**
 * @brief Log a message at VERBOSE level (printf-style)
 *
 * @param context Context handle
 * @param format Format string (printf-style)
 * @param ... Variable arguments
 *
 * @return CLOG_RETURN_OK on success, CLOG_RETURN_LOGGING_DISABLED if filtered, CLOG_RETURN_ERROR/CLOG_RETURN_WRONG_PARAMETER on failure
 */
clog_return_t clogv(clog_context_t context, const char *file, int line, const char *format, ...)
{
    va_list args;
    clog_return_t result;

    va_start(args, format);
    result = clog_log_dispatch(context, file, line, CLOG_LEVEL_VERBOSE, format, args);
    va_end(args);

    return result;
}

clog_return_t clog_is_enabled(void)
{
    return g_clog_state.enabled ? CLOG_RETURN_TRUE : CLOG_RETURN_OK;
}

/**
 * @brief Auto-cleanup handler called by atexit()
 * 
 * This function is registered with atexit() to ensure cleanup happens
 * automatically when the program terminates.
 */
static void clog_auto_cleanup_handler(void)
{
    if (g_auto_cleanup_enabled && g_clog_state.initialized) {
        clog_unregister_app();
    }
}

clog_return_t clog_enable_auto_cleanup(void)
{
    /* Auto-cleanup is now enabled by default in clog_register_app()
     * This function is kept for backward compatibility but has no effect.
     */
    return CLOG_RETURN_OK;
}

clog_return_t clog_disable_auto_cleanup(void)
{
    g_auto_cleanup_enabled = 0;
    return CLOG_RETURN_OK;
}

clog_return_t clog_is_auto_cleanup_enabled(void)
{
    return g_auto_cleanup_enabled ? CLOG_RETURN_TRUE : CLOG_RETURN_OK;
}

/* ============================================================================
 * Runtime Log-Level Change Notification
 * ============================================================================ */

clog_return_t clog_register_log_level_changed_callback(clog_log_level_changed_callback_t callback)
{
    if (!g_clog_state.initialized) {
        return CLOG_RETURN_ERROR;
    }

    pthread_mutex_lock(&g_clog_state.lock);

    /* Store the user's callback */
    g_log_level_changed_callback = callback;

    if (callback != NULL) {
        g_callback_registered = 1;
        g_clog_state.callback_enabled = 1;
    }

    pthread_mutex_unlock(&g_clog_state.lock);
    return CLOG_RETURN_OK;
}

clog_return_t clog_unregister_log_level_changed_callback(void)
{
    if (!g_clog_state.initialized) {
        return CLOG_RETURN_ERROR;
    }

    pthread_mutex_lock(&g_clog_state.lock);

    /* Clear the callback */
    g_log_level_changed_callback = NULL;
    g_clog_state.callback_enabled = 0;

    pthread_mutex_unlock(&g_clog_state.lock);
    return CLOG_RETURN_OK;
}

clog_return_t clog_notify_log_level_changed(const char *context_id, clog_level_t log_level, clog_backend_t log_sink)
{
    clog_level_change_info_t info;

    if (!g_clog_state.initialized) {
        return CLOG_RETURN_ERROR;
    }

    pthread_mutex_lock(&g_clog_state.lock);

    if (g_log_level_changed_callback == NULL) {
        pthread_mutex_unlock(&g_clog_state.lock);
        return CLOG_RETURN_ERROR;
    }

    info.log_level = log_level;
    info.log_sink = log_sink;
    g_log_level_changed_callback((char *)context_id, info);

    pthread_mutex_unlock(&g_clog_state.lock);
    return CLOG_RETURN_OK;
}

/* ============================================================================
 * Context Log Data Query Implementation 
 * ============================================================================ */

clog_return_t clog_get_context_log_data(clog_context_t context, clog_context_log_data_t *log_data)
{
    clog_context_internal_t *ctx = (clog_context_internal_t *)context;
    clog_level_t backend_level;

    if (!g_clog_state.initialized || ctx == NULL || log_data == NULL) {
        return CLOG_RETURN_WRONG_PARAMETER;
    }

    if (!ctx->is_registered) {
        return CLOG_RETURN_ERROR;
    }

    pthread_mutex_lock(&g_clog_state.lock);

    /* Get the resolved log level for this context. */
    backend_level = clog_get_log_level(ctx);

    /* Fill output structure with the configured backend */
    log_data->backend = clog_get_active_backend();
    log_data->log_level = backend_level;

    pthread_mutex_unlock(&g_clog_state.lock);

    return CLOG_RETURN_OK;
}
