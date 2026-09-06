/*
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

/*
 * Public API Header
 */

#ifndef CLOG_H
#define CLOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#ifdef CLOG_WITH_DIAG
#include <msgcfg.h>
#endif

#ifdef __GNUC__
#define CLOG_PRINTF_FORMAT(fmt_idx, arg_idx) __attribute__((format(printf, fmt_idx, arg_idx)))
#else
#define CLOG_PRINTF_FORMAT(fmt_idx, arg_idx)
#endif

/* ============================================================================
 * Types
 * ============================================================================ */

#ifndef CLOG_BACKEND_T_DEFINED
#define CLOG_BACKEND_T_DEFINED 1
typedef enum {
    CLOG_BACKEND_CONSOLE = 0,
    CLOG_BACKEND_DLT,
    CLOG_BACKEND_DIAG,
    CLOG_BACKEND_SYSLOG,
    CLOG_BACKEND_LOGCAT,
    CLOG_BACKEND_FILE
} clog_backend_t;
#endif

#ifndef CLOG_LEVEL_T_DEFINED
#define CLOG_LEVEL_T_DEFINED 1
typedef enum {
    CLOG_LEVEL_OFF = 0,
    CLOG_LEVEL_FATAL,
    CLOG_LEVEL_ERROR,
    CLOG_LEVEL_WARN,
    CLOG_LEVEL_INFO,
    CLOG_LEVEL_DEBUG,
    CLOG_LEVEL_VERBOSE
} clog_level_t;
#endif

/** @brief CLOG's own DLT-style typed return value for the public API */
typedef enum {
    CLOG_RETURN_LOGGING_DISABLED = -4,
    CLOG_RETURN_WRONG_PARAMETER  = -3,
    CLOG_RETURN_BUFFER_FULL      = -2,
    CLOG_RETURN_ERROR            = -1,
    CLOG_RETURN_OK               = 0,
    CLOG_RETURN_TRUE             = 1
} clog_return_t;

#ifndef CLOG_APP_ID_T_DEFINED
#define CLOG_APP_ID_T_DEFINED 1
/** @brief Numeric application identifier used in backend detail formatting when enabled */
typedef uint16_t clog_app_id_t;
#endif

#ifndef CLOG_CONTEXT_ID_T_DEFINED
#define CLOG_CONTEXT_ID_T_DEFINED 1
/** @brief Numeric context identifier used in backend detail formatting when enabled */
typedef uint16_t clog_context_id_t;
#endif

/** @brief Opaque handle to a registered logging context */
typedef void* clog_context_t;

/** @brief Sentinel for clog_context_config_t.diag_ssid meaning "not specified" */
#define CLOG_DIAG_SSID_NOT_AVAILABLE ((uint16_t)0)

/** @brief Per-context backend identifiers supplied at registration time */
typedef struct {
    const char *dlt_context_id;
    const char *dlt_description;
    uint16_t diag_ssid;
    const char *logcat_tag;
    const char *syslog_tag;
} clog_context_config_t;

/** @brief Snapshot of a context's active backend/level */
typedef struct {
    clog_backend_t backend;
    clog_level_t log_level;
} clog_context_log_data_t;

/** @brief Extensible snapshot passed to the log-level-changed callback */
typedef struct {
    clog_level_t log_level;
    clog_backend_t log_sink;
} clog_level_change_info_t;

/** @brief Callback invoked when a backend's runtime log level changes */
typedef void (*clog_log_level_changed_callback_t)(char *context_id,
                                                    clog_level_change_info_t info);

/* ============================================================================
 * Initialization & Cleanup
 * ============================================================================ */

clog_return_t clog_register_app(clog_app_id_t clog_app_id, const char *app_id, const char *app_description);
clog_return_t clog_unregister_app(void);
clog_return_t clog_is_initialized(void);

#define CLOG_REGISTER_APP(numeric_app_id, app_id, app_desc) \
    clog_register_app((clog_app_id_t)(numeric_app_id), (app_id), (app_desc))

#define CLOG_UNREGISTER_APP() \
    clog_unregister_app()

#define CLOG_IS_INITIALIZED() \
    clog_is_initialized()

/**
 * @brief Register the calling library with CLOG, auto-registering the app if needed
 *
 * Intended for libraries that link CLOG internally and log at context level,
 * but have no visibility into whether the top-level application (or another
 * library sharing this process) has already called clog_register_app().
 * Safe to call once per library, from multiple libraries, and in any order
 * relative to an explicit clog_register_app() call:
 *   - If CLOG is already initialized (by the app or another library), this
 *     just records the caller's dependency on it and reuses it as-is.
 *   - Otherwise, it auto-generates an app_id/app_description from the
 *     process's name/PID/TID and registers them via clog_register_app(),
 *     tracking that CLOG itself owns this registration.
 * Call clog_unregister_library() (or rely on process-exit auto-cleanup) to
 * release it; CLOG only tears down what it auto-registered, and only once
 * every calling library has released its dependency on it.
 *
 * Note: if a library auto-registers first and the application later calls
 * clog_register_app() explicitly, that call returns CLOG_RETURN_ERROR
 * (already initialized) - existing clog_init_internal() behavior. Call
 * clog_is_initialized() first if the app needs to skip its own explicit
 * registration in that case.
 *
 * @param caller_tag Identifies the calling library in CLOG's own internal
 *        diagnostics. Callers should use CLOG_REGISTER_LIBRARY() rather than
 *        calling this directly - the macro fills this in automatically from
 *        __FILE__, so every library gets a distinct tag with no extra effort.
 * @return CLOG_RETURN_OK on success, CLOG_RETURN_ERROR on failure
 */
