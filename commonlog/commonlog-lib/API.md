# CLOG Library API Reference

CLOG (`libclog`) is the unified logging wrapper library used across
telematics platform components. It multiplexes application log calls to
one of several backends (DLT, DIAG, syslog, logcat, file, console) chosen
at runtime via cfg-lib-resolved app/context configuration, while presenting callers with a
single, backend-agnostic API.

This document covers the **public application-facing API declared in
`src/clog.h`** — the macros callers should use, and the handful of
`clog.h` functions that have no macro wrapper. It does not cover the
backend-specific `sinkImpl/*.h` headers (`clog_sink_dlt.h`,
`clog_sink_diag.h`, `clog_sink_syslog.h`, `clog_sink_logcat.h`,
`clog_sink_file.h`, `clog_sink_console.h`) — those are internal-dispatch
functions invoked by `clog.c`, not intended to be called directly by
applications, and have no macro wrappers.

Rows marked `N/A` in the Macro column are functions with no macro
wrapper; call them directly by their API name.

## Data Structures

Types used by the general APIs below, all declared in `clog.h`.

| Name | Kind | Fields / Values | Purpose |
|---|---|---|---|
| `clog_backend_t` | enum | `CLOG_BACKEND_CONSOLE`, `CLOG_BACKEND_DLT`, `CLOG_BACKEND_DIAG`, `CLOG_BACKEND_SYSLOG`, `CLOG_BACKEND_LOGCAT`, `CLOG_BACKEND_FILE` | Identifies which logging backend is active/configured. |
| `clog_return_t` | enum | `CLOG_RETURN_LOGGING_DISABLED` (-4), `CLOG_RETURN_WRONG_PARAMETER` (-3), `CLOG_RETURN_BUFFER_FULL` (-2), `CLOG_RETURN_ERROR` (-1), `CLOG_RETURN_OK` (0), `CLOG_RETURN_TRUE` (1) | DLT-style status code returned by every CLOG public API function; see [Return Values](#return-values) below. Convert to a string via `clog_return_to_string(clog_return_t)` (declared in `clog_sinkImpl.h`). |
| `clog_level_t` | enum | `CLOG_LEVEL_OFF`, `CLOG_LEVEL_FATAL`, `CLOG_LEVEL_ERROR`, `CLOG_LEVEL_WARN`, `CLOG_LEVEL_INFO`, `CLOG_LEVEL_DEBUG`, `CLOG_LEVEL_VERBOSE` | Severity level for a log message or a context's/backend's active threshold. |
| `clog_app_id_t` | typedef (`uint16_t`) | Any 16-bit value, e.g. from the `clog_app_id_e` enum in `clog_ids.h` | Numeric application identifier passed to `CLOG_REGISTER_APP`/`clog_register_app`; used in backend detail formatting when `add_clogdetails_in_msg=1`. |
| `clog_context_id_t` | typedef (`uint16_t`) | Any 16-bit value, e.g. from the `clog_actx_id_e`/`clog_lctx_id_e` enums in `clog_ids.h` | Numeric context identifier passed to `CLOG_REGISTER_CONTEXT`/`clog_register_context`; used in backend detail formatting when `add_clogdetails_in_msg=1`. |
| `clog_context_t` | typedef (`void *`) | Opaque pointer | Handle returned by `clog_register_context`/`CLOG_REGISTER_CONTEXT` (`NULL` on failure), passed to every logging/query/unregister call for that context. |
| `CLOG_DIAG_SSID_NOT_AVAILABLE` | macro constant (`(uint16_t)0`) | `0` | Sentinel value for `clog_context_config_t.diag_ssid` meaning "no DIAG SSID specified." |
| `clog_context_config_t` | struct | `const char *dlt_context_id;`<br>`const char *dlt_description;`<br>`uint16_t diag_ssid;`<br>`const char *logcat_tag;`<br>`const char *syslog_tag;` | Per-context backend identifiers supplied at registration time (one field consumed per backend); passed to `clog_register_context`/built internally by `CLOG_REGISTER_CONTEXT`. |
| `clog_context_log_data_t` | struct | `clog_backend_t backend;`<br>`clog_level_t log_level;` | Snapshot of a context's active backend and resolved log level, filled in by `clog_get_context_log_data`/`CLOG_GET_CONTEXT_LOG_DATA`. |
| `clog_level_change_info_t` | struct | `clog_level_t log_level;`<br>`clog_backend_t log_sink;` | Extensible snapshot passed to the log-level-changed callback, carrying the new level/backend. |
| `clog_log_level_changed_callback_t` | typedef (function pointer) | `void (*)(char *context_id, clog_level_change_info_t info)` | Signature required by `CLOG_REGISTER_LOG_LEVEL_CHANGED_CALLBACK`/`clog_register_log_level_changed_callback`; invoked when a context's runtime log level/backend changes. |

## Return Values

Every public CLOG API function returns a `clog_return_t` (DLT-style typed
status code) instead of a plain `int`/`NULL`:

| Value | Numeric | Meaning |
|---|---|---|
| `CLOG_RETURN_LOGGING_DISABLED` | -4 | The message was filtered out (level below threshold, or logging disabled) rather than actually written — only returned by the logging functions (`CLOG_LOG`/`CLOGE`/etc.). |
| `CLOG_RETURN_WRONG_PARAMETER` | -3 | A required argument was `NULL` or otherwise invalid (e.g. NULL `app_id`, NULL config, NULL context). |
| `CLOG_RETURN_BUFFER_FULL` | -2 | The context table is full (`CLOG_MAX_CONTEXTS` reached); only returned by `clog_register_context`. |
| `CLOG_RETURN_ERROR` | -1 | General failure — CLOG not initialized, already initialized, allocation failure, backend init failure, no callback registered, etc. |
| `CLOG_RETURN_OK` | 0 | Success, or a boolean "false"/"no" answer from a query function. |
| `CLOG_RETURN_TRUE` | 1 | Boolean "true"/"yes" answer from a query function (`clog_is_enabled`, `clog_is_initialized`, `clog_is_auto_cleanup_enabled`). |

Use `clog_return_to_string(clog_return_t)` (declared in `clog_sinkImpl.h`)
to print a value by name for logging/diagnostics.

## Initialization & Cleanup

| Macro | API | Purpose | Return Value |
|---|---|---|---|
| `CLOG_REGISTER_APP(numeric_app_id, app_id, app_desc)` | `clog_register_app(clog_app_id_t clog_app_id, const char *app_id, const char *app_description)` | Registers the calling application with CLOG under a numeric app ID plus a short app ID string and description; resolves the app sink/level and runtime settings directly from `/etc/commonlog.conf`, and enables auto-cleanup at process exit. Must be called once before any logging or context registration. | `CLOG_RETURN_OK` on success; `CLOG_RETURN_WRONG_PARAMETER` on NULL `app_id`/`app_description`; `CLOG_RETURN_ERROR` on cfg-lib lookup failure or if already initialized |
| `CLOG_UNREGISTER_APP()` | `clog_unregister_app(void)` | Unregisters all contexts, tears down the active backend, and resets CLOG to an uninitialized state. | `CLOG_RETURN_OK` on success; `CLOG_RETURN_ERROR` if CLOG was not initialized |
| `CLOG_IS_INITIALIZED()` | `clog_is_initialized(void)` | Reports whether `clog_register_app()` (directly or via `clog_register_library()`) has completed successfully. Useful for a caller to check before its own explicit registration, to avoid the `CLOG_RETURN_ERROR`/"already initialized" case documented on `clog_register_library()`. | `CLOG_RETURN_TRUE` if initialized, `CLOG_RETURN_OK` (false) otherwise |
| `CLOG_REGISTER_LIBRARY()` | `clog_register_library(const char *caller_tag)` | Registers a shared library as a CLOG dependent without requiring it to own app-level registration. The macro automatically passes `__FILE__` as `caller_tag`, so CLOG's own internal diagnostics show which library source file registered/reused the registration - no extra argument needed at the call site. If CLOG is already initialized (by the app or another library), just records the dependency; otherwise auto-generates an app_id/description from the process name/PID/TID and registers them itself. Safe to call from multiple libraries, in any order relative to `clog_register_app()`. | `CLOG_RETURN_OK` on success; `CLOG_RETURN_ERROR` on failure (only when CLOG must auto-register and identity generation fails) |
| `CLOG_UNREGISTER_LIBRARY()` | `clog_unregister_library(void)` | Releases a prior `clog_register_library()` call; decrements the library reference count and, if this was the last dependent and CLOG auto-registered the app itself, tears it down via `clog_unregister_app()`. | `CLOG_RETURN_OK` always (including as a no-op if nothing is outstanding) |

## Context Management

| Macro | API | Purpose | Return Value |
|---|---|---|---|
| `CLOG_REGISTER_CONTEXT(numeric_ctx_id, ctx_id, ctx_desc, diag_id, logcat_tag_val, syslog_tag_val)` | `clog_register_context(clog_context_id_t clog_context_id, const clog_context_config_t *config)` | Registers a logging context (a per-subsystem tag) under a numeric context ID, together with the backend-specific identifiers needed by every backend (DLT context ID/description, DIAG SSID, logcat tag, syslog tag). Must be called after `clog_register_app()`/`clog_register_library()`. Calling it again with the same `ctx_id` string returns the existing context's handle instead of creating a duplicate. | Returns the `clog_context_t` handle on success; `NULL` on failure (bad argument, CLOG not initialized, context table full, or allocation failure) |
| `CLOG_UNREGISTER_CONTEXT(context)` | `clog_unregister_context(clog_context_t context)` | Unregisters a previously registered context, releasing its backend-specific resources (e.g. DLT context handle). | `CLOG_RETURN_OK` on success; `CLOG_RETURN_ERROR` on failure (CLOG not initialized, NULL/unregistered context) |
| `CLOG_GET_CONTEXT_LOG_DATA(context, log_data_ptr)` | `clog_get_context_log_data(clog_context_t context, clog_context_log_data_t *log_data)` | Queries a context's current active backend and resolved log level, writing the result into the caller-supplied `clog_context_log_data_t`. | `CLOG_RETURN_OK` on success; `CLOG_RETURN_WRONG_PARAMETER`/`CLOG_RETURN_ERROR` on failure (CLOG not initialized, NULL/unregistered context, or NULL output pointer) |

## Logging API

Each logging macro/function automatically captures the caller's
`__FILE__`/`__LINE__` (via the macro) and forwards it to backends that can
use it (currently DIAG); other backends ignore it.

| Macro | API | Purpose | Return Value |
|---|---|---|---|
| `CLOG_LOG(context, level, format, ...)` | `clog_log(clog_context_t context, const char *file, int line, clog_level_t level, const char *format, ...)` | Logs a formatted message to a context at an explicitly given level. | `CLOG_RETURN_OK` on success; `CLOG_RETURN_LOGGING_DISABLED` if filtered out by level/enablement; `CLOG_RETURN_WRONG_PARAMETER`/`CLOG_RETURN_ERROR` on failure (CLOG not initialized, NULL context/format) |
| `CLOG_FATAL(context, format, ...)` | `clog_log(context, file, line, CLOG_LEVEL_FATAL, format, ...)` | Convenience wrapper logging at `CLOG_LEVEL_FATAL`. | Same as `CLOG_LOG` |
| `CLOGE(context, format, ...)` | `cloge(clog_context_t context, const char *file, int line, const char *format, ...)` | Logs a formatted message at `CLOG_LEVEL_ERROR`. | Same as `CLOG_LOG` |
| `CLOGW(context, format, ...)` | `clogw(clog_context_t context, const char *file, int line, const char *format, ...)` | Logs at `CLOG_LEVEL_WARN`. | Same as `CLOG_LOG` |
| `CLOGI(context, format, ...)` | `clogi(clog_context_t context, const char *file, int line, const char *format, ...)` | Logs at `CLOG_LEVEL_INFO`. | Same as `CLOG_LOG` |
| `CLOGD(context, format, ...)` | `clogd(clog_context_t context, const char *file, int line, const char *format, ...)` | Logs at `CLOG_LEVEL_DEBUG`. | Same as `CLOG_LOG` |
| `CLOGV(context, format, ...)` | `clogv(clog_context_t context, const char *file, int line, const char *format, ...)` | Logs at `CLOG_LEVEL_VERBOSE`. | Same as `CLOG_LOG` |
| `N/A` | `clog_is_enabled(void)` | Reports whether logging is currently enabled process-wide. | `CLOG_RETURN_TRUE` if enabled, `CLOG_RETURN_OK` (false) otherwise |

## Auto-Cleanup Control

Auto-cleanup at process exit is enabled automatically by
`clog_register_app()`; these functions exist for explicit control.

| Macro | API | Purpose | Return Value |
|---|---|---|---|
| `N/A` | `clog_enable_auto_cleanup(void)` | Retained for backward compatibility; auto-cleanup is already enabled by default in `clog_register_app()`, so this call has no effect. | `CLOG_RETURN_OK` always |
| `N/A` | `clog_disable_auto_cleanup(void)` | Disables the automatic `clog_unregister_app()` call registered via `atexit()`. | `CLOG_RETURN_OK` always |
| `N/A` | `clog_is_auto_cleanup_enabled(void)` | Reports whether auto-cleanup-at-exit is currently enabled. | `CLOG_RETURN_TRUE` if enabled, `CLOG_RETURN_OK` (false) otherwise |

## Runtime Log-Level Change Notification

| Macro | API | Purpose | Return Value |
|---|---|---|---|
| `CLOG_REGISTER_LOG_LEVEL_CHANGED_CALLBACK(callback)` | `clog_register_log_level_changed_callback(clog_log_level_changed_callback_t callback)` | Registers a process-wide callback, invoked whenever a context's runtime log level or backend changes, with the affected context ID and the new level/backend. | `CLOG_RETURN_OK` on success; `CLOG_RETURN_ERROR` if CLOG was not initialized |
| `CLOG_UNREGISTER_LOG_LEVEL_CHANGED_CALLBACK()` | `clog_unregister_log_level_changed_callback(void)` | Removes the previously registered log-level-changed callback. | `CLOG_RETURN_OK` on success; `CLOG_RETURN_ERROR` if CLOG was not initialized |
| `CLOG_NOTIFY_LOG_LEVEL_CHANGED(context_id, log_level, log_sink)` | `clog_notify_log_level_changed(const char *context_id, clog_level_t log_level, clog_backend_t log_sink)` | Fires the registered log-level-changed callback with the given context ID, level, and backend. Used internally by backend sinks (e.g. the DLT backend, when the DLT daemon changes a context's level externally) and available for a caller to invoke itself (e.g. after a config reload) to push a level change to its registered callback. | `CLOG_RETURN_OK` on success; `CLOG_RETURN_ERROR` if CLOG was not initialized or no callback is registered |
