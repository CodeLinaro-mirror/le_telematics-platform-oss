/*
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear
*/

/*
 * clog_qcom_ids.h: registry of numeric CLOG app/context IDs defined by
 * Qualcomm.
 *
 * Reserved range: 1 - 25000 (inclusive), for both clog_app_id_t and
 * clog_context_id_t values. Qualcomm-internal domains are assigned
 * blocks of 1000 within this range (e.g. 1000s for Audio, 2000s for
 * C2C, ...). Do not define IDs > 25000 in this file -- that range is
 * reserved for customers; see clog_customer_ids.h.
 *
 * This file is included by clog_ids.h and should not normally be
 * included directly.
 */

#ifndef CLOG_QCOM_IDS_H
#define CLOG_QCOM_IDS_H

#include "clog.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CLOG Internal / Dev/Demo Ids
 *
 * Range: 1 - 1000. Sample app/context/library-context IDs for clients to
 * use during development/testing, before the platform team assigns them
 * real IDs out of their domain's block. Do not ship production code
 * registered under a CLOG_*_TEST* ID.
 * ============================================================================ */
#define CLOG_APP_TEST1 1
#define CLOG_APP_TEST2 2
#define CLOG_APP_TEST3 3
#define CLOG_APP_TEST4 4

#define CLOG_ACTX_TEST1_1 201
#define CLOG_ACTX_TEST1_2 202
#define CLOG_ACTX_TEST1_3 203

#define CLOG_ACTX_TEST2_1 204
#define CLOG_ACTX_TEST2_2 205

#define CLOG_ACTX_TEST3_1 206
#define CLOG_ACTX_TEST3_2 207
#define CLOG_ACTX_TEST3_3 208

#define CLOG_ACTX_TEST4_1 209
#define CLOG_ACTX_TEST4_2 210

#define CLOG_LCTX_TESTLIB1 601
#define CLOG_LCTX_TESTLIB2 602
#define CLOG_LCTX_TESTLIB3 603
#define CLOG_LCTX_TESTLIB4 604

/* ============================================================================
 * Audio Domain
 * ============================================================================ */
#define CLOG_APP_AGM 1001

#define CLOG_ACTX_AGM_SERV 1201

#define CLOG_LCTX_PALC 1601
#define CLOG_LCTX_SERV 1602
#define CLOG_LCTX_GSL 1603
#define CLOG_LCTX_ACDB 1604
#define CLOG_LCTX_ATS 1605
#define CLOG_LCTX_ARGS 1606
#define CLOG_LCTX_GPR 1607

/* ============================================================================
 * C2C Domain
 * ============================================================================ */
#define CLOG_APP_SKUD 2001
#define CLOG_APP_EHUB 2002

#define CLOG_ACTX_SKUD 2201
#define CLOG_ACTX_EHUB 2202

#define CLOG_LCTX_LQMS 2601
#define CLOG_LCTX_LSKU 2602

/* ============================================================================
 * Core Domain
 * ============================================================================ */
#define CLOG_APP_TFTP 3001
#define CLOG_APP_RMTS 3002
#define CLOG_APP_DRTR 3003
#define CLOG_APP_TIMD 3004

#define CLOG_ACTX_TFTP_GNRL 3201
#define CLOG_ACTX_RMTS_GNRL 3202
#define CLOG_ACTX_DRTR_GNRL 3203
#define CLOG_ACTX_TIMD_GNRL 3204

#define CLOG_LCTX_RPMB 3601
#define CLOG_LCTX_DLIB 3602
#define CLOG_LCTX_QCCI 3603
#define CLOG_LCTX_QCSI 3604

/* ============================================================================
 * Data Domain
 * ============================================================================ */
#define CLOG_APP_QMAP 4001
#define CLOG_APP_NMGR 4002
#define CLOG_APP_EPWM 4003
#define CLOG_APP_QMS 4004
#define CLOG_APP_QIPA 4005
#define CLOG_APP_CLI 4006
#define CLOG_APP_QTID 4007
#define CLOG_APP_ADPL 4008

#define CLOG_ACTX_QMAP_QCMS 4201
#define CLOG_ACTX_QMAP_DSI 4202
#define CLOG_ACTX_QMAP_QDI 4203
#define CLOG_ACTX_NMGR 4204
#define CLOG_ACTX_EPWM 4205
#define CLOG_ACTX_QMS_GNRL 4206
#define CLOG_ACTX_QIPA_GNRL 4207
#define CLOG_ACTX_CLI_GNRL 4208
#define CLOG_ACTX_QTID 4209
#define CLOG_ACTX_ADPL 4210

