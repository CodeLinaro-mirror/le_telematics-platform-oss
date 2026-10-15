/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * interfaces.h - Replacement for Legato-generated interfaces
 * 
 * This file replaces the Legato-generated interfaces.h that was created
 * from .api files. It contains all the type definitions and function
 * declarations needed by the SOME/IP gateway application.
 */

#ifndef INTERFACES_H
#define INTERFACES_H

#include "teleplat_framework.h"

#ifdef __cplusplus
extern "C" {
#endif

//==================================================================================================
// SOME/IP Definition Constants
//==================================================================================================

#define PLATFORM_SOMEIPDEF_MAX_PAYLOAD_SIZE 65535
#define PLATFORM_SOMEIPDEF_EVENT_MASK 0x8000
#define PLATFORM_SOMEIPDEF_ANY_MAJOR 0xFF
#define PLATFORM_SOMEIPDEF_ANY_MINOR 0xFFFFFFFF
#define PLATFORM_SOMEIPDEF_DEFAULT_MAJOR 0x00
#define PLATFORM_SOMEIPDEF_DEFAULT_MINOR 0x00000000

// Return codes
#define PLATFORM_SOMEIPDEF_E_OK 0x00
#define PLATFORM_SOMEIPDEF_E_NOT_OK 0x01
#define PLATFORM_SOMEIPDEF_E_TIMEOUT 0x02
#define PLATFORM_SOMEIPDEF_E_NOT_REACHABLE 0x03

// Message types
#define PLATFORM_SOMEIPDEF_MT_REQUEST 0x00
#define PLATFORM_SOMEIPDEF_MT_REQUEST_NO_RETURN 0x01
#define PLATFORM_SOMEIPDEF_MT_RESPONSE 0x80
#define PLATFORM_SOMEIPDEF_MT_ERROR 0x81

//==================================================================================================
// SOME/IP Definition Types
//==================================================================================================

/**
 * Event type enumeration
 */
typedef enum
{
    PLATFORM_SOMEIPDEF_ET_EVENT = 0,    ///< Pure event
    PLATFORM_SOMEIPDEF_ET_FIELD = 1     ///< Field notification
} platform_someipDef_EventType_t;

//==================================================================================================
// SOME/IP Server API Types
//==================================================================================================

/**
 * Service reference (opaque)
 */
typedef void* teleplat_someipSvr_ServiceRef_t;

/**
 * Rx message reference (opaque)
 */
typedef void* teleplat_someipSvr_RxMsgRef_t;

/**
 * Rx message handler reference (opaque)
 */
typedef void* teleplat_someipSvr_RxMsgHandlerRef_t;

/**
 * Subscription handler reference (opaque)
 */
typedef void* teleplat_someipSvr_SubscriptionHandlerRef_t;

/**
 * Rx message handler function type
 */
typedef void (*teleplat_someipSvr_RxMsgHandlerFunc_t)
(
    teleplat_someipSvr_RxMsgRef_t msgRef,
    void* contextPtr
);

/**
 * Subscription handler function type
 */
typedef void (*teleplat_someipSvr_SubscriptionHandlerFunc_t)
(
    teleplat_someipSvr_ServiceRef_t serviceRef,
    uint16_t eventGroupId,
    bool isSubscribed,
    void* contextPtr
);

//==================================================================================================
// SOME/IP Server API Functions
//==================================================================================================

/**
 * Get service reference
 */
teleplat_someipSvr_ServiceRef_t teleplat_someipSvr_GetService
(
    uint16_t serviceId,
    uint16_t instanceId
);

teleplat_someipSvr_ServiceRef_t teleplat_someipSvr_GetServiceEx
(
    uint16_t serviceId,
    uint16_t instanceId,
    const char* ifName
);

le_result_t teleplat_someipSvr_SetServiceVersion
(
    teleplat_someipSvr_ServiceRef_t serviceRef,
    uint8_t majVer,
    uint32_t minVer
);

le_result_t teleplat_someipSvr_SetServicePort
(
    teleplat_someipSvr_ServiceRef_t serviceRef,
    uint16_t udpPort,
    uint16_t tcpPort,
    bool enableMagicCookies
);

le_result_t teleplat_someipSvr_OfferService
(
    teleplat_someipSvr_ServiceRef_t serviceRef
);

le_result_t teleplat_someipSvr_StopOfferService
(
    teleplat_someipSvr_ServiceRef_t serviceRef
);

le_result_t teleplat_someipSvr_SetEventType
(
    teleplat_someipSvr_ServiceRef_t serviceRef,
    uint16_t eventId,
    platform_someipDef_EventType_t eventType
);

le_result_t teleplat_someipSvr_SetEventCycleTime
(
    teleplat_someipSvr_ServiceRef_t serviceRef,
    uint16_t eventId,
    uint32_t cycleTime
);

le_result_t teleplat_someipSvr_EnableEvent
(
    teleplat_someipSvr_ServiceRef_t serviceRef,
    uint16_t eventId,
    uint16_t eventgroupId
);

le_result_t teleplat_someipSvr_DisableEvent
(
    teleplat_someipSvr_ServiceRef_t serviceRef,
    uint16_t eventId
);

le_result_t teleplat_someipSvr_OfferEvent
(
    teleplat_someipSvr_ServiceRef_t serviceRef,
    uint16_t eventId
);

le_result_t teleplat_someipSvr_StopOfferEvent
(
    teleplat_someipSvr_ServiceRef_t serviceRef,
    uint16_t eventId
);

le_result_t teleplat_someipSvr_Notify
(
    teleplat_someipSvr_ServiceRef_t serviceRef,
    uint16_t eventId,
    const uint8_t* dataPtr,
    size_t dataSize
);

teleplat_someipSvr_RxMsgHandlerRef_t teleplat_someipSvr_AddRxMsgHandler
(
    teleplat_someipSvr_ServiceRef_t serviceRef,
    teleplat_someipSvr_RxMsgHandlerFunc_t handlerPtr,
    void* contextPtr
);

void teleplat_someipSvr_RemoveRxMsgHandler
(
    teleplat_someipSvr_RxMsgHandlerRef_t handlerRef
);

teleplat_someipSvr_SubscriptionHandlerRef_t teleplat_someipSvr_AddSubscriptionHandler
(
    teleplat_someipSvr_ServiceRef_t serviceRef,
    uint16_t eventGroupId,
    teleplat_someipSvr_SubscriptionHandlerFunc_t handlerPtr,
    void* contextPtr
);

void teleplat_someipSvr_RemoveSubscriptionHandler
(
    teleplat_someipSvr_SubscriptionHandlerRef_t handlerRef
);

le_result_t teleplat_someipSvr_GetServiceId
(
    teleplat_someipSvr_RxMsgRef_t msgRef,
    uint16_t* serviceIdPtr,
    uint16_t* instanceIdPtr
);

le_result_t teleplat_someipSvr_GetMethodId
(
    teleplat_someipSvr_RxMsgRef_t msgRef,
    uint16_t* methodIdPtr
);

le_result_t teleplat_someipSvr_GetClientId
(
    teleplat_someipSvr_RxMsgRef_t msgRef,
    uint16_t* clientIdPtr
);

le_result_t teleplat_someipSvr_GetMsgType
(
    teleplat_someipSvr_RxMsgRef_t msgRef,
    uint8_t* msgTypePtr
);

le_result_t teleplat_someipSvr_GetPayloadSize
(
    teleplat_someipSvr_RxMsgRef_t msgRef,
    uint32_t* payloadSizePtr
);

le_result_t teleplat_someipSvr_GetPayloadData
(
    teleplat_someipSvr_RxMsgRef_t msgRef,
    uint8_t* dataPtr,
    size_t* dataSizePtr
);

le_result_t teleplat_someipSvr_SendResponse
(
    teleplat_someipSvr_RxMsgRef_t msgRef,
    bool isErrRsp,
    uint8_t returnCode,
    const uint8_t* dataPtr,
    size_t dataSize
);

le_result_t teleplat_someipSvr_ReleaseRxMsg
(
    teleplat_someipSvr_RxMsgRef_t msgRef
);

/**
 * Get server service reference (for IPC)
 */
static inline le_msg_ServiceRef_t teleplat_someipSvr_GetServiceRef(void)
{
    // Return a dummy service reference for standalone build
    return (le_msg_ServiceRef_t)1;
}

/**
 * Get client session reference (for IPC)
 */
static inline le_msg_SessionRef_t teleplat_someipSvr_GetClientSessionRef(void)
{
    // Return a dummy session reference for standalone build
    return (le_msg_SessionRef_t)1;
}

//==================================================================================================
// SOME/IP Client API Types
//==================================================================================================

/**
 * Service reference (opaque)
 */
typedef void* teleplat_someipClnt_ServiceRef_t;

/**
 * Tx message reference (opaque)
 */
typedef void* teleplat_someipClnt_TxMsgRef_t;

/**
 * State change handler reference (opaque)
 */
typedef void* teleplat_someipClnt_StateChangeHandlerRef_t;

/**
 * Event message handler reference (opaque)
 */
typedef void* teleplat_someipClnt_EventMsgHandlerRef_t;

/**
 * Service state enumeration
 */
typedef enum
{
    TELEPLAT_SOMEIPCLNT_UNAVAILABLE = 0,    ///< Service unavailable
    TELEPLAT_SOMEIPCLNT_AVAILABLE = 1       ///< Service available
} teleplat_someipClnt_State_t;

/**
 * State change handler function type
 */
typedef void (*teleplat_someipClnt_StateChangeHandlerFunc_t)
(
    teleplat_someipClnt_ServiceRef_t serviceRef,
    teleplat_someipClnt_State_t state,
    void* contextPtr
);

/**
 * Response message handler function type
 */
typedef void (*teleplat_someipClnt_RespMsgHandlerFunc_t)
(
    le_result_t result,
    bool isErrResp,
    uint8_t returnCode,
    const uint8_t* dataPtr,
    size_t dataSize,
    void* contextPtr
);

/**
 * Event message handler function type
 */
typedef void (*teleplat_someipClnt_EventMsgHandlerFunc_t)
(
    teleplat_someipClnt_ServiceRef_t serviceRef,
    uint16_t eventId,
    const uint8_t* dataPtr,
    size_t dataSize,
    void* contextPtr
);

//==================================================================================================
// SOME/IP Client API Functions
//==================================================================================================

uint16_t teleplat_someipClnt_GetClientId(void);

le_result_t teleplat_someipClnt_GetClientIdEx
(
    const char* ifName,
    uint16_t* clientIdPtr
);

teleplat_someipClnt_ServiceRef_t teleplat_someipClnt_RequestService
(
    uint16_t serviceId,
    uint16_t instanceId
);

teleplat_someipClnt_ServiceRef_t teleplat_someipClnt_RequestServiceEx
(
    uint16_t serviceId,
    uint16_t instanceId,
    const char* ifName
);

teleplat_someipClnt_ServiceRef_t teleplat_someipClnt_RequestServiceWithVersion
(
    uint16_t serviceId,
    uint16_t instanceId,
    uint8_t majVer,
    uint32_t minVer,
    const char* ifName
);

le_result_t teleplat_someipClnt_ReleaseService
(
    teleplat_someipClnt_ServiceRef_t serviceRef
);

le_result_t teleplat_someipClnt_GetState
(
    teleplat_someipClnt_ServiceRef_t serviceRef,
    teleplat_someipClnt_State_t* statePtr
);

le_result_t teleplat_someipClnt_GetVersion
(
    teleplat_someipClnt_ServiceRef_t serviceRef,
    uint8_t* majVerPtr,
    uint32_t* minVerPtr
);

teleplat_someipClnt_StateChangeHandlerRef_t teleplat_someipClnt_AddStateChangeHandler
(
    teleplat_someipClnt_ServiceRef_t serviceRef,
    teleplat_someipClnt_StateChangeHandlerFunc_t handlerPtr,
    void* contextPtr
);

void teleplat_someipClnt_RemoveStateChangeHandler
(
    teleplat_someipClnt_StateChangeHandlerRef_t handlerRef
);

teleplat_someipClnt_TxMsgRef_t teleplat_someipClnt_CreateMsg
(
    teleplat_someipClnt_ServiceRef_t serviceRef,
    uint16_t methodId
);

le_result_t teleplat_someipClnt_SetNonRet
(
    teleplat_someipClnt_TxMsgRef_t msgRef
);

le_result_t teleplat_someipClnt_SetReliable
(
    teleplat_someipClnt_TxMsgRef_t msgRef
);

le_result_t teleplat_someipClnt_SetTimeout
(
    teleplat_someipClnt_TxMsgRef_t msgRef,
    uint32_t timeOut
);

le_result_t teleplat_someipClnt_SetPayload
(
    teleplat_someipClnt_TxMsgRef_t msgRef,
    const uint8_t* dataPtr,
    size_t dataSize
);

le_result_t teleplat_someipClnt_DeleteMsg
(
    teleplat_someipClnt_TxMsgRef_t msgRef
);

void teleplat_someipClnt_RequestResponse
(
    teleplat_someipClnt_TxMsgRef_t msgRef,
    teleplat_someipClnt_RespMsgHandlerFunc_t handlerPtr,
    void* contextPtr
);

le_result_t teleplat_someipClnt_EnableEventGroup
(
    teleplat_someipClnt_ServiceRef_t serviceRef,
    uint16_t eventGroupId,
    uint16_t eventId,
    platform_someipDef_EventType_t eventType
);

le_result_t teleplat_someipClnt_DisableEventGroup
(
    teleplat_someipClnt_ServiceRef_t serviceRef,
    uint16_t eventGroupId
);

le_result_t teleplat_someipClnt_SubscribeEventGroup
(
    teleplat_someipClnt_ServiceRef_t serviceRef,
    uint16_t eventGroupId
);

le_result_t teleplat_someipClnt_UnsubscribeEventGroup
(
    teleplat_someipClnt_ServiceRef_t serviceRef,
    uint16_t eventGroupId
);

teleplat_someipClnt_EventMsgHandlerRef_t teleplat_someipClnt_AddEventMsgHandler
(
    teleplat_someipClnt_ServiceRef_t serviceRef,
    uint16_t eventGroupId,
    teleplat_someipClnt_EventMsgHandlerFunc_t handlerPtr,
    void* contextPtr
);

void teleplat_someipClnt_RemoveEventMsgHandler
(
    teleplat_someipClnt_EventMsgHandlerRef_t handlerRef
);

/**
 * Get client service reference (for IPC)
 */
static inline le_msg_ServiceRef_t teleplat_someipClnt_GetServiceRef(void)
{
    // Return a dummy service reference for standalone build
    return (le_msg_ServiceRef_t)2;
}

/**
 * Get client session reference (for IPC)
 */
static inline le_msg_SessionRef_t teleplat_someipClnt_GetClientSessionRef(void)
{
    // Return a dummy session reference for standalone build
    return (le_msg_SessionRef_t)2;
}

#ifdef __cplusplus
}
#endif

#endif // INTERFACES_H