/*
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

/*
 * cfg.c - binary configuration file reader/writer.
 *
 * The library always operates directly on /etc/commonlog.conf.
 * The file is assumed to be writable and persistent. Writes are performed
 * in place, followed by fflush() and fsync(). No temporary file, rename,
 * fallback, or tmpfs staging logic is used.
 */

#include "cfg.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef USE_GLIB
#define __packed __attribute__((packed))
#include <glib.h>
#define strlcpy g_strlcpy
#endif

typedef struct {
    cfg_header_t   header;
    cfg_global_t   global;
    cfg_settings_t settings;
    cfg_app_t      *apps;
    cfg_override_t *overrides;
    cfg_lctx_t     *lctxs;
} cfg_data_t;

#define CFG_DEFAULT_FILE_PATH "/etc/commonlog.conf"

static void init_default_settings(cfg_settings_t *settings)
{
    settings->add_clogdetails_in_msg = (uint8_t)CFG_DEFAULT_ADD_CLOGDETAILS_IN_MSG;
    settings->reserved0 = 0u;
    settings->sink_file_max_size_mb = (uint16_t)CFG_DEFAULT_SINK_FILE_MAX_SIZE_MB;
    settings->sink_file_rotate_count = (uint16_t)CFG_DEFAULT_SINK_FILE_ROTATE_COUNT;
}

static void normalize_settings(cfg_settings_t *settings)
{
    settings->reserved0 = 0u;
    if (settings->sink_file_max_size_mb == 0u) {
        settings->sink_file_max_size_mb = (uint16_t)CFG_DEFAULT_SINK_FILE_MAX_SIZE_MB;
        settings->sink_file_rotate_count = (uint16_t)CFG_DEFAULT_SINK_FILE_ROTATE_COUNT;
    }
}

static int settings_are_valid(const cfg_settings_t *settings)
{
    return settings != NULL &&
           settings->add_clogdetails_in_msg <= 1u &&
           settings->sink_file_max_size_mb > 0u;
}

static void init_default_data(cfg_data_t *d)
{
    memset(d, 0, sizeof(*d));
    d->header.magic = CFG_MAGIC;
    d->header.version = CFG_VERSION;
    d->global.sink = CLOG_BACKEND_DLT;
    d->global.level = CLOG_LEVEL_INFO;
    init_default_settings(&d->settings);
}

static void free_data(cfg_data_t *d)
{
    free(d->apps);
    free(d->overrides);
    free(d->lctxs);
    d->apps = NULL;
    d->overrides = NULL;
    d->lctxs = NULL;
}

static int write_data_stream(FILE *f, const cfg_data_t *d)
{
    cfg_header_t header = d->header;
    cfg_settings_t settings = d->settings;

    header.magic = CFG_MAGIC;
    header.version = CFG_VERSION;
    normalize_settings(&settings);

    if (fwrite(&header, sizeof(header), 1, f) != 1) return -1;
    if (fwrite(&d->global, sizeof(d->global), 1, f) != 1) return -1;
    if (fwrite(&settings, sizeof(settings), 1, f) != 1) return -1;
    if (d->header.app_count > 0 &&
        fwrite(d->apps, sizeof(cfg_app_t), d->header.app_count, f) != d->header.app_count) return -1;
    if (d->header.override_count > 0 &&
        fwrite(d->overrides, sizeof(cfg_override_t), d->header.override_count, f) != d->header.override_count) return -1;
    if (d->header.lctx_count > 0 &&
        fwrite(d->lctxs, sizeof(cfg_lctx_t), d->header.lctx_count, f) != d->header.lctx_count) return -1;

    if (fflush(f) != 0) return -1;
    if (fsync(fileno(f)) != 0) return -1;
    return 0;
}

static int write_data(const char *path, const cfg_data_t *d)
{
    FILE *f = fopen(path, "wb");
    int saved_errno;

    if (!f) return -1;
    if (write_data_stream(f, d) != 0) {
        saved_errno = errno;
        (void)fclose(f);
        errno = saved_errno;
        return -1;
    }
    return fclose(f) == 0 ? 0 : -1;
}