#define CLOG_LCTX_QCML 4601
#define CLOG_LCTX_DSI 4602
#define CLOG_LCTX_QDI 4603
#define CLOG_LCTX_DSUT 4604
#define CLOG_LCTX_QIPA 4605

/* ============================================================================
 * Location Domain
 * ============================================================================ */
#define CLOG_APP_LHD 5001
#define CLOG_APP_XTRA 5002
#define CLOG_APP_EDGN 5003
#define CLOG_APP_ESDR 5004
#define CLOG_APP_ESPI 5005
#define CLOG_APP_ESED 5006
#define CLOG_APP_ESPP 5007
#define CLOG_APP_PROL 5008
#define CLOG_APP_SLIM 5009
#define CLOG_APP_LCAT 5010
#define CLOG_APP_LIDL 5011

#define CLOG_ACTX_LHD_HALD 5201
#define CLOG_ACTX_LHD_LAV2 5202
#define CLOG_ACTX_LHD_LOCM 5203
#define CLOG_ACTX_LHD_PBMC 5204
#define CLOG_ACTX_LHD_LADP 5205
#define CLOG_ACTX_LHD_GADP 5206
#define CLOG_ACTX_LHD_LSOC 5207
#define CLOG_ACTX_LHD_UTIL 5208
#define CLOG_ACTX_LHD_LBUF 5209
#define CLOG_ACTX_LHD_LIPC 5210
#define CLOG_ACTX_LHD_MTSK 5211
#define CLOG_ACTX_LHD_LCTX 5212
#define CLOG_ACTX_LHD_APIU 5213
#define CLOG_ACTX_LHD_DIFC 5214
#define CLOG_ACTX_LHD_APWR 5215
#define CLOG_ACTX_LHD_UTLT 5216
#define CLOG_ACTX_LHD_LINK 5217
#define CLOG_ACTX_LHD_MISC 5218
#define CLOG_ACTX_LHD_MSGQ 5219
#define CLOG_ACTX_LHD_CTXB 5220
#define CLOG_ACTX_LHD_LSSO 5221
#define CLOG_ACTX_LHD_LABS 5222
#define CLOG_ACTX_LHD_SYSS 5223
#define CLOG_ACTX_LHD_XMLP 5224
#define CLOG_ACTX_LHD_AGPS 5225
#define CLOG_ACTX_LHD_ACLB 5226
#define CLOG_ACTX_LHD_LAPI 5227
#define CLOG_ACTX_LHD_DICT 5228
#define CLOG_ACTX_LHD_UCFG 5229
#define CLOG_ACTX_LHD_NMEA 5230
#define CLOG_ACTX_LHD_XSSO 5231
#define CLOG_ACTX_LHD_GPWR 5232
#define CLOG_ACTX_LHD_NAGP 5233
#define CLOG_ACTX_LHD_XTSS 5234
#define CLOG_ACTX_LHD_EHQW 5235
#define CLOG_ACTX_LHD_EQDG 5236
#define CLOG_ACTX_LHD_EAGG 5237
#define CLOG_ACTX_LHD_EMGR 5238
#define CLOG_ACTX_LHD_LCLO 5239
#define CLOG_ACTX_LHD_LDIF 5240
#define CLOG_ACTX_LHD_LBSR 5241
#define CLOG_ACTX_LHD_LBSA 5242
#define CLOG_ACTX_LHD_IPLL 5243
#define CLOG_ACTX_LHD_ISVU 5244
#define CLOG_ACTX_LHD_LNIB 5245
#define CLOG_ACTX_LHD_LNIH 5246
#define CLOG_ACTX_LHD_ISLP 5247
#define CLOG_ACTX_LHD_LAPB 5248
#define CLOG_ACTX_LHD_OSFW 5249
#define CLOG_ACTX_LHD_SUBS 5250
#define CLOG_ACTX_LHD_LNEL 5251
#define CLOG_ACTX_LHD_IZAX 5252
#define CLOG_ACTX_LHD_IZPR 5253
#define CLOG_ACTX_LHD_EHMU 5254
#define CLOG_ACTX_LHD_IZPP 5255
#define CLOG_ACTX_LHD_WPAC 5256
#define CLOG_ACTX_LHD_IRAP 5257
#define CLOG_ACTX_LHD_IPCH 5258
#define CLOG_ACTX_LHD_MQCL 5259
#define CLOG_ACTX_LHD_LNIF 5260
#define CLOG_ACTX_LHD_LNTI 5261
#define CLOG_ACTX_LHD_SICH 5262
#define CLOG_ACTX_LHD_LIZA 5263
#define CLOG_ACTX_LHD_LIRP 5264
#define CLOG_ACTX_LHD_CDFS 5265
#define CLOG_ACTX_LHD_QCBE 5266
#define CLOG_ACTX_LHD_QCBD 5267
#define CLOG_ACTX_LHD_QCFR 5268
#define CLOG_ACTX_LHD_QCDU 5269
#define CLOG_ACTX_LHD_CDFW 5270
#define CLOG_ACTX_LHD_LIA2 5271
#define CLOG_ACTX_LOC_XTRA 5272
#define CLOG_ACTX_LOC_PALN 5273
#define CLOG_ACTX_EDGN_NTCL 5274
#define CLOG_ACTX_EDGN_NSRC 5275
#define CLOG_ACTX_ENGS_MAIN 5276
#define CLOG_ACTX_ENGS_EPLG 5277
#define CLOG_ACTX_ENGS_MSGC 5278
#define CLOG_ACTX_ENGS_EPCT 5279
#define CLOG_ACTX_ENGS_ESIM 5280
#define CLOG_ACTX_ENGS_QDRE 5281
#define CLOG_ACTX_ENGS_QPPE 5282
#define CLOG_ACTX_ENGS_PPEC 5283
#define CLOG_ACTX_ENGS_EAPI 5284
#define CLOG_ACTX_ENGS_APII 5285
#define CLOG_ACTX_ENGS_EPDIAG 5286
#define CLOG_ACTX_ENGS_QCEP 5287
#define CLOG_ACTX_ENGS_QFED 5288
#define CLOG_ACTX_ENGS_QEPC 5289
#define CLOG_ACTX_ENGS_QFEC 5290
#define CLOG_ACTX_ENGS_QFES 5291
#define CLOG_ACTX_PROL_LOCL 5292
#define CLOG_ACTX_SLIM_HLOS 5293
#define CLOG_ACTX_SLIM_DIAG 5294
#define CLOG_ACTX_SLIM_QCSI 5295
#define CLOG_ACTX_SLIM_QCCI 5296
#define CLOG_ACTX_SLIM_QMFW 5297
#define CLOG_ACTX_SLIM_SLIM 5298
#define CLOG_ACTX_SLIM_QMIS 5299
#define CLOG_ACTX_SLIM_SNDK 5300
#define CLOG_ACTX_SLIM_S1__ 5301
#define CLOG_ACTX_SLIM_MAIN 5302
#define CLOG_ACTX_SLIM_QLOC 5303
#define CLOG_ACTX_SLIM_IPCS 5304
#define CLOG_ACTX_SLIM_VNW_ 5305
#define CLOG_ACTX_SLIM_VNS_ 5306
#define CLOG_ACTX_SLIM_SHDM 5307
#define CLOG_ACTX_SLIM_OTHR 5308
#define CLOG_ACTX_LCAT_LCA_ 5309
#define CLOG_ACTX_LCAT_LIA_ 5310
#define CLOG_ACTX_LCAT_LIAI 5311
#define CLOG_ACTX_LIDL_IDLS 5312
#define CLOG_ACTX_LIDL_IDPH 5313
#define CLOG_ACTX_LIDL_LCAC 5314

