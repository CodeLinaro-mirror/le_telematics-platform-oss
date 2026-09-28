/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef PLATFORMSOMEIPSVR_HPP
#define PLATFORMSOMEIPSVR_HPP

#include "interfaces.h"
#include "platformSvcIF.hpp"

#include <chrono>
#include <condition_variable>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <mutex>
#include <vsomeip/vsomeip.hpp>

#define DEFAULT_SERVICE_REF_CNT 16
#define DEFAULT_RXMSG_REF_CNT 16
#define DEFAULT_RXHANDLER_REF_CNT 16
#define DEFAULT_SUBSHANDLER_REF_CNT 32
#define DEFAULT_MAX_PENDING_MSG_CNT 512

//--------------------------------------------------------------------------------------------------
/**
 * Platform VSOMEIP Generic event messages.
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    VS_RX_MSG_PTR,
    VS_RX_MSG_REF,
    VS_SUBS_HANDLE,
}VsMsgType_t;

typedef struct
{
    void* ref;
    vsomeip::client_t clientId;
    uid_t uId;
    gid_t gId;
    bool isSubscribed;
}VsSubsHandle_t;

typedef struct
{
    VsMsgType_t type;
    union
    {
        void* ptr;
        void* ref;
        VsSubsHandle_t handle;
    };
}VsMsg_t;

//--------------------------------------------------------------------------------------------------
/**
 * SOME/IP service handler structure.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    teleplat_someipSvr_ServiceRef_t serviceRef;          ///< Service reference.
    teleplat_someipSvr_RxMsgHandlerFunc_t func;          ///< Handler function.
    void* context;                                  ///< Handler context.
    teleplat_someipSvr_RxMsgHandlerRef_t ref;            ///< own reference.
}SomeipSvr_Handler_t;

//--------------------------------------------------------------------------------------------------
/**
 * SOME/IP group subscribe handler structure.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    teleplat_someipSvr_ServiceRef_t serviceRef;          ///< Service reference.
    uint8_t routingId;                              ///< Routing Identifier.
    uint16_t serviceId;                             ///< Service Identifier.
    uint16_t instanceId;                            ///< Instance Identifier.
    uint16_t groupId;                               ///< EventGroup Identifier.
    teleplat_someipSvr_SubscriptionHandlerFunc_t func;   ///< Handler function.
    void* context;                                  ///< Handler context.
    teleplat_someipSvr_SubscriptionHandlerRef_t ref;     ///< own reference.
}SomeipSvr_SubscriptionHandler_t;

//--------------------------------------------------------------------------------------------------
/**
 * SOME/IP service structure.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    uint8_t routingId;                              ///< Routing Identifier.
    uint16_t serviceId;                             ///< Service Identifier.
    uint16_t instanceId;                            ///< Instance Identifier.
    uint8_t majorVersion;                           ///< Major Version.
    uint32_t minorVersion;                          ///< Minor version.
    uint16_t udpPort;                               ///< UDP port.
    uint16_t tcpPort;                               ///< TCP port.
    bool isMagicCookieEnabled;                      ///< If TCP magic cookie is enabled.
    bool isOffered;                                 ///< If the service is offered.
    le_dls_List_t rxMsgList;                        ///< Pending Rx message list of the service.
    le_dls_List_t eventList;                        ///< Enabled events list of the service.
    le_msg_SessionRef_t sessionRef;                 ///< Client that represents the service.
    teleplat_someipSvr_RxMsgHandlerRef_t handlerRef;     ///< Rx Message Handler of the service.
    teleplat_someipSvr_ServiceRef_t ref;                 ///< own reference.
}SomeipSvr_Service_t;

//--------------------------------------------------------------------------------------------------
/**
 * SOME/IP event group structure.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    le_dls_Link_t link;                   ///< Link to event group list.
    uint16_t groupId;                     ///< Event Group Identifier.
}SomeipSvr_EventGroup_t;

//--------------------------------------------------------------------------------------------------
/**
 * SOME/IP event structure.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    le_dls_Link_t link;                   ///< Link to event list.
    uint16_t eventId;                     ///< Event Identifier.
    platform_someipDef_EventType_t eventType;  ///< Event Type.
    uint32_t cycleTime;                   ///< Cyclic sending time for EVENT notification.
    le_dls_List_t eventGroupList;         ///< Group list which contain the event.
    bool isEnabled;                       ///< If the event is enabled.
    bool isOffered;                       ///< If the event is offered.
    teleplat_someipSvr_ServiceRef_t serviceRef;///< Reference to the service of the event.
}SomeipSvr_Event_t;

//--------------------------------------------------------------------------------------------------
/**
 * SOME/IP Rx message.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    le_dls_Link_t link;                                 ///< Link to the Rx message list.
    teleplat_someipSvr_RxMsgRef_t ref;                       ///< own reference.
    uint8_t routingId;                                  ///< Routing Identifier.
    uint16_t serviceId;                                 ///< Service Identifier.
    uint16_t instanceId;                                ///< Instance Identifier.
    uint16_t methodId;                                  ///< Method Identifier.
    uint16_t clientId;                                  ///< Client Identifier.
    uint16_t sessionId;                                 ///< Session Identifier.
    uint8_t protocolVer;                                ///< Protocol Version.
    uint8_t interfaceVer;                               ///< Interface Version.
    uint8_t  msgType;                                   ///< Message Type.
    bool isReliable;                                    ///< Is received from reliable port.
    uint32_t payloadSize;                               ///< Payload Size.
    uint8_t payloadData[PLATFORM_SOMEIPDEF_MAX_PAYLOAD_SIZE];///< Payload Data Buffer.
}SomeipSvr_RxMsg_t;

//--------------------------------------------------------------------------------------------------
/**
 * SOME/IP Server Service Class
 */
