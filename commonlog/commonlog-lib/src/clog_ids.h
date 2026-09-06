/*
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

/*
 * clog_ids.h: platform-wide registry of numeric CLOG app/context IDs.
 *
 * clog_register_app() / clog_register_context() both accept a numeric
 * clog_app_id_t / clog_context_id_t identifier that CLOG stores but does
 * not otherwise interpret. Without a shared registry, every consumer
 * invents its own value, and nothing stops two unrelated components
 * from picking the same one by accident.
 *
 * The registry is split across two headers so Qualcomm-defined and
 * customer-defined IDs can never collide:
 *
 *   - clog_qcom_ids.h    IDs defined by Qualcomm.    Range: 1 - 25000
 *   - clog_customer_ids.h  IDs defined by the customer. Range: 25001 - 65535
 *
 * Any application or library using CLOG should #include this header
 * and pass the matching named constant to clog_register_app() /
 * CLOG_REGISTER_APP() / clog_register_context() / CLOG_REGISTER_CONTEXT()
 * instead of a locally hand-picked literal.
 */

#ifndef CLOG_IDS_H
#define CLOG_IDS_H

#include "clog.h"
#include "clog_qcom_ids.h"
#include "clog_customer_ids.h"

#endif /* CLOG_IDS_H */
