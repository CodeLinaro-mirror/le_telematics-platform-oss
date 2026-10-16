/*
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

/*
 * clogctl.c - control/admin tool for the persisted file.
 *
 * Usage:
 *   clogctl get-global
 *   clogctl set-global <sink> [<level>]
 *   clogctl get-settings
 *   clogctl set-setting <key> <value>
 *   clogctl get-app <app_id>
 *   clogctl set-app <app_id> <sink> [<level>]
 *   clogctl reset-app <app_id>
 *   clogctl get-context <app_id> <context_id>
 *   clogctl set-override <app_id> <context_id> <level>
 *   clogctl reset-override <app_id> <context_id>
 *   clogctl get-lctx <context_id>
 *   clogctl set-lctx <context_id> <level>
 *   clogctl reset-lctx <context_id>
 *   clogctl list-configs
 *   clogctl list-supported-sinks
 *   clogctl list-supported-loglevels
 *
 * Display rule: wherever a sink+level pair is printed, if the sink is
 * DIAG or DLT, only the sink is shown -- diag/dlt routing doesn't carry
 * a separate level concept here, so the level field is suppressed. For
 * set-global/set-app, DIAG and DLT may omit the level and are persisted
 * with INFO for consistency.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "cfg.h"

#ifdef USE_GLIB
#define __packed __attribute__((packed))
#include <glib.h>
#define strlcpy g_strlcpy
#endif

typedef enum {
    CLOGCTL_CMD_NONE = 0,
    CLOGCTL_CMD_GET_GLOBAL,
    CLOGCTL_CMD_SET_GLOBAL,
    CLOGCTL_CMD_GET_SETTINGS,
    CLOGCTL_CMD_SET_SETTING,
    CLOGCTL_CMD_GET_APP,
    CLOGCTL_CMD_SET_APP,
    CLOGCTL_CMD_RESET_APP,
    CLOGCTL_CMD_GET_CONTEXT,
    CLOGCTL_CMD_SET_OVERRIDE,
    CLOGCTL_CMD_RESET_OVERRIDE,
    CLOGCTL_CMD_GET_LCTX,
    CLOGCTL_CMD_SET_LCTX,
    CLOGCTL_CMD_RESET_LCTX,
    CLOGCTL_CMD_LIST_CONFIGS,
    CLOGCTL_CMD_LIST_SUPPORTED_SINKS,
    CLOGCTL_CMD_LIST_SUPPORTED_LOGLEVELS
} clogctl_cmd_t;

typedef enum {
    CLOGCTL_SETTING_NONE = 0,
    CLOGCTL_SETTING_ADD_CLOGDETAILS_IN_MSG,
    CLOGCTL_SETTING_SINK_FILE_MAX_SIZE_MB,
    CLOGCTL_SETTING_SINK_FILE_ROTATE_COUNT
} clogctl_setting_key_t;

static void usage(const char *prog);

static int parse_uint_strict(const char *cmd, const char *name, const char *s, unsigned long *out)
{
    char *end = NULL;
    unsigned long value;

    if ((cmd == NULL) || (name == NULL) ||
        (s == NULL) || (out == NULL) || (*s == '\0')) {
        return 0;
    }

    errno = 0;
    value = strtoul(s, &end, 10);

    if (errno != 0) {
        fprintf(stderr,
                "clogctl: %s: invalid %s '%s' "
                "(conversion failed, errno=%d)\n",
                cmd, name, s, errno);
        return 0;
    }

    if ((end == s) || (end == NULL) || (*end != '\0')) {
        fprintf(stderr,
                "clogctl: %s: invalid %s '%s' "
                "(expected unsigned integer)\n",
                cmd, name, s);
        return 0;
    }

    *out = value;
    return 1;
}

static int parse_u16_arg(const char *cmd, const char *name, const char *s, unsigned long *out)
{
    if (!parse_uint_strict(cmd, name, s, out)) {
        return 0;
    }
    if (*out > (unsigned long)UINT16_MAX) {
        fprintf(stderr, "clogctl: %s: invalid %s '%s' (expected 0..%u)\n",
                cmd, name, s, (unsigned int)UINT16_MAX);
        return 0;
    }
    return 1;
}

static int parse_sink_arg(const char *cmd, const char *s, unsigned long *out)
{
    if (!parse_uint_strict(cmd, "sink", s, out)) {
        return 0;
    }
    if (*out > (unsigned long)CLOG_BACKEND_FILE) {
        fprintf(stderr, "clogctl: %s: invalid sink '%s' (expected 0..%d; use list-supported-sinks)\n",
                cmd, s, (int)CLOG_BACKEND_FILE);
        return 0;
    }
    return 1;
}

static int parse_level_arg(const char *cmd, const char *s, unsigned long *out)
{
    if (!parse_uint_strict(cmd, "level", s, out)) {
        return 0;
    }
    if (*out > (unsigned long)CLOG_LEVEL_VERBOSE) {
        fprintf(stderr, "clogctl: %s: invalid level '%s' (expected 0..%d; use list-supported-loglevels)\n",
                cmd, s, (int)CLOG_LEVEL_VERBOSE);
        return 0;
    }
    return 1;
}

static int sink_uses_implicit_info(unsigned long sink)
{
    return sink == (unsigned long)CLOG_BACKEND_DIAG ||
           sink == (unsigned long)CLOG_BACKEND_DLT;
}

static unsigned long normalize_sink_level(unsigned long sink, unsigned long level)
{
    return sink_uses_implicit_info(sink) ? (unsigned long)CLOG_LEVEL_INFO : level;
}

static int parse_bool_arg(const char *cmd, const char *name, const char *s, unsigned long *out)
{
    if (!parse_uint_strict(cmd, name, s, out)) {
        return 0;
    }
    if (*out > 1ul) {
        fprintf(stderr, "clogctl: %s: invalid %s '%s' (expected 0 or 1)\n",
                cmd, name, s);
        return 0;
    }
    return 1;
}

static clogctl_setting_key_t parse_setting_key(const char *key)
{
    if (strcmp(key, "add_clogdetails_in_msg") == 0) {
        return CLOGCTL_SETTING_ADD_CLOGDETAILS_IN_MSG;
    }
    if (strcmp(key, "sink_file_max_size_mb") == 0) {
        return CLOGCTL_SETTING_SINK_FILE_MAX_SIZE_MB;
    }
    if (strcmp(key, "sink_file_rotate_count") == 0) {
        return CLOGCTL_SETTING_SINK_FILE_ROTATE_COUNT;
    }
    return CLOGCTL_SETTING_NONE;
}

static int wrong_argc(const char *prog, const char *cmd)
{
    fprintf(stderr, "clogctl: %s: wrong number of arguments\n", cmd);
    usage(prog);
    return 1;
}

static const char *backend_to_str(clog_backend_t sink)
{
    switch (sink) {
        case CLOG_BACKEND_CONSOLE: return "CONSOLE";
        case CLOG_BACKEND_DLT:     return "DLT";
        case CLOG_BACKEND_DIAG:    return "DIAG";
        case CLOG_BACKEND_SYSLOG:  return "SYSLOG";
        case CLOG_BACKEND_LOGCAT:  return "LOGCAT";
        case CLOG_BACKEND_FILE:    return "FILE";
        default:                   return "UNKNOWN";
    }
}

static const char *level_to_str(clog_level_t level)
{
    switch (level) {
        case CLOG_LEVEL_OFF:     return "OFF";
        case CLOG_LEVEL_FATAL:   return "FATAL";
        case CLOG_LEVEL_ERROR:   return "ERROR";
        case CLOG_LEVEL_WARN:    return "WARN";
        case CLOG_LEVEL_INFO:    return "INFO";
        case CLOG_LEVEL_DEBUG:   return "DEBUG";
        case CLOG_LEVEL_VERBOSE: return "VERBOSE";
        default:                 return "UNKNOWN";
    }
}

static void print_sink_and_level(const char *prefix, clog_backend_t sink, clog_level_t level)
{
    if (sink == CLOG_BACKEND_DIAG || sink == CLOG_BACKEND_DLT) {
        printf("%sCLOG SINK=%d(%s)\n", prefix, (int)sink, backend_to_str(sink));
    } else {
        printf("%sCLOG SINK=%d(%s), CLOG LOGLEVEL=%d(%s)\n",
               prefix, (int)sink, backend_to_str(sink), (int)level, level_to_str(level));
    }
}

static void print_settings(const cfg_settings_t *settings)
{
    printf("SETTINGS:\n");
    printf("  add_clogdetails_in_msg=%u\n", (unsigned int)settings->add_clogdetails_in_msg);
    printf("  sink_file_max_size_mb=%u\n", (unsigned int)settings->sink_file_max_size_mb);
    printf("  sink_file_rotate_count=%u\n", (unsigned int)settings->sink_file_rotate_count);
}

static int report_reset_status(const char *cmd, cfg_status_t rc)
{
    if (rc == CFG_ERR_NOT_FOUND) {
        fprintf(stderr, "clogctl: %s failed: %s\n", cmd, cfg_status_str(rc));
        return 1;
    }
    if (!CFG_STATUS_IS_OK(rc)) {
        fprintf(stderr, "clogctl: %s failed: %s\n", cmd, cfg_status_str(rc));
        return 1;
    }

    printf("clogctl: %s: %s\n", cmd, cfg_status_str(rc));
    return 0;
}

static int cmd_get_global(void)
{
    clog_backend_t sink;
    clog_level_t level;

    if (cfg_get_global(&sink, &level) != CFG_OK) {
        fprintf(stderr, "clogctl: get-global failed\n");
        return 1;
    }
    print_sink_and_level("GLOBAL ", sink, level);
    return 0;
}

static int cmd_set_global(unsigned long sink, unsigned long level)
{
    cfg_status_t rc = cfg_set_global((clog_backend_t)sink,
                                     (clog_level_t)normalize_sink_level(sink, level));

    if (!CFG_STATUS_IS_OK(rc)) {
        fprintf(stderr, "clogctl: set-global failed: %s\n", cfg_status_str(rc));
        return 1;
    }
    printf("clogctl: set-global: %s\n", cfg_status_str(rc));
    return cmd_get_global();
}

static int cmd_get_settings(void)
{
    cfg_settings_t settings;

    if (cfg_get_settings(&settings) != CFG_OK) {
        fprintf(stderr, "clogctl: get-settings failed\n");
        return 1;
    }
    print_settings(&settings);
    return 0;
}

static int cmd_set_setting(clogctl_setting_key_t key, const char *key_name, unsigned long value)
{
    cfg_settings_t settings;
    cfg_status_t rc;

    if (cfg_get_settings(&settings) != CFG_OK) {
        fprintf(stderr, "clogctl: set-setting failed (unable to read current settings)\n");
        return 1;
    }

    switch (key) {
        case CLOGCTL_SETTING_ADD_CLOGDETAILS_IN_MSG:
            settings.add_clogdetails_in_msg = (uint8_t)value;
            break;
        case CLOGCTL_SETTING_SINK_FILE_MAX_SIZE_MB:
            settings.sink_file_max_size_mb = (uint16_t)value;
            break;
        case CLOGCTL_SETTING_SINK_FILE_ROTATE_COUNT:
            settings.sink_file_rotate_count = (uint16_t)value;
            break;
        default:
            fprintf(stderr, "clogctl: set-setting failed (unknown key '%s')\n", key_name);
            return 1;
    }

    rc = cfg_set_settings(&settings);
    if (!CFG_STATUS_IS_OK(rc)) {
        fprintf(stderr, "clogctl: set-setting failed: %s\n", cfg_status_str(rc));
        return 1;
    }
    printf("clogctl: set-setting %s: %s\n", key_name, cfg_status_str(rc));
    return cmd_get_settings();
}

static int cmd_get_app(unsigned long app_id)
{
    clog_backend_t sink;
    clog_level_t level;
    char prefix[64] = {0};

    if (cfg_get_app((clog_app_id_t)app_id, &sink, &level) != CFG_OK) {
        fprintf(stderr, "clogctl: get-app failed (no record for app %lu)\n", app_id);
        return 1;
    }
    if (snprintf(prefix, sizeof(prefix), "APP %lu ", app_id) < 0)
    {
        prefix[0] = '\0';
    }
    print_sink_and_level(prefix, sink, level);
    return 0;
}

static int cmd_set_app(unsigned long app_id, unsigned long sink, unsigned long level)
{
    cfg_status_t rc = cfg_set_app((clog_app_id_t)app_id,
                                  (clog_backend_t)sink,
                                  (clog_level_t)normalize_sink_level(sink, level));

    if (rc == CFG_ERR_LIMIT) {
        fprintf(stderr, "clogctl: set-app failed (app section is full, max %u entries)\n", CFG_MAX_APP_COUNT);
        return 1;
    }
    if (!CFG_STATUS_IS_OK(rc)) {
        fprintf(stderr, "clogctl: set-app failed: %s\n", cfg_status_str(rc));
        return 1;
    }
    printf("clogctl: set-app: %s\n", cfg_status_str(rc));
    return cmd_get_app(app_id);
}

static int cmd_reset_app(unsigned long app_id)
{
    return report_reset_status("reset-app",
            cfg_remove_app((clog_app_id_t)app_id));
}

static int cmd_get_context(unsigned long app_id, unsigned long context_id)
{
    clog_level_t level;

    if (cfg_get_resolved_context_level((clog_app_id_t)app_id,
                                       (clog_context_id_t)context_id,
                                       &level) != CFG_OK) {
        fprintf(stderr, "clogctl: get-context failed\n");
        return 1;
    }
    printf("CONTEXT app=%lu context=%lu level=%d(%s)\n",
           app_id, context_id, (int)level, level_to_str(level));
    return 0;
}

static int cmd_set_override(unsigned long app_id, unsigned long context_id, unsigned long level)
{
    cfg_status_t rc = cfg_set_app_context_override((clog_app_id_t)app_id, (clog_context_id_t)context_id, (clog_level_t)level);

    if (rc == CFG_ERR_LIMIT) {
        fprintf(stderr, "clogctl: set-override failed (override section is full, max %u entries)\n", CFG_MAX_OVERRIDE_COUNT);
        return 1;
    }
    if (!CFG_STATUS_IS_OK(rc)) {
        fprintf(stderr, "clogctl: set-override failed: %s\n", cfg_status_str(rc));
        return 1;
    }
    printf("clogctl: set-override: %s\n", cfg_status_str(rc));
    return cmd_get_context(app_id, context_id);
}

static int cmd_reset_override(unsigned long app_id, unsigned long context_id)
{
    return report_reset_status("reset-override",
            cfg_remove_app_context_override((clog_app_id_t)app_id,
                                            (clog_context_id_t)context_id));
}

static int cmd_get_lctx(unsigned long context_id)
{
    clog_level_t level;

    if (cfg_get_lctx((clog_context_id_t)context_id, &level) != CFG_OK) {
        fprintf(stderr, "clogctl: get-lctx failed (no record for context %lu)\n", context_id);
        return 1;
    }
    printf("LCTX %lu level=%d(%s)\n", context_id, (int)level, level_to_str(level));
    return 0;
}

static int cmd_set_lctx(unsigned long context_id, unsigned long level)
{
    cfg_status_t rc = cfg_set_lctx((clog_context_id_t)context_id, (clog_level_t)level);

    if (rc == CFG_ERR_LIMIT) {
        fprintf(stderr, "clogctl: set-lctx failed (lctx section is full, max %u entries)\n", CFG_MAX_LCTX_COUNT);
        return 1;
    }
    if (!CFG_STATUS_IS_OK(rc)) {
        fprintf(stderr, "clogctl: set-lctx failed: %s\n", cfg_status_str(rc));
        return 1;
    }
    printf("clogctl: set-lctx: %s\n", cfg_status_str(rc));
    return cmd_get_lctx(context_id);
}

static int cmd_reset_lctx(unsigned long context_id)
{
    return report_reset_status("reset-lctx",
            cfg_remove_lctx((clog_context_id_t)context_id));
}

static int cmd_list_supported_sinks(void)
{
    clog_backend_t sink;

    printf("SUPPORTED SINKS (%d):\n ", (int)CLOG_BACKEND_FILE + 1);
    for (sink = CLOG_BACKEND_CONSOLE; sink <= CLOG_BACKEND_FILE; sink++) {
        printf(" %d(%s)", (int)sink, backend_to_str(sink));
    }
    printf("\n");

    return 0;
}

static int cmd_list_supported_loglevels(void)
{
    clog_level_t level;

    printf("SUPPORTED LEVELS (%d):\n ", (int)CLOG_LEVEL_VERBOSE + 1);
    for (level = CLOG_LEVEL_OFF; level <= CLOG_LEVEL_VERBOSE; level++) {
        printf(" %d(%s)", (int)level, level_to_str(level));
    }
    printf("\n");

    return 0;
}

static int cmd_list_configs(void)
{
    uint32_t count, i;
    clog_backend_t g_sink;
    clog_level_t g_level;
    cfg_settings_t settings;

    cmd_list_supported_sinks();
    cmd_list_supported_loglevels();

    if (cfg_get_settings(&settings) == CFG_OK) {
        print_settings(&settings);
    }

    if (cfg_get_global(&g_sink, &g_level) == CFG_OK) {
        print_sink_and_level("GLOBAL ", g_sink, g_level);
    }

    if (cfg_get_app_count(&count) != CFG_OK) {
        fprintf(stderr, "clogctl: list-configs failed (app enumeration)\n");
        return 1;
    }
    printf("APPS (%u):\n", count);
    for (i = 0; i < count; i++) {
        clog_app_id_t app_id;
        clog_backend_t sink;
        clog_level_t level;
        char prefix[64] = {0};

        if (cfg_get_app_at(i, &app_id, &sink, &level) != CFG_OK) {
            continue;
        }
	if (snprintf(prefix, sizeof(prefix), " %u, ", app_id) < 0)
        {
            prefix[0] = '\0';
        }
        print_sink_and_level(prefix, sink, level);
    }

    if (cfg_get_lctx_count(&count) != CFG_OK) {
        fprintf(stderr, "clogctl: list-configs failed (lctx enumeration)\n");
        return 1;
    }
    printf("LCTX (%u):\n", count);
    for (i = 0; i < count; i++) {
        clog_context_id_t context_id;
        clog_level_t level;

        if (cfg_get_lctx_at(i, &context_id, &level) != CFG_OK) {
            continue;
        }
        printf("  CLOG CONTEXTID=%u, CLOG LOGLEVEL=%d(%s)\n", context_id, (int)level, level_to_str(level));
    }

    if (cfg_get_override_count(&count) != CFG_OK) {
        fprintf(stderr, "clogctl: list-configs failed (override enumeration)\n");
        return 1;
    }
    printf("OVERRIDES (%u):\n", count);
    for (i = 0; i < count; i++) {
        clog_app_id_t app_id;
        clog_context_id_t context_id;
        clog_level_t level;

        if (cfg_get_override_at(i, &app_id, &context_id, &level) != CFG_OK) {
            continue;
        }
        printf("  CLOG APPID=%u, CLOG CONTEXTID=%u, CLOG LOGLEVEL=%d(%s)\n",
               app_id, context_id, (int)level, level_to_str(level));
    }

    return 0;
}

static void usage(const char *prog)
{
    fprintf(stderr,
        "usage: %s get-global\n"
        "       %s set-global <sink> [<level>]\n"
        "       %s get-settings\n"
        "       %s set-setting <key> <value>\n"
        "       %s get-app <app_id>\n"
        "       %s set-app <app_id> <sink> [<level>]\n"
        "       %s reset-app <app_id>\n"
        "       %s get-context <app_id> <context_id>\n"
        "       %s set-override <app_id> <context_id> <level>\n"
        "       %s reset-override <app_id> <context_id>\n"
        "       %s get-lctx <context_id>\n"
        "       %s set-lctx <context_id> <level>\n"
        "       %s reset-lctx <context_id>\n"
        "       %s list-configs\n"
        "       %s list-supported-sinks\n"
        "       %s list-supported-loglevels\n"
        "note: for DLT/DIAG sinks, <level> is optional for set-global/set-app\n"
        "      and INFO is persisted for consistency.\n",
        prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog);
}

int main(int argc, char *argv[])
{
    const char *cmd;
    const char *setting_name = NULL;
    unsigned long a1 = 0, a2 = 0, a3 = 0;
    clogctl_cmd_t action;
    clogctl_setting_key_t setting_key;
    int ret;

    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }
    cmd = argv[1];
    action = CLOGCTL_CMD_NONE;
    setting_key = CLOGCTL_SETTING_NONE;

    if (strcmp(cmd, "list-supported-sinks") == 0) {
        if (argc != 2) {
            return wrong_argc(argv[0], cmd);
        }
        action = CLOGCTL_CMD_LIST_SUPPORTED_SINKS;
    } else if (strcmp(cmd, "list-supported-loglevels") == 0) {
        if (argc != 2) {
            return wrong_argc(argv[0], cmd);
        }
        action = CLOGCTL_CMD_LIST_SUPPORTED_LOGLEVELS;
    } else if (strcmp(cmd, "get-global") == 0) {
        if (argc != 2) {
            return wrong_argc(argv[0], cmd);
        }
        action = CLOGCTL_CMD_GET_GLOBAL;
    } else if (strcmp(cmd, "set-global") == 0) {
        if (argc != 3 && argc != 4) {
            return wrong_argc(argv[0], cmd);
        }
        if (!parse_sink_arg(cmd, argv[2], &a1)) {
            return 1;
        }
        if (sink_uses_implicit_info(a1)) {
            if (argc == 4 && !parse_level_arg(cmd, argv[3], &a2)) {
                return 1;
            }
            a2 = (unsigned long)CLOG_LEVEL_INFO;
        } else {
            if (argc != 4) {
                fprintf(stderr, "clogctl: %s: level is required unless sink is DLT or DIAG\n", cmd);
                return 1;
            }
            if (!parse_level_arg(cmd, argv[3], &a2)) {
                return 1;
            }
        }
        action = CLOGCTL_CMD_SET_GLOBAL;
    } else if (strcmp(cmd, "get-settings") == 0) {
        if (argc != 2) {
            return wrong_argc(argv[0], cmd);
        }
        action = CLOGCTL_CMD_GET_SETTINGS;
    } else if (strcmp(cmd, "set-setting") == 0) {
        if (argc != 4) {
            return wrong_argc(argv[0], cmd);
        }
        setting_name = argv[2];
        setting_key = parse_setting_key(setting_name);
        if (setting_key == CLOGCTL_SETTING_NONE) {
            fprintf(stderr, "clogctl: %s: unknown setting key '%s'\n", cmd, setting_name);
            return 1;
        }
        if (setting_key == CLOGCTL_SETTING_ADD_CLOGDETAILS_IN_MSG) {
            if (!parse_bool_arg(cmd, "value", argv[3], &a1)) {
                return 1;
            }
        } else if (!parse_u16_arg(cmd, "value", argv[3], &a1)) {
            return 1;
        }
        if (setting_key == CLOGCTL_SETTING_SINK_FILE_MAX_SIZE_MB && a1 == 0ul) {
            fprintf(stderr, "clogctl: %s: invalid value '%s' (sink_file_max_size_mb must be > 0)\n",
                    cmd, argv[3]);
            return 1;
        }
        action = CLOGCTL_CMD_SET_SETTING;
    } else if (strcmp(cmd, "get-app") == 0) {
        if (argc != 3) {
            return wrong_argc(argv[0], cmd);
        }
        if (!parse_u16_arg(cmd, "app_id", argv[2], &a1)) {
            return 1;
        }
        action = CLOGCTL_CMD_GET_APP;
    } else if (strcmp(cmd, "set-app") == 0) {
        if (argc != 4 && argc != 5) {
            return wrong_argc(argv[0], cmd);
        }
        if (!parse_u16_arg(cmd, "app_id", argv[2], &a1) ||
            !parse_sink_arg(cmd, argv[3], &a2)) {
            return 1;
        }
        if (sink_uses_implicit_info(a2)) {
            if (argc == 5 && !parse_level_arg(cmd, argv[4], &a3)) {
                return 1;
            }
            a3 = (unsigned long)CLOG_LEVEL_INFO;
        } else {
            if (argc != 5) {
                fprintf(stderr, "clogctl: %s: level is required unless sink is DLT or DIAG\n", cmd);
                return 1;
            }
            if (!parse_level_arg(cmd, argv[4], &a3)) {
                return 1;
            }
        }
        action = CLOGCTL_CMD_SET_APP;
    } else if (strcmp(cmd, "reset-app") == 0) {
        if (argc != 3) {
            return wrong_argc(argv[0], cmd);
        }
        if (!parse_u16_arg(cmd, "app_id", argv[2], &a1)) {
            return 1;
        }
        action = CLOGCTL_CMD_RESET_APP;
    } else if (strcmp(cmd, "get-context") == 0) {
        if (argc != 4) {
            return wrong_argc(argv[0], cmd);
        }
        if (!parse_u16_arg(cmd, "app_id", argv[2], &a1) ||
            !parse_u16_arg(cmd, "context_id", argv[3], &a2)) {
            return 1;
        }
        action = CLOGCTL_CMD_GET_CONTEXT;
    } else if (strcmp(cmd, "set-override") == 0) {
        if (argc != 5) {
            return wrong_argc(argv[0], cmd);
        }
        if (!parse_u16_arg(cmd, "app_id", argv[2], &a1) ||
            !parse_u16_arg(cmd, "context_id", argv[3], &a2) ||
            !parse_level_arg(cmd, argv[4], &a3)) {
            return 1;
        }
        action = CLOGCTL_CMD_SET_OVERRIDE;
    } else if (strcmp(cmd, "reset-override") == 0) {
        if (argc != 4) {
            return wrong_argc(argv[0], cmd);
        }
        if (!parse_u16_arg(cmd, "app_id", argv[2], &a1) ||
            !parse_u16_arg(cmd, "context_id", argv[3], &a2)) {
            return 1;
        }
        action = CLOGCTL_CMD_RESET_OVERRIDE;
    } else if (strcmp(cmd, "get-lctx") == 0) {
        if (argc != 3) {
            return wrong_argc(argv[0], cmd);
        }
        if (!parse_u16_arg(cmd, "context_id", argv[2], &a1)) {
            return 1;
        }
        action = CLOGCTL_CMD_GET_LCTX;
    } else if (strcmp(cmd, "set-lctx") == 0) {
        if (argc != 4) {
            return wrong_argc(argv[0], cmd);
        }
        if (!parse_u16_arg(cmd, "context_id", argv[2], &a1) ||
            !parse_level_arg(cmd, argv[3], &a2)) {
            return 1;
        }
        action = CLOGCTL_CMD_SET_LCTX;
    } else if (strcmp(cmd, "reset-lctx") == 0) {
        if (argc != 3) {
            return wrong_argc(argv[0], cmd);
        }
        if (!parse_u16_arg(cmd, "context_id", argv[2], &a1)) {
            return 1;
        }
        action = CLOGCTL_CMD_RESET_LCTX;
    } else if (strcmp(cmd, "list-configs") == 0) {
        if (argc != 2) {
            return wrong_argc(argv[0], cmd);
        }
        action = CLOGCTL_CMD_LIST_CONFIGS;
    } else {
        fprintf(stderr, "clogctl: unknown command '%s'\n", cmd);
        usage(argv[0]);
        return 1;
    }

    switch (action) {
        case CLOGCTL_CMD_LIST_SUPPORTED_SINKS:
            return cmd_list_supported_sinks();

        case CLOGCTL_CMD_LIST_SUPPORTED_LOGLEVELS:
            return cmd_list_supported_loglevels();

        default:
            break;
    }

    switch (action) {
        case CLOGCTL_CMD_GET_GLOBAL:
            ret = cmd_get_global();
            break;
        case CLOGCTL_CMD_SET_GLOBAL:
            ret = cmd_set_global(a1, a2);
            break;
        case CLOGCTL_CMD_GET_SETTINGS:
            ret = cmd_get_settings();
            break;
        case CLOGCTL_CMD_SET_SETTING:
            ret = cmd_set_setting(setting_key, setting_name, a1);
            break;
        case CLOGCTL_CMD_GET_APP:
            ret = cmd_get_app(a1);
            break;
        case CLOGCTL_CMD_SET_APP:
            ret = cmd_set_app(a1, a2, a3);
            break;
        case CLOGCTL_CMD_RESET_APP:
            ret = cmd_reset_app(a1);
            break;
        case CLOGCTL_CMD_GET_CONTEXT:
            ret = cmd_get_context(a1, a2);
            break;
        case CLOGCTL_CMD_SET_OVERRIDE:
            ret = cmd_set_override(a1, a2, a3);
            break;
        case CLOGCTL_CMD_RESET_OVERRIDE:
            ret = cmd_reset_override(a1, a2);
            break;
        case CLOGCTL_CMD_GET_LCTX:
            ret = cmd_get_lctx(a1);
            break;
        case CLOGCTL_CMD_SET_LCTX:
            ret = cmd_set_lctx(a1, a2);
            break;
        case CLOGCTL_CMD_RESET_LCTX:
            ret = cmd_reset_lctx(a1);
            break;
        case CLOGCTL_CMD_LIST_CONFIGS:
            ret = cmd_list_configs();
            break;
        default:
            ret = 1;
            break;
    }

    return ret;
}