//--------------------------------------------------------------------------------------------------
    namespace platformsvc
    {
        class PlatformSomeipSvr : public IPlatformSvc
        {
            public:
                PlatformSomeipSvr() {};
                ~PlatformSomeipSvr() {};
                void Init();

                // Static member functions.
                static PlatformSomeipSvr &GetInstance();
                static void OnClientDisconnection(le_msg_SessionRef_t sessionRef,
                                                         void *contextPtr);
                static void RxEventHandler(void* reportPtr);
                static void EventObjDestructor(void* objPtr);
                static void ServiceObjDestructor(void* objPtr);

                // Public methods.
                void VSOMEIPHandler(uint8_t routingId,
                                         const std::shared_ptr<vsomeip::message>& msg);
                bool VSOMEIPSubsHandler(teleplat_someipSvr_SubscriptionHandlerRef_t subsHandlerRef,
                                              vsomeip::client_t clientId, uid_t uId, gid_t gId,
                                              bool isSubscribed);

                void ProcessRxMsgRef(void* msgRef);
                void ProcessRxMsgPtr(void* msgPtr);
                void ProcessSubsHandle(VsSubsHandle_t handle);

                teleplat_someipSvr_ServiceRef_t GetServiceRef(uint8_t routingId, uint16_t serviceId,
                                                             uint16_t instanceId);
                le_result_t SetServiceVersion(teleplat_someipSvr_ServiceRef_t serviceRef,
                                                    uint8_t majVer,
                                                    uint32_t minVer);
                le_result_t SetServicePort(teleplat_someipSvr_ServiceRef_t serviceRef,
                                                uint16_t udpPort,
                                                uint16_t tcpPort,
                                                bool enableMagicCookies);
                le_result_t OfferService(teleplat_someipSvr_ServiceRef_t serviceRef);
                le_result_t StopOfferService(teleplat_someipSvr_ServiceRef_t serviceRef);
                le_result_t SetEventType(teleplat_someipSvr_ServiceRef_t serviceRef,
                                             uint16_t eventId,
                                             platform_someipDef_EventType_t eventType);
                le_result_t SetEventCycleTime(teleplat_someipSvr_ServiceRef_t serviceRef,
                                                    uint16_t eventId,
                                                    uint32_t cycleTime);
                le_result_t EnableEvent(teleplat_someipSvr_ServiceRef_t serviceRef,
                                            uint16_t eventId,
                                            uint16_t eventgroupId);
                le_result_t DisableEvent(teleplat_someipSvr_ServiceRef_t serviceRef,
                                             uint16_t eventId);
                le_result_t OfferEvent(teleplat_someipSvr_ServiceRef_t serviceRef,
                                          uint16_t eventId);
                le_result_t StopOfferEvent(teleplat_someipSvr_ServiceRef_t serviceRef,
                                                uint16_t eventId);
                le_result_t Notify(teleplat_someipSvr_ServiceRef_t serviceRef,
                                     uint16_t eventId,
                                     const uint8_t* dataPtr,
                                     size_t dataSize);
                teleplat_someipSvr_RxMsgHandlerRef_t AddRxMsgHandler(
                    teleplat_someipSvr_ServiceRef_t serviceRef,
                    teleplat_someipSvr_RxMsgHandlerFunc_t handlerPtr,
                    void* contextPtr);
                void RemoveRxMsgHandler(teleplat_someipSvr_RxMsgHandlerRef_t handlerRef);
                teleplat_someipSvr_SubscriptionHandlerRef_t AddSubscriptionHandler(
                    teleplat_someipSvr_ServiceRef_t serviceRef,
                    uint16_t eventGroupId,
                    teleplat_someipSvr_SubscriptionHandlerFunc_t handlerPtr,
                    void* contextPtr);
                void RemoveSubscriptionHandler(teleplat_someipSvr_SubscriptionHandlerRef_t handlerRef);

                le_result_t GetServiceId(teleplat_someipSvr_RxMsgRef_t msgRef,
                                             uint16_t* serviceIdPtr,
                                             uint16_t* instanceIdPtr);
                le_result_t GetMethodId(teleplat_someipSvr_RxMsgRef_t msgRef,
                                            uint16_t* methodIdPtr);
                le_result_t GetClientId(teleplat_someipSvr_RxMsgRef_t msgRef,
                                            uint16_t* clientIdPtr);
                le_result_t GetMsgType(teleplat_someipSvr_RxMsgRef_t msgRef,
                                          uint8_t* msgTypePtr);
                le_result_t GetPayloadSize(teleplat_someipSvr_RxMsgRef_t msgRef,
                                                uint32_t* payloadSizePtr);
                le_result_t GetPayloadData(teleplat_someipSvr_RxMsgRef_t msgRef,
                                                uint8_t* dataPtr,
                                                size_t* dataSizePtr);
                le_result_t SendResponse(teleplat_someipSvr_RxMsgRef_t msgRef,
                                             bool isErrRsp,
                                             uint8_t returnCode,
                                             const uint8_t* dataPtr,
                                             size_t dataSize);
                le_result_t ReleaseRxMsg(teleplat_someipSvr_RxMsgRef_t msgRef);

            private:
                // Internal search functions.
                SomeipSvr_Service_t* SearchServiceInList(    uint8_t routingId,
                                                               uint16_t serviceId,
                                                               uint16_t instanceId);
                SomeipSvr_Event_t* SearchEventInList    (le_dls_List_t* eventListPtr,
                                                           uint16_t eventId);
                SomeipSvr_EventGroup_t* SearchEventGroupInList(le_dls_List_t* eventGroupListPtr,
                                                                      uint16_t eventGroupId);
                SomeipSvr_SubscriptionHandler_t* SearchSubscriptionHandlerInList(
                    teleplat_someipSvr_ServiceRef_t serviceRef,uint16_t groupId);

                // Internal clear functions.
                void ClearEventList(le_dls_List_t* eventListPtr);
                void ClearEventGroupList(le_dls_List_t* eventGroupListPtr);
                void ClearRxMessageList(le_dls_List_t* msgListPtr);

                // Internal VSOMEIP interface functions.
                void VSOMEIPOfferService(SomeipSvr_Service_t* servicePtr);
                void VSOMEIPStopOfferService(SomeipSvr_Service_t* servicePtr);
                void VSOMEIPStopOfferAllEvents(SomeipSvr_Service_t* servicePtr);
                void VSOMEIPOfferEvent(SomeipSvr_Event_t* eventPtr);
                void VSOMEIPStopOfferEvent(SomeipSvr_Event_t* eventPtr);
                void VSOMEIPSendResponse(SomeipSvr_RxMsg_t* reqPtr, bool isErrRsp,
                                               uint8_t returnCode, const uint8_t* dataPtr,
                                               size_t dataSize);
                void VSOMEIPNotify(SomeipSvr_Event_t* eventPtr,
                                       const uint8_t* dataPtr, size_t dataSize);

                // Service and event object.
                le_ref_MapRef_t ServiceRefMap;
                le_mem_PoolRef_t ServicePool;
                le_mem_PoolRef_t EventPool;
                le_mem_PoolRef_t EventGroupPool;

                // Rx message object.
                le_ref_MapRef_t RxMsgRefMap;
                le_mem_PoolRef_t RxMsgPool;

                // Rx message handler object.
                le_ref_MapRef_t RxHandlerRefMap;
                le_mem_PoolRef_t RxHandlerPool;

                // Subscription handler object.
                le_ref_MapRef_t SubsHandlerRefMap;
                le_mem_PoolRef_t SubsHandlerPool;

                // VSOMEIP interface.
                le_event_Id_t VsomeipEvent;
                le_event_HandlerRef_t VsomeipEventHandlerRef;
        };
    }
#endif /* #ifndef PLATFORMSOMEIP_HPP */