static int read_data(const char *path, cfg_data_t *out)
{
    FILE *f;

    memset(out, 0, sizeof(*out));
    init_default_settings(&out->settings);
    f = fopen(path, "rb");
    if (!f) return -1;

    if (fread(&out->header, sizeof(out->header), 1, f) != 1 ||
        out->header.magic != CFG_MAGIC ||
        (out->header.version != 1u && out->header.version != CFG_VERSION)) {
        fclose(f);
        return -1;
    }
    if (fread(&out->global, sizeof(out->global), 1, f) != 1) {
        fclose(f);
        return -1;
    }
    if (out->header.version >= CFG_VERSION) {
        if (fread(&out->settings, sizeof(out->settings), 1, f) != 1 ||
            out->settings.add_clogdetails_in_msg > 1u) {
            fclose(f);
            return -1;
        }
        normalize_settings(&out->settings);
        if (!settings_are_valid(&out->settings)) {
            fclose(f);
            return -1;
        }
    }
    if (out->header.app_count > CFG_MAX_APP_COUNT ||
        out->header.override_count > CFG_MAX_OVERRIDE_COUNT ||
        out->header.lctx_count > CFG_MAX_LCTX_COUNT) {
        fclose(f);
        return -1;
    }
    if (out->header.app_count > 0) {
        out->apps = malloc(sizeof(cfg_app_t) * out->header.app_count);
        if (!out->apps || fread(out->apps, sizeof(cfg_app_t), out->header.app_count, f) != out->header.app_count) {
            fclose(f); free_data(out); return -1;
        }
    }
    if (out->header.override_count > 0) {
        out->overrides = malloc(sizeof(cfg_override_t) * out->header.override_count);
        if (!out->overrides || fread(out->overrides, sizeof(cfg_override_t), out->header.override_count, f) != out->header.override_count) {
            fclose(f); free_data(out); return -1;
        }
    }
    if (out->header.lctx_count > 0) {
        out->lctxs = malloc(sizeof(cfg_lctx_t) * out->header.lctx_count);
        if (!out->lctxs || fread(out->lctxs, sizeof(cfg_lctx_t), out->header.lctx_count, f) != out->header.lctx_count) {
            fclose(f); free_data(out); return -1;
        }
    }
    fclose(f);
    return 0;
}

static int ensure_default_file(void)
{
    cfg_data_t d;

    if (access(CFG_DEFAULT_FILE_PATH, F_OK) == 0) {
        return 0;
    }

    init_default_data(&d);
    return write_data(CFG_DEFAULT_FILE_PATH, &d);
}

static int read_data_current(cfg_data_t *out)
{
    errno = 0;
    if (read_data(CFG_DEFAULT_FILE_PATH, out) == 0) {
        return 0;
    }
    if (errno != ENOENT) {
        return -1;
    }
    if (ensure_default_file() != 0) {
        return -1;
    }
    return read_data(CFG_DEFAULT_FILE_PATH, out);
}

static int modify_current(int (*mutate)(cfg_data_t *d, const void *arg), const void *arg)
{
    cfg_data_t d;
    int rc;

    if (read_data_current(&d) != 0) init_default_data(&d);
    d.header.magic = CFG_MAGIC;
    d.header.version = CFG_VERSION;
    normalize_settings(&d.settings);
    rc = mutate(&d, arg);
    if (rc >= 0 && write_data(CFG_DEFAULT_FILE_PATH, &d) != 0) rc = -1;
    free_data(&d);
    return rc;
}

static cfg_status_t modify_status(int rc)
{
    if (rc == 0) return CFG_OK_UPDATED;
    if (rc == 1) return CFG_OK_INSERTED;
    if (rc == 2) return CFG_OK_REMOVED;
    if (rc == -2) return CFG_ERR_LIMIT;
    if (rc == -3) return CFG_ERR_NOT_FOUND;
    return CFG_ERR;
}

const char *cfg_status_str(cfg_status_t status)
{
    switch (status) {
        case CFG_OK: return "OK";
        case CFG_OK_UPDATED: return "OK (existing entry updated)";
        case CFG_OK_INSERTED: return "OK (new entry inserted)";
        case CFG_OK_REMOVED: return "OK (existing entry removed)";
        case CFG_ERR: return "ERROR";
        case CFG_ERR_PARAM: return "ERROR (invalid parameter)";
        case CFG_ERR_LIMIT: return "ERROR (section full, limit reached)";
        case CFG_ERR_NOT_FOUND: return "ERROR (requested entry not found)";
        default: return "UNKNOWN";
    }
}

cfg_status_t cfg_get_global(clog_backend_t *sink, clog_level_t *level)
{
    cfg_data_t d;
    if (!sink || !level) return CFG_ERR_PARAM;
    if (read_data_current(&d) != 0) return CFG_ERR;
    *sink = (clog_backend_t)d.global.sink;
    *level = (clog_level_t)d.global.level;
    free_data(&d);
    return CFG_OK;
}