/* ============================================================================
 * Platform Domain
 * ============================================================================ */

#define CLOG_APP_QPMD 6001
#define CLOG_APP_TSRS 6002
#define CLOG_APP_RDFS 6003
#define CLOG_APP_QSKU 6004
#define CLOG_APP_NADC 6005
#define CLOG_APP_NADA 6006
#define CLOG_APP_NADT 6007
#define CLOG_APP_SENS 6008
#define CLOG_APP_ISRV 6009
#define CLOG_APP_ISAP 6010

#define CLOG_ACTX_QPMD_GNRL 6201
#define CLOG_ACTX_TSRS_GNRL 6202
#define CLOG_ACTX_RDFS_RDFS_OFST 6203
#define CLOG_ACTX_QSKU_GNRL 6204
#define CLOG_ACTX_NADC_GNRL 6205
#define CLOG_ACTX_NADA_ABCL 6206
#define CLOG_ACTX_NADT_TMRC 6207
#define CLOG_ACTX_SENS_SHD 6208
#define CLOG_ACTX_ISRV_GNRL 6209
#define CLOG_ACTX_ISAP_GNRL 6210

#define CLOG_LCTX_QPMD 6601
#define CLOG_LCTX_ABCL 6602
#define CLOG_LCTX_FDEK 6603
#define CLOG_LCTX_TMRC 6604
#define CLOG_LCTX_ABAL 6605
#define CLOG_LCTX_MTDL 6606
#define CLOG_LCTX_UBIL 6607
#define CLOG_LCTX_OFSR 6608
#define CLOG_LCTX_SCL 6609