clog_return_t clog_register_library(const char *caller_tag);

/**
 * @brief Release a prior clog_register_library() call
 *
 * Decrements the library refcount. If this was the last outstanding
 * library registration and CLOG itself auto-registered the app (as opposed
 * to reusing an app-level registration made by someone else), tears down
 * that registration via clog_unregister_app().
 *
 * @return CLOG_RETURN_OK always (including a no-op if nothing is outstanding)
 */
clog_return_t clog_unregister_library(void);

#define CLOG_REGISTER_LIBRARY()   clog_register_library(__FILE__)
#define CLOG_UNREGISTER_LIBRARY() clog_unregister_library()

/* ============================================================================
 * Context Management
 * ============================================================================ */

clog_context_t clog_register_context(clog_context_id_t clog_context_id, const clog_context_config_t *config);
clog_return_t clog_unregister_context(clog_context_t context);
clog_return_t clog_get_context_log_data(clog_context_t context, clog_context_log_data_t *log_data);

#define CLOG_REGISTER_CONTEXT(numeric_ctx_id, ctx_id, ctx_desc, diag_id, logcat_tag_val, syslog_tag_val) \
    ({ \
        clog_context_config_t clog_reg_ctx_cfg__ = { \
            .dlt_context_id = (ctx_id), \
            .dlt_description = (ctx_desc), \
            .diag_ssid = (diag_id), \
            .logcat_tag = (logcat_tag_val), \
            .syslog_tag = (syslog_tag_val) \
        }; \
        clog_register_context((clog_context_id_t)(numeric_ctx_id), &clog_reg_ctx_cfg__); \
    })

#define CLOG_UNREGISTER_CONTEXT(context) \
    clog_unregister_context(context)

#define CLOG_GET_CONTEXT_LOG_DATA(context, log_data_ptr) \
    clog_get_context_log_data((context), (log_data_ptr))

/* ============================================================================
 * Logging API
 * ============================================================================ */

clog_return_t clog_log(clog_context_t context, const char *file, int line, clog_level_t level, const char *format, ...) CLOG_PRINTF_FORMAT(5, 6);
clog_return_t cloge(clog_context_t context, const char *file, int line, const char *format, ...) CLOG_PRINTF_FORMAT(4, 5);
clog_return_t clogw(clog_context_t context, const char *file, int line, const char *format, ...) CLOG_PRINTF_FORMAT(4, 5);
clog_return_t clogi(clog_context_t context, const char *file, int line, const char *format, ...) CLOG_PRINTF_FORMAT(4, 5);
clog_return_t clogd(clog_context_t context, const char *file, int line, const char *format, ...) CLOG_PRINTF_FORMAT(4, 5);
clog_return_t clogv(clog_context_t context, const char *file, int line, const char *format, ...) CLOG_PRINTF_FORMAT(4, 5);

clog_return_t clog_is_enabled(void);

/* ============================================================================
 * Macro-Based Logging API
 * ============================================================================ */

#ifdef __GNUC__
#define CLOG_VA_ARGS(...) , ##__VA_ARGS__
#else
#define CLOG_VA_ARGS(...) , __VA_ARGS__
#endif

#define CLOG_LOG(context, level, format, ...)   clog_log((context), __FILE__, __LINE__, (level), (format) CLOG_VA_ARGS(__VA_ARGS__))
#define CLOG_FATAL(context, format, ...)         clog_log((context), __FILE__, __LINE__, CLOG_LEVEL_FATAL, (format) CLOG_VA_ARGS(__VA_ARGS__))
#define CLOGE(context, format, ...)             cloge((context), __FILE__, __LINE__, (format) CLOG_VA_ARGS(__VA_ARGS__))
#define CLOGW(context, format, ...)             clogw((context), __FILE__, __LINE__, (format) CLOG_VA_ARGS(__VA_ARGS__))
#define CLOGI(context, format, ...)             clogi((context), __FILE__, __LINE__, (format) CLOG_VA_ARGS(__VA_ARGS__))
#define CLOGD(context, format, ...)             clogd((context), __FILE__, __LINE__, (format) CLOG_VA_ARGS(__VA_ARGS__))
#define CLOGV(context, format, ...)             clogv((context), __FILE__, __LINE__, (format) CLOG_VA_ARGS(__VA_ARGS__))

/* ============================================================================
 * Auto-Cleanup Control
 * ============================================================================ */

clog_return_t clog_enable_auto_cleanup(void);
clog_return_t clog_disable_auto_cleanup(void);
clog_return_t clog_is_auto_cleanup_enabled(void);

/* ============================================================================
 * Runtime Log-Level Change Notification
 * ============================================================================ */

clog_return_t clog_register_log_level_changed_callback(clog_log_level_changed_callback_t callback);
clog_return_t clog_unregister_log_level_changed_callback(void);
clog_return_t clog_notify_log_level_changed(const char *context_id, clog_level_t log_level, clog_backend_t log_sink);

#define CLOG_REGISTER_LOG_LEVEL_CHANGED_CALLBACK(callback) \
    clog_register_log_level_changed_callback(callback)

#define CLOG_UNREGISTER_LOG_LEVEL_CHANGED_CALLBACK() \
    clog_unregister_log_level_changed_callback()

#define CLOG_NOTIFY_LOG_LEVEL_CHANGED(context_id, log_level, log_sink) \
    clog_notify_log_level_changed((context_id), (log_level), (log_sink))

#ifdef __cplusplus
}
#endif

#endif /* CLOG_H */
