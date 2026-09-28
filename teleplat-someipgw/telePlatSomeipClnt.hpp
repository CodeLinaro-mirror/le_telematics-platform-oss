/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef PLATFORMSOMEIPCLNT_HPP
#define PLATFORMSOMEIPCLNT_HPP

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

#define CLNT_SERVICE_REF_CNT 16
#define CLNT_TXMSG_REF_CNT 16
#define CLNT_STATE_HANDLER_REF_CNT 16
#define CLNT_EVENT_HANDLER_REF_CNT 16
#define CLNT_MAX_TXNID_CNT 512

//--------------------------------------------------------------------------------------------------
/**
 * Platform VSOMEIP Generic event messages.
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    VS_MSG_STATE,                                   ///< VSOMEIP State message.
    VS_MSG_RESPONSE,                                ///< VSOMEIP Response message.
    VS_MSG_EVENT,                                   ///< VSOMEIP Event message.
}VsClntMsgType_t;

typedef struct
{
    uint8_t routingId;
    uint16_t serviceId;
    uint16_t instanceId;
    uint8_t majVer;
    uint32_t minVer;
    bool isAvailable;
}VsState_t;

typedef struct
{
    VsClntMsgType_t type;
    union
    {
        void* msgPtr;
        VsState_t state;
    };
}VsClntMsg_t;

//--------------------------------------------------------------------------------------------------
/**
 * SOME/IP client-service-instance state.
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    SERVICE_STATE_UNKNOWN,                          ///< Service state is unknown.
    SERVICE_STATE_UNAVAILABLE,                      ///< Service state is unavailable.
    SERVICE_STATE_AVAILABLE,                        ///< Service state is available.
}ServiceState_t;

//--------------------------------------------------------------------------------------------------
/**
 * SOME/IP common header in a request/response message.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    uint8_t  routingId;                             ///< Routing Identifier.
    uint16_t serviceId;                             ///< Service Identifier.
    uint16_t instanceId;                            ///< Instance Identifier.
    uint16_t methodId;                              ///< Method Identifier.
    uint16_t sessionId;                             ///< Session Identifier.
    uint16_t clientId;                              ///< Client Identifier.
    uint8_t  interfaceVer;                          ///< Interface version of the service.
}CommonMsgHdr_t;

//--------------------------------------------------------------------------------------------------
/**
 * SOME/IP payload data in a request/response message.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    size_t size;                                    ///< payload data size.
    uint8_t data[PLATFORM_SOMEIPDEF_MAX_PAYLOAD_SIZE];   ///< payload data buffer.
}Payload_t;

//--------------------------------------------------------------------------------------------------
/**
 * SOME/IP response message struct.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    CommonMsgHdr_t msgHdr;                          ///< Common message header.
    bool isErrResp;                                 ///< If an error response.
    uint8_t returnCode;                             ///< Return code.
    Payload_t* payloadPtr;                          ///< Payload data.
}SomeipClnt_RespMsg_t;

//--------------------------------------------------------------------------------------------------
/**
 * SOME/IP event message struct.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    CommonMsgHdr_t msgHdr;                          ///< Common message header.
    Payload_t* payloadPtr;                          ///< Payload data.
}SomeipClnt_EventMsg_t;

//--------------------------------------------------------------------------------------------------
/**
 * SOME/IP request message struct.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    le_dls_Link_t link;                             ///< Link to the list.
    teleplat_someipClnt_TxMsgRef_t ref;                  ///< Self reference.
    teleplat_someipClnt_ServiceRef_t serviceRef;         ///< Server reference.
    uint16_t methodId;                              ///< Method ID.
    bool isNonRet;                                  ///< If it's non-return request.
    bool isReliable;                                ///< Send message via TCP or UDP.
    uint32_t timeoutMs;                             ///< Timeout milliseconds for response.
    Payload_t* payloadPtr;                          ///< Payload data.
}SomeipClnt_TxMsg_t;

//--------------------------------------------------------------------------------------------------
/**
 * SOME/IP Transaction ID struct. Used to track a request-response pair.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    le_dls_Link_t link;                             ///< Link to the list.
    CommonMsgHdr_t msgHdr;                          ///< Common message header.
    teleplat_someipClnt_ServiceRef_t serviceRef;         ///< Service reference.
    le_timer_Ref_t timerRef;                        ///< Response waiting timer.
    teleplat_someipClnt_RespMsgHandlerFunc_t respHandleFunc; ///< Response handler function.
    void* contextPtr;                               ///< Function context.
}Txn_t;

//--------------------------------------------------------------------------------------------------
/**
 * SOME/IP service state change handler struct.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    le_dls_Link_t link;                             ///< Link to the list.
    teleplat_someipClnt_StateChangeHandlerRef_t ref;     ///< Self reference.
    teleplat_someipClnt_ServiceRef_t serviceRef;         ///< Service reference.
    teleplat_someipClnt_StateChangeHandlerFunc_t handlefunc; ///< State change handler function.
    void* context;                                  ///< Function context.
}StateHandler_t;

//--------------------------------------------------------------------------------------------------
/**
 * SOME/IP client-service-instace struct.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    le_dls_Link_t link;                             ///< Link to the list.
    uint8_t routingId;                              ///< Routing Identifier.
    uint16_t serviceId;                             ///< Service Identifier.
    uint16_t instanceId;                            ///< Instance Identifier.
    ServiceState_t state;                           ///< Service state.
    uint8_t reqMajVer;                              ///< Requested major version.
    uint32_t reqMinVer;                             ///< Requested minor version.
    uint8_t majorVersion;                           ///< Current major version.
    uint32_t minorVersion;                          ///< Current minor version.
    le_dls_List_t groupList;                        ///< Enabled Event group list.
    le_dls_List_t eventList;                        ///< Enabled Event list
}SomeipClnt_Service_t;

//--------------------------------------------------------------------------------------------------
/**
 * Platform client-service-session struct. Presents a client which requesting a SOME/IP service.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    teleplat_someipClnt_ServiceRef_t ref;                ///< Self reference.
    le_msg_SessionRef_t clientRef;                  ///< Client session reference.
    SomeipClnt_Service_t* servicePtr;               ///< Service object pointer.
    ServiceState_t state;                           ///< Service state.
    le_dls_List_t eventHandlerList;                 ///< Event group handler.
    le_dls_List_t stateHandlerList;                 ///< State handler list.
    le_dls_List_t txMsgList;                        ///< Tx message list(unsent messages).
    le_dls_List_t txnIdList;                        ///< Txn IDs list.
}SvcClient_t;

//--------------------------------------------------------------------------------------------------
/**
 * SOME/IP event message handler struct.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    le_dls_Link_t link;                             ///< Link to the list.
    teleplat_someipClnt_EventMsgHandlerRef_t ref;        ///< Self reference.
    teleplat_someipClnt_ServiceRef_t serviceRef;         ///< Service reference.
    uint16_t groupId;                               ///< Event group Identifier.
    teleplat_someipClnt_EventMsgHandlerFunc_t handlefunc;///< State change handler function.
    void* context;                                  ///< Function context.
}EventHandler_t;

//--------------------------------------------------------------------------------------------------
/**
 * SOME/IP event group structure.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    le_dls_Link_t link;                             ///< Link to the group list of the service.
    uint16_t groupId;                               ///< Event Group Identifier.
    bool setForSubscription;                        ///< If subscription is required.
    bool isSubscribed;                              ///< If subscription is done in VSOMEIP layer.
    le_msg_SessionRef_t clientRef;                  ///< Client session reference.
    SomeipClnt_Service_t* servicePtr;               ///< Service object pointer.
}SomeipClnt_Group_t;

//--------------------------------------------------------------------------------------------------
/**
 * SOME/IP event structure.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    le_dls_Link_t link;                             ///< Link to event list of the service.
    uint16_t eventId;                               ///< Event Identifier.
    uint16_t groupId;                               ///< Group Identifier the event is added to.
    platform_someipDef_EventType_t eventType;            ///< Event Type.
    le_msg_SessionRef_t clientRef;                  ///< Client session reference.
    SomeipClnt_Service_t* servicePtr;               ///< Service object pointer.
}SomeipClnt_Event_t;

//--------------------------------------------------------------------------------------------------
/**
 * SOME/IP Client Service Class
 */