cfg_status_t cfg_get_settings(cfg_settings_t *settings)
{
    cfg_data_t d;
    if (!settings) return CFG_ERR_PARAM;
    if (read_data_current(&d) != 0) return CFG_ERR;
    *settings = d.settings;
    free_data(&d);
    return CFG_OK;
}

static void resolve_app_from_data(const cfg_data_t *d, clog_app_id_t app_id,
                                  clog_backend_t *sink, clog_level_t *level)
{
    uint32_t i;
    *sink = (clog_backend_t)d->global.sink;
    *level = (clog_level_t)d->global.level;
    for (i = 0; i < d->header.app_count; i++) {
        if (d->apps[i].app_id == app_id) {
            *sink = (clog_backend_t)d->apps[i].sink;
            *level = (clog_level_t)d->apps[i].level;
            break;
        }
    }
}

static void resolve_context_level_from_data(const cfg_data_t *d, clog_app_id_t app_id,
                                            clog_context_id_t context_id, clog_level_t *level)
{
    uint32_t i;
    *level = (clog_level_t)d->global.level;
    for (i = 0; i < d->header.app_count; i++)
        if (d->apps[i].app_id == app_id) { *level = (clog_level_t)d->apps[i].level; break; }
    for (i = 0; i < d->header.lctx_count; i++)
        if (d->lctxs[i].context_id == context_id) { *level = (clog_level_t)d->lctxs[i].level; break; }
    for (i = 0; i < d->header.override_count; i++)
        if (d->overrides[i].app_id == app_id && d->overrides[i].context_id == context_id) {
            *level = (clog_level_t)d->overrides[i].level; break;
        }
}

struct set_global_arg { uint8_t sink; uint8_t level; };
static int mutate_set_global(cfg_data_t *d, const void *arg)
{
    const struct set_global_arg *a = arg;
    d->global.sink = a->sink; d->global.level = a->level; return 0;
}

cfg_status_t cfg_set_global(clog_backend_t sink, clog_level_t level)
{
    struct set_global_arg arg;
    arg.sink = (uint8_t)sink; arg.level = (uint8_t)level;
    return modify_current(mutate_set_global, &arg) >= 0 ? CFG_OK : CFG_ERR;
}

struct set_settings_arg { cfg_settings_t settings; };
static int mutate_set_settings(cfg_data_t *d, const void *arg)
{
    d->settings = ((const struct set_settings_arg *)arg)->settings; return 0;
}

cfg_status_t cfg_set_settings(const cfg_settings_t *settings)
{
    struct set_settings_arg arg;
    if (!settings_are_valid(settings)) return CFG_ERR_PARAM;
    arg.settings = *settings; normalize_settings(&arg.settings);
    return modify_current(mutate_set_settings, &arg) >= 0 ? CFG_OK : CFG_ERR;
}

cfg_status_t cfg_get_app(clog_app_id_t app_id, clog_backend_t *sink, clog_level_t *level)
{
    cfg_data_t d; uint32_t i; cfg_status_t ret = CFG_ERR;
    if (!sink || !level) return CFG_ERR_PARAM;
    if (read_data_current(&d) != 0) return CFG_ERR;
    for (i = 0; i < d.header.app_count; i++) if (d.apps[i].app_id == app_id) {
        *sink = (clog_backend_t)d.apps[i].sink; *level = (clog_level_t)d.apps[i].level; ret = CFG_OK; break;
    }
    free_data(&d); return ret;
}

cfg_status_t cfg_get_resolved_app_sink(clog_app_id_t app_id, clog_backend_t *sink, clog_level_t *level)
{
    cfg_data_t d;
    if (!sink || !level) return CFG_ERR_PARAM;
    if (read_data_current(&d) != 0) return CFG_ERR;
    resolve_app_from_data(&d, app_id, sink, level); free_data(&d); return CFG_OK;
}

struct set_app_arg { clog_app_id_t app_id; uint8_t sink; uint8_t level; };
static int mutate_set_app(cfg_data_t *d, const void *arg)
{
    const struct set_app_arg *a = arg; uint32_t i; cfg_app_t *grown;
    for (i = 0; i < d->header.app_count; i++) if (d->apps[i].app_id == a->app_id) {
        d->apps[i].sink = a->sink; d->apps[i].level = a->level; return 0;
    }
    if (d->header.app_count >= CFG_MAX_APP_COUNT) return -2;
    grown = realloc(d->apps, sizeof(cfg_app_t) * (d->header.app_count + 1));
    if (!grown) return -1;
    d->apps = grown; d->apps[d->header.app_count].app_id = a->app_id;
    d->apps[d->header.app_count].sink = a->sink; d->apps[d->header.app_count].level = a->level;
    d->header.app_count++; return 1;
}