/* ============================================================================
 * Security Domain
 * ============================================================================ */
#define CLOG_APP_QWES 7001

#define CLOG_ACTX_QWES_DAEM 7201

/* ============================================================================
 * TelSdk Domain
 * ============================================================================ */

#define CLOG_APP_TAS 8001
#define CLOG_APP_TCS 8002
#define CLOG_APP_TSS 8003
#define CLOG_APP_TTS 8004
#define CLOG_APP_TPR 8005
#define CLOG_APP_RILD 8006
#define CLOG_APP_RSIM 8007
#define CLOG_APP_SAPD 8008
#define CLOG_APP_RSPD 8009
#define CLOG_APP_CHSK 8010

#define CLOG_ACTX_RILD_GNRL 8201
#define CLOG_ACTX_RSIM_GNRL 8202
#define CLOG_ACTX_SAPD_GNRL 8203
#define CLOG_ACTX_RSPD_GNRL 8204
#define CLOG_ACTX_CHSK_GNRL 8205

#define CLOG_LCTX_TLX 8601
#define CLOG_LCTX_TLX_AUD 8602
#define CLOG_LCTX_TLX_V2X 8603
#define CLOG_LCTX_TLX_DATA 8604
#define CLOG_LCTX_TLX_LOC 8605
#define CLOG_LCTX_TLX_PWR 8606
#define CLOG_LCTX_TLX_TEL 8607
#define CLOG_LCTX_TLX_THER 8608
#define CLOG_LCTX_TLX_SENS 8609
#define CLOG_LCTX_TLX_PLT 8610
#define CLOG_LCTX_TLX_SEC 8611
#define CLOG_LCTX_TLX_SAT 8612
#define CLOG_LCTX_QRIL 8613

/* ============================================================================
 * Thermal Domain
 * ============================================================================ */

#define CLOG_APP_THRM 9001

#define CLOG_ACTX_THRM_GNRL 9201

#define CLOG_LCTX_TCLI 9601


/* ============================================================================
 * VNW Domain
 * ============================================================================ */

#define CLOG_APP_AVBL 10001
#define CLOG_APP_AVBT 10002
#define CLOG_APP_QAVB 10003
#define CLOG_APP_MAVB 10004
#define CLOG_APP_AVBS 10005
#define CLOG_APP_AVBH 10006
#define CLOG_APP_AVBC 10007
#define CLOG_APP_GPTP 10008
#define CLOG_APP_LPTP 10009

#define CLOG_ACTX_AVBL_AVTP 10201
#define CLOG_ACTX_AVBT_AVTP 10202
#define CLOG_ACTX_QAVB_AVTP 10203
#define CLOG_ACTX_MAVB_AVTP 10204
#define CLOG_ACTX_AVBS_AVBH 10205
#define CLOG_ACTX_AVBH_AVBH 10206
#define CLOG_ACTX_AVBC_AVTP 10207
#define CLOG_ACTX_GPTP_GPTP 10208
#define CLOG_ACTX_LPTP_GPTP 10209

#define CLOG_LCTX_GPTP 10601
#define CLOG_LCTX_AVTP 10602
#define CLOG_LCTX_AVBH 10603

/* ============================================================================
 * WLAN/BT Domain
 * ============================================================================ */
#define CLOG_APP_WLAN 11001

#define CLOG_ACTX_WLAN_GNRL 11201

/* ============================================================================
 * IPC Logging Domain
 * ============================================================================ */
#define CLOG_APP_IPCL 12001

#include "clog_actx_ipcl.h"

#ifdef __cplusplus
}
#endif

#endif /* CLOG_QCOM_IDS_H */