//--------------------------------------------------------------------------------------------------
    namespace platformsvc
    {
        class PlatformSomeipClient: public IPlatformSvc
        {
            public:
                PlatformSomeipClient() {};
                ~PlatformSomeipClient() {};
                void Init();

                // Static member functions.
                static PlatformSomeipClient &GetInstance();
                static void PlatformClientDisconnection(le_msg_SessionRef_t sessionRef, void *contextPtr);
                static void PlatformVsMsgHandler(void* reportPtr);
                static void PlatformServiceDestructor(void* objPtr);
                static void PlatformGroupDestructor(void* objPtr);
                static void PlatformTxnTimerExpiryHandler(void* timerRef);

                // Static hash functions.
                static size_t HashComputeTxnId(const void* keyPtr);
                static bool HashCompareTxnIds(const void* firstKeyPtr, const void* secondKeyPtr);

                // Public methods used in VSOMEIP handlers or context.
                void VSOMEIPRespHandler(uint8_t routingId,
                                              const std::shared_ptr<vsomeip::message>& msg);
                void VSOMEIPEventHandler(uint8_t routingId,
                                               const std::shared_ptr<vsomeip::message>& msg);
                void VSOMEIPStateHandler(uint8_t routingId, uint16_t serviceId,
                                               uint16_t instanceId, bool isAvailable);

                // Public methods used in generic service layer.
                void ClientDisconnection(le_msg_SessionRef_t sessionRef, void *contextPtr);
                void ServiceDestructor(void* objPtr);
                void GroupDestructor(void* objPtr);
                void ProcessStateChangeMessage(VsState_t serviceState);
                void ProcessResponseMessage(void* msgPtr);
                void ProcessEventMessage(void* msgPtr);
                void TxnTimerExpiryHandler(le_timer_Ref_t timerRef);

                // Pubilc methods for API handler wrapper.
                uint16_t GetClientId(uint8_t routingId);
                teleplat_someipClnt_ServiceRef_t RequestService(uint8_t routingId, uint16_t serviceId,
                                                                uint16_t instanceId, uint8_t majVer,
                                                                uint32_t minVer);
                le_result_t ReleaseService(teleplat_someipClnt_ServiceRef_t serviceRef);
                le_result_t GetState(teleplat_someipClnt_ServiceRef_t serviceRef,
                                        teleplat_someipClnt_State_t* statePtr);
                le_result_t GetVersion(teleplat_someipClnt_ServiceRef_t serviceRef,
                                          uint8_t* majVerPtr,
                                          uint32_t* minVerPtr);
                teleplat_someipClnt_StateChangeHandlerRef_t AddStateChangeHandler(
                    teleplat_someipClnt_ServiceRef_t serviceRef,
                    teleplat_someipClnt_StateChangeHandlerFunc_t handlerPtr,
                    void* contextPtr);
                void RemoveStateChangeHandler(teleplat_someipClnt_StateChangeHandlerRef_t handlerRef);
                teleplat_someipClnt_TxMsgRef_t CreateMsg(teleplat_someipClnt_ServiceRef_t serviceRef,
                                                       uint16_t methodId);
                le_result_t SetNonRet(teleplat_someipClnt_TxMsgRef_t msgRef);

                le_result_t SetReliable(teleplat_someipClnt_TxMsgRef_t msgRef);
                le_result_t SetTimeout(teleplat_someipClnt_TxMsgRef_t msgRef, uint32_t timeOut);
                le_result_t SetPayload(teleplat_someipClnt_TxMsgRef_t msgRef, const uint8_t* dataPtr,
                                          size_t dataSize);
                le_result_t DeleteMsg(teleplat_someipClnt_TxMsgRef_t msgRef);
                void RequestResponse(teleplat_someipClnt_TxMsgRef_t msgRef,
                                          teleplat_someipClnt_RespMsgHandlerFunc_t handlerPtr,
                                          void* contextPtr);
                le_result_t EnableEventGroup(teleplat_someipClnt_ServiceRef_t serviceRef,
                                                  uint16_t groupId,
                                                  uint16_t eventId,
                                                  platform_someipDef_EventType_t eventType);
                le_result_t DisableEventGroup(teleplat_someipClnt_ServiceRef_t serviceRef,
                                                    uint16_t groupId);
                le_result_t SubscribeEventGroup(teleplat_someipClnt_ServiceRef_t serviceRef,
                                                      uint16_t groupId);
                le_result_t UnsubscribeEventGroup(teleplat_someipClnt_ServiceRef_t serviceRef,
                                                         uint16_t groupId);
                teleplat_someipClnt_EventMsgHandlerRef_t AddEventMsgHandler(
                    teleplat_someipClnt_ServiceRef_t serviceRef,
                    uint16_t groupId,
                    teleplat_someipClnt_EventMsgHandlerFunc_t handlerPtr,
                    void* contextPtr);
                void RemoveEventMsgHandler(teleplat_someipClnt_EventMsgHandlerRef_t handlerRef);

            private:
                // Internal functions.
                SomeipClnt_Service_t* FindService(uint8_t routingId, uint16_t serviceId,
                                                      uint16_t instanceId);
                SvcClient_t* FindSvcClient(SomeipClnt_Service_t* serverPtr,
                                               le_msg_SessionRef_t clientSessionRef);
                bool IsSvcClientReleasable(SvcClient_t* svcClientPtr);
                void CleanupClientResources(SvcClient_t* svcClientPtr);
                void NotifyServiceStateChange(SvcClient_t* svcClientPtr,
                                                      teleplat_someipClnt_State_t state);
                void NotifyEventMessage(SvcClient_t* svcClientPtr,
                                              uint16_t groupId,
                                              uint16_t eventId,
                                              Payload_t* payloadPtr);
                SomeipClnt_Group_t* FindGroup(le_dls_List_t* groupListPtr, uint16_t groupId);
                SomeipClnt_Event_t* FindEvent(le_dls_List_t* eventListPtr, uint16_t eventId);
                void ClearEventsInGroup(le_dls_List_t* eventListPtr, uint16_t groupId,
                                        le_msg_SessionRef_t clientSessionRef);
                void ClearGroups(le_dls_List_t* groupListPtr, le_msg_SessionRef_t clientSessionRef);

                // Internal VSOMEIP interface functions.
                void VSOMEIPRequestService(SomeipClnt_Service_t* servicePtr);
                void VSOMEIPReleaseService(SomeipClnt_Service_t* servicePtr);
                void VSOMEIPSendRequest(CommonMsgHdr_t* msgHdrPtr,bool isNonRetMsg,
                                              bool isReliable, Payload_t* payloadPtr);
                bool VSOMEIPGetServiceInfo(uint8_t routingId, uint16_t serviceId,
                                                  uint16_t instanceId, uint8_t* majVerPtr,
                                                  uint32_t* minVerPtr);
                void VSOMEIPSubscribeEventGroup(SomeipClnt_Group_t* groupPtr);
                void VSOMEIPUnsubscribeEventGroup(SomeipClnt_Group_t* groupPtr);

                // Global Txn hash map and Txn object.
                le_hashmap_Ref_t TxnHashMapRef;
                le_mem_PoolRef_t TxnPool;

                // Global service list and service object.
                le_dls_List_t ServiceList;
                le_mem_PoolRef_t ServicePool;

                // Client session object that requests a service.
                le_ref_MapRef_t SvcClientRefMap;
                le_mem_PoolRef_t SvcClientPool;

                // Tx message object.
                le_ref_MapRef_t TxMsgRefMap;
                le_mem_PoolRef_t TxMsgPool;

                // State handler object.
                le_ref_MapRef_t StateHandlerRefMap;
                le_mem_PoolRef_t StateHandlerPool;

                // Payload in Tx/Resp message.
                le_mem_PoolRef_t PayloadPool;

                // Response message object.
                le_mem_PoolRef_t RespMsgPool;

                // Event message object.
                le_mem_PoolRef_t EventMsgPool;

                // Event and Group object.
                le_mem_PoolRef_t GroupPool;
                le_mem_PoolRef_t EventPool;

                // Event handler object.
                le_ref_MapRef_t EventHandlerRefMap;
                le_mem_PoolRef_t EventHandlerPool;

                // VSOMEIP interface.
                le_event_Id_t VsMsgEvent;
                le_event_HandlerRef_t VsMsgEventHandlerRef;
        };
    }
#endif /* #ifndef PLATFORMSOMEIP_HPP */