cfg_status_t cfg_set_app(clog_app_id_t app_id, clog_backend_t sink, clog_level_t level)
{
    struct set_app_arg arg;
    arg.app_id = app_id; arg.sink = (uint8_t)sink; arg.level = (uint8_t)level;
    return modify_status(modify_current(mutate_set_app, &arg));
}

struct remove_app_arg { clog_app_id_t app_id; };
static int mutate_remove_app(cfg_data_t *d, const void *arg)
{
    const struct remove_app_arg *a = arg; uint32_t i;
    for (i = 0; i < d->header.app_count; i++) if (d->apps[i].app_id == a->app_id) {
        if (i + 1u < d->header.app_count) memmove(&d->apps[i], &d->apps[i + 1u], sizeof(cfg_app_t) * (d->header.app_count - i - 1u));
        d->header.app_count--; return 2;
    }
    return -3;
}

cfg_status_t cfg_remove_app(clog_app_id_t app_id)
{
    struct remove_app_arg arg;
    arg.app_id = app_id; return modify_status(modify_current(mutate_remove_app, &arg));
}

cfg_status_t cfg_get_resolved_context_level(clog_app_id_t app_id, clog_context_id_t context_id, clog_level_t *level)
{
    cfg_data_t d;
    if (!level) return CFG_ERR_PARAM;
    if (read_data_current(&d) != 0) return CFG_ERR;
    resolve_context_level_from_data(&d, app_id, context_id, level); free_data(&d); return CFG_OK;
}

struct set_override_arg { clog_app_id_t app_id; clog_context_id_t context_id; uint8_t level; };
static int mutate_set_override(cfg_data_t *d, const void *arg)
{
    const struct set_override_arg *a = arg; uint32_t i; cfg_override_t *grown;
    for (i = 0; i < d->header.override_count; i++)
        if (d->overrides[i].app_id == a->app_id && d->overrides[i].context_id == a->context_id) {
            d->overrides[i].level = a->level; return 0;
        }
    if (d->header.override_count >= CFG_MAX_OVERRIDE_COUNT) return -2;
    grown = realloc(d->overrides, sizeof(cfg_override_t) * (d->header.override_count + 1));
    if (!grown) return -1;
    d->overrides = grown; d->overrides[d->header.override_count].app_id = a->app_id;
    d->overrides[d->header.override_count].context_id = a->context_id;
    d->overrides[d->header.override_count].level = a->level; d->overrides[d->header.override_count]._pad = 0;
    d->header.override_count++; return 1;
}

cfg_status_t cfg_set_app_context_override(clog_app_id_t app_id, clog_context_id_t context_id, clog_level_t level)
{
    struct set_override_arg arg;
    arg.app_id = app_id; arg.context_id = context_id; arg.level = (uint8_t)level;
    return modify_status(modify_current(mutate_set_override, &arg));
}

struct remove_override_arg { clog_app_id_t app_id; clog_context_id_t context_id; };
static int mutate_remove_override(cfg_data_t *d, const void *arg)
{
    const struct remove_override_arg *a = arg; uint32_t i;
    for (i = 0; i < d->header.override_count; i++)
        if (d->overrides[i].app_id == a->app_id && d->overrides[i].context_id == a->context_id) {
            if (i + 1u < d->header.override_count) memmove(&d->overrides[i], &d->overrides[i + 1u], sizeof(cfg_override_t) * (d->header.override_count - i - 1u));
            d->header.override_count--; return 2;
        }
    return -3;
}

cfg_status_t cfg_remove_app_context_override(clog_app_id_t app_id, clog_context_id_t context_id)
{
    struct remove_override_arg arg;
    arg.app_id = app_id; arg.context_id = context_id;
    return modify_status(modify_current(mutate_remove_override, &arg));
}

cfg_status_t cfg_get_lctx(clog_context_id_t context_id, clog_level_t *level)
{
    cfg_data_t d; uint32_t i; cfg_status_t ret = CFG_ERR;
    if (!level) return CFG_ERR_PARAM;
    if (read_data_current(&d) != 0) return CFG_ERR;
    for (i = 0; i < d.header.lctx_count; i++) if (d.lctxs[i].context_id == context_id) {
        *level = (clog_level_t)d.lctxs[i].level; ret = CFG_OK; break;
    }
    free_data(&d); return ret;
}

