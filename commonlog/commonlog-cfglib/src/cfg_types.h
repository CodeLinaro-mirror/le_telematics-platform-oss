/*
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#ifndef CFG_TYPES_H
#define CFG_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CLOG_APP_ID_T_DEFINED
#define CLOG_APP_ID_T_DEFINED 1
typedef uint16_t clog_app_id_t;
#endif

#ifndef CLOG_CONTEXT_ID_T_DEFINED
#define CLOG_CONTEXT_ID_T_DEFINED 1
typedef uint16_t clog_context_id_t;
#endif

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

#ifdef __cplusplus
}
#endif

#endif /* CFG_TYPES_H */
