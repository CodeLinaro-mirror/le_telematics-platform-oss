/*
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

/*
 * clog_customer_ids.h: registry of numeric CLOG app/context IDs defined
 * by the customer (OEM).
 *
 * Reserved range: 25001 - 65535 (inclusive), for both clog_app_id_t and
 * clog_context_id_t values. Qualcomm-defined IDs live in
 * clog_qcom_ids.h and occupy 1 - 25000; do not define IDs in that
 * range here, to avoid colliding with Qualcomm-defined or
 * future Qualcomm-reserved values.
 *
 * Add customer app/context IDs below, following the same pattern as
 * clog_qcom_ids.h: group related IDs under a domain comment banner,
 * and give every app/context a unique value within 25001 - 65535.
 *
 * This file is included by clog_ids.h.
 */

#ifndef CLOG_CUSTOMER_IDS_H
#define CLOG_CUSTOMER_IDS_H

#include "clog.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Customer-defined IDs go here. Example:
 *
 * #define CLOG_APP_MYAPP 25001
 * #define CLOG_ACTX_MYAPP_GNRL 25201
 * ============================================================================ */

#ifdef __cplusplus
}
#endif

#endif /* CLOG_CUSTOMER_IDS_H */