struct set_lctx_arg { clog_context_id_t context_id; uint8_t level; };
static int mutate_set_lctx(cfg_data_t *d, const void *arg)
{
    const struct set_lctx_arg *a = arg; uint32_t i; cfg_lctx_t *grown;
    for (i = 0; i < d->header.lctx_count; i++) if (d->lctxs[i].context_id == a->context_id) {
        d->lctxs[i].level = a->level; return 0;
    }
    if (d->header.lctx_count >= CFG_MAX_LCTX_COUNT) return -2;
    grown = realloc(d->lctxs, sizeof(cfg_lctx_t) * (d->header.lctx_count + 1));
    if (!grown) return -1;
    d->lctxs = grown; d->lctxs[d->header.lctx_count].context_id = a->context_id;
    d->lctxs[d->header.lctx_count].level = a->level; d->lctxs[d->header.lctx_count]._pad = 0;
    d->header.lctx_count++; return 1;
}

cfg_status_t cfg_set_lctx(clog_context_id_t context_id, clog_level_t level)
{
    struct set_lctx_arg arg;
    arg.context_id = context_id; arg.level = (uint8_t)level;
    return modify_status(modify_current(mutate_set_lctx, &arg));
}

struct remove_lctx_arg { clog_context_id_t context_id; };
static int mutate_remove_lctx(cfg_data_t *d, const void *arg)
{
    const struct remove_lctx_arg *a = arg; uint32_t i;
    for (i = 0; i < d->header.lctx_count; i++) if (d->lctxs[i].context_id == a->context_id) {
        if (i + 1u < d->header.lctx_count) memmove(&d->lctxs[i], &d->lctxs[i + 1u], sizeof(cfg_lctx_t) * (d->header.lctx_count - i - 1u));
        d->header.lctx_count--; return 2;
    }
    return -3;
}

cfg_status_t cfg_remove_lctx(clog_context_id_t context_id)
{
    struct remove_lctx_arg arg;
    arg.context_id = context_id; return modify_status(modify_current(mutate_remove_lctx, &arg));
}

cfg_status_t cfg_get_app_count(uint32_t *count)
{
    cfg_data_t d;
    if (!count) return CFG_ERR_PARAM;
    if (read_data_current(&d) != 0) return CFG_ERR;
    *count = d.header.app_count; free_data(&d); return CFG_OK;
}

cfg_status_t cfg_get_app_at(uint32_t index, clog_app_id_t *app_id, clog_backend_t *sink, clog_level_t *level)
{
    cfg_data_t d;
    if (!app_id || !sink || !level) return CFG_ERR_PARAM;
    if (read_data_current(&d) != 0) return CFG_ERR;
    if (index >= d.header.app_count) { free_data(&d); return CFG_ERR_PARAM; }
    *app_id = d.apps[index].app_id; *sink = (clog_backend_t)d.apps[index].sink; *level = (clog_level_t)d.apps[index].level;
    free_data(&d); return CFG_OK;
}

cfg_status_t cfg_get_override_count(uint32_t *count)
{
    cfg_data_t d;
    if (!count) return CFG_ERR_PARAM;
    if (read_data_current(&d) != 0) return CFG_ERR;
    *count = d.header.override_count; free_data(&d); return CFG_OK;
}

cfg_status_t cfg_get_override_at(uint32_t index, clog_app_id_t *app_id, clog_context_id_t *context_id, clog_level_t *level)
{
    cfg_data_t d;
    if (!app_id || !context_id || !level) return CFG_ERR_PARAM;
    if (read_data_current(&d) != 0) return CFG_ERR;
    if (index >= d.header.override_count) { free_data(&d); return CFG_ERR_PARAM; }
    *app_id = d.overrides[index].app_id; *context_id = d.overrides[index].context_id; *level = (clog_level_t)d.overrides[index].level;
    free_data(&d); return CFG_OK;
}

cfg_status_t cfg_get_lctx_count(uint32_t *count)
{
    cfg_data_t d;
    if (!count) return CFG_ERR_PARAM;
    if (read_data_current(&d) != 0) return CFG_ERR;
    *count = d.header.lctx_count; free_data(&d); return CFG_OK;
}

cfg_status_t cfg_get_lctx_at(uint32_t index, clog_context_id_t *context_id, clog_level_t *level)
{
    cfg_data_t d;
    if (!context_id || !level) return CFG_ERR_PARAM;
    if (read_data_current(&d) != 0) return CFG_ERR;
    if (index >= d.header.lctx_count) { free_data(&d); return CFG_ERR_PARAM; }
    *context_id = d.lctxs[index].context_id; *level = (clog_level_t)d.lctxs[index].level;
    free_data(&d); return CFG_OK;
}
