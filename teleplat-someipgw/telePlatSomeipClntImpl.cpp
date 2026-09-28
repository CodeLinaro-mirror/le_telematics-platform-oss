/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <syslog.h>
#include <assert.h>
#include "teleplat_framework.h"
#include "telePlatSomeipClnt.hpp"
#include "telePlatSomeipGWSvc.hpp"

using namespace platformsvc;
using namespace std;

//--------------------------------------------------------------------------------------------------
/**
 * Get the single instance of Platform SOME/IP client class.
 */
//--------------------------------------------------------------------------------------------------
PlatformSomeipClient& PlatformSomeipClient::GetInstance()
{
    static PlatformSomeipClient instance;
    return instance;
}

//--------------------------------------------------------------------------------------------------
/**
 * Generic VSOME/IP response message handler.
 *
 * Each VSOME/IP response is mapped to a legato event report object through le_event_Report().
 */
//--------------------------------------------------------------------------------------------------
void PlatformSomeipClient::VSOMEIPRespHandler
(
    uint8_t routingId,
    const std::shared_ptr<vsomeip::message>& msg
)
{
    // Validate the client ID.
    uint16_t routClientId = someip_GetClientId(routingId);
    uint16_t msgClientId = msg->get_client();
    if (routClientId != msgClientId)
    {
        syslog(LOG_ERR,"routClientId(0x%x) mismatches msgClientId(0x%x), dropped the resp message.",
                  routClientId, msgClientId);
        return;
    }

    // Create a Platform SOME/IP response message to hold the VSOMEIP response message.
    SomeipClnt_RespMsg_t* respMsgPtr = (SomeipClnt_RespMsg_t*)le_mem_ForceAlloc(RespMsgPool);
    memset(respMsgPtr, 0, sizeof(SomeipClnt_RespMsg_t));

    // Fill the txnId.
    respMsgPtr->msgHdr.routingId = routingId;
    respMsgPtr->msgHdr.serviceId = msg->get_service();
    respMsgPtr->msgHdr.instanceId = msg->get_instance();
    respMsgPtr->msgHdr.clientId = msg->get_client();
    respMsgPtr->msgHdr.methodId = msg->get_method();
    respMsgPtr->msgHdr.sessionId = msg->get_session();
    respMsgPtr->msgHdr.interfaceVer = msg->get_interface_version();
    respMsgPtr->isErrResp = false;

    // Fill the response type.
    if (msg->get_message_type() == vsomeip::message_type_e::MT_ERROR)
    {
        respMsgPtr->isErrResp = true;
    }

    // Fill the return code.
    respMsgPtr->returnCode = (uint8_t)msg->get_return_code();

    // Fill the payload data.
    respMsgPtr->payloadPtr = NULL;
    size_t len = msg->get_payload()->get_length();
    if (len != 0)
    {
        respMsgPtr->payloadPtr = (Payload_t*)le_mem_ForceAlloc(PayloadPool);
        memset(respMsgPtr->payloadPtr, 0, sizeof(Payload_t));
        respMsgPtr->payloadPtr->size = len;
        memcpy(respMsgPtr->payloadPtr->data, msg->get_payload()->get_data(), len);
    }

    // Create a generic response message object.
    VsClntMsg_t vsMsg;

    vsMsg.type = VS_MSG_RESPONSE;
    vsMsg.msgPtr = (void*)respMsgPtr;

    // Report to the common VSOMEIP msg handler in service layer.
    le_event_Report(VsMsgEvent, &vsMsg, sizeof(VsClntMsg_t));

    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * Generic VSOME/IP event message handler.
 *
 * Each VSOME/IP event is mapped to a legato event report object through le_event_Report().
 */
//--------------------------------------------------------------------------------------------------
void PlatformSomeipClient::VSOMEIPEventHandler
(
    uint8_t routingId,
    const std::shared_ptr<vsomeip::message>& msg
)
{
    // Create a Platform SOME/IP event message to hold the VSOMEIP event message.
    SomeipClnt_EventMsg_t* eventMsgPtr = (SomeipClnt_EventMsg_t*)le_mem_ForceAlloc(EventMsgPool);
    memset(eventMsgPtr, 0, sizeof(SomeipClnt_EventMsg_t));

    // Fill the txnId.
    eventMsgPtr->msgHdr.routingId = routingId;
    eventMsgPtr->msgHdr.serviceId = msg->get_service();
    eventMsgPtr->msgHdr.instanceId = msg->get_instance();
    eventMsgPtr->msgHdr.clientId = msg->get_client();
    eventMsgPtr->msgHdr.methodId = msg->get_method();
    eventMsgPtr->msgHdr.sessionId = msg->get_session();
    eventMsgPtr->msgHdr.interfaceVer = msg->get_interface_version();

    // Fill the payload data.
    eventMsgPtr->payloadPtr = NULL;
    size_t len = msg->get_payload()->get_length();
    if (len != 0)
    {
        eventMsgPtr->payloadPtr = (Payload_t*)le_mem_ForceAlloc(PayloadPool);
        memset(eventMsgPtr->payloadPtr, 0, sizeof(Payload_t));
        eventMsgPtr->payloadPtr->size = len;
        memcpy(eventMsgPtr->payloadPtr->data, msg->get_payload()->get_data(), len);
    }

    // Create a generic event message object.
    VsClntMsg_t vsMsg;

    vsMsg.type = VS_MSG_EVENT;
    vsMsg.msgPtr = (void*)eventMsgPtr;

    // Report to the common VSOMEIP msg handler in service layer.
    le_event_Report(VsMsgEvent, &vsMsg, sizeof(VsClntMsg_t));

    return;
}

//--------------------------------------------------------------------------------------------------
/**
 * Generic VSOME/IP state change message handler.
 *
 * Each VSOME/IP state message is mapped to a legato event report object through le_event_Report().
 */
//--------------------------------------------------------------------------------------------------
void PlatformSomeipClient::VSOMEIPStateHandler
(
    uint8_t routingId,
    uint16_t serviceId,
    uint16_t instanceId,
    bool isAvailable
)
{
    // Create a generic state message object.
    VsClntMsg_t vsMsg;
    vsMsg.type = VS_MSG_STATE;
    vsMsg.state.routingId = routingId;
    vsMsg.state.serviceId = serviceId;
    vsMsg.state.instanceId = instanceId;
    vsMsg.state.isAvailable = isAvailable;
    vsMsg.state.majVer = PLATFORM_SOMEIPDEF_DEFAULT_MAJOR;
    vsMsg.state.minVer = PLATFORM_SOMEIPDEF_DEFAULT_MINOR;

    // Report to the common VSOMEIP msg handler in service layer.
    le_event_Report(VsMsgEvent, &vsMsg, sizeof(VsClntMsg_t));
}

//--------------------------------------------------------------------------------------------------
/**
 * Legato event handler to process event reports which contains VSOMEIP messages.
 */
//--------------------------------------------------------------------------------------------------
void PlatformSomeipClient::PlatformVsMsgHandler
(
    void* reportPtr
)
{
    if (reportPtr != NULL)
    {
        // Get someip client instance.
        PlatformSomeipClient& mySomeipClient = PlatformSomeipClient::GetInstance();

        // Get the message type.
        VsClntMsg_t* vsMsgPtr = (VsClntMsg_t*)reportPtr;
        VsClntMsgType_t msgType = vsMsgPtr->type;

        switch(msgType)
        {
            case VS_MSG_STATE:
                mySomeipClient.ProcessStateChangeMessage(vsMsgPtr->state);
                break;

            case VS_MSG_RESPONSE:
                mySomeipClient.ProcessResponseMessage(vsMsgPtr->msgPtr);
                break;

            case VS_MSG_EVENT:
                mySomeipClient.ProcessEventMessage(vsMsgPtr->msgPtr);
                break;

            default:
                syslog(LOG_CRIT,"Unknown VsMsg(type=%d).", msgType);
                break;
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Legato destructor of service object.
 */
//--------------------------------------------------------------------------------------------------
void PlatformSomeipClient::PlatformServiceDestructor
(
    void* objPtr
)
{
    // Get someip client instance.
    PlatformSomeipClient& mySomeipClient = PlatformSomeipClient::GetInstance();

    // Call the dedicated service destructor.
    mySomeipClient.ServiceDestructor(objPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Legato destructor of group object.
 */
//--------------------------------------------------------------------------------------------------
void PlatformSomeipClient::PlatformGroupDestructor
(
    void* objPtr
)
{
    // Get someip client instance.
    PlatformSomeipClient& mySomeipClient = PlatformSomeipClient::GetInstance();

    // Call the dedicated group destructor.
    mySomeipClient.GroupDestructor(objPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Legato session close handler.
 */
//--------------------------------------------------------------------------------------------------
void PlatformSomeipClient::PlatformClientDisconnection
(
    le_msg_SessionRef_t sessionRef,
    void *contextPtr
)
{
    // Get someip client instance.
    PlatformSomeipClient& mySomeipClient = PlatformSomeipClient::GetInstance();

    // Call the dedicated client session disconnection handler.
    mySomeipClient.ClientDisconnection(sessionRef, contextPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Legato Txn timer expiry handler.
 */
//--------------------------------------------------------------------------------------------------
void PlatformSomeipClient::PlatformTxnTimerExpiryHandler
(
    void* timerRef
)
{
    // Get someip client instance.
    PlatformSomeipClient& mySomeipClient = PlatformSomeipClient::GetInstance();

    // Call the dedicated timer expiry handler.
    mySomeipClient.TxnTimerExpiryHandler((le_timer_Ref_t)timerRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * Compute the hash of the Txn ID.
 */
//--------------------------------------------------------------------------------------------------
size_t PlatformSomeipClient::HashComputeTxnId
(
    const void* keyPtr
)
{
    assert(keyPtr != NULL);

    const CommonMsgHdr_t* msgHdrPtr = (const CommonMsgHdr_t*)keyPtr;

    return (size_t)msgHdrPtr->sessionId;
}

//--------------------------------------------------------------------------------------------------
/**
 * Compare the hash keys of the given two Txn IDs.
 */
//--------------------------------------------------------------------------------------------------
bool PlatformSomeipClient::HashCompareTxnIds
(
    const void* firstKeyPtr,
    const void* secondKeyPtr
)
{
    assert((firstKeyPtr != NULL) && (secondKeyPtr != NULL));

    const CommonMsgHdr_t* msg1HdrPtr = (const CommonMsgHdr_t*)firstKeyPtr;
    const CommonMsgHdr_t* msg2HdrPtr = (const CommonMsgHdr_t*)secondKeyPtr;

    if ((msg1HdrPtr->routingId == msg2HdrPtr->routingId) &&
        (msg1HdrPtr->serviceId == msg2HdrPtr->serviceId) &&
        (msg1HdrPtr->instanceId == msg2HdrPtr->instanceId) &&
        (msg1HdrPtr->methodId == msg2HdrPtr->methodId) &&
        (msg1HdrPtr->sessionId == msg2HdrPtr->sessionId) &&
        (msg1HdrPtr->clientId == msg2HdrPtr->clientId) &&
        (msg1HdrPtr->interfaceVer == msg2HdrPtr->interfaceVer))
    {
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
/**
 * Call all needed handlers for a client once the service state is changed.
 */
//--------------------------------------------------------------------------------------------------
void PlatformSomeipClient::NotifyServiceStateChange
(
    SvcClient_t* svcClientPtr,
    teleplat_someipClnt_State_t state
)
{
    assert(svcClientPtr != NULL);
    assert(svcClientPtr->servicePtr != NULL);

    // Notify the client that the service state is changed.
    le_dls_Link_t* linkPtr = le_dls_Peek(&svcClientPtr->stateHandlerList);
    while (linkPtr != NULL)
    {
        StateHandler_t* handlerPtr = CONTAINER_OF(linkPtr, StateHandler_t, link);

        if ((handlerPtr != NULL) && (handlerPtr->handlefunc != NULL))
        {
            syslog(LOG_DEBUG,"Notify service(%u:0x%x/0x%x) state(%d) for handlerRef(%p) of serviceRef(%p)",
                     svcClientPtr->servicePtr->routingId, svcClientPtr->servicePtr->serviceId,
                     svcClientPtr->servicePtr->instanceId, state,
                     handlerPtr->ref, svcClientPtr->ref);

            handlerPtr->handlefunc(handlerPtr->serviceRef, state, handlerPtr->context);
        }

        linkPtr = le_dls_PeekNext(&svcClientPtr->stateHandlerList, linkPtr);
    }

    // Call corresponding response handlers and free all TxnIds if the service is down.
    if (state == TELEPLAT_SOMEIPCLNT_UNAVAILABLE)
    {
        le_dls_Link_t* linkPtr = le_dls_Pop(&svcClientPtr->txnIdList);
        while (linkPtr != NULL)
        {
            Txn_t* txnIdPtr = CONTAINER_OF(linkPtr, Txn_t, link);
            assert(txnIdPtr != NULL);
            assert(svcClientPtr->ref == txnIdPtr->serviceRef);

            syslog(LOG_DEBUG,"Freed txnIdPtr(%p) for serviceRef(%p).", txnIdPtr, txnIdPtr->serviceRef);

            // Call the response handler to report service is unavailable.
            txnIdPtr->respHandleFunc(LE_UNAVAILABLE, true, PLATFORM_SOMEIPDEF_E_NOT_REACHABLE,
                                     NULL, 0, txnIdPtr->contextPtr);

            // Delete the timer if have.
            if (txnIdPtr->timerRef != NULL)
            {
                le_timer_Delete(txnIdPtr->timerRef);
                txnIdPtr->timerRef = NULL;
            }

            // Remove the txnId from the hash map.
            le_hashmap_Remove(TxnHashMapRef, &txnIdPtr->msgHdr);

            // Delete the txnId object.
            le_mem_Release(txnIdPtr);

            linkPtr = le_dls_Pop(&svcClientPtr->txnIdList);
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Call all event message handlers for a client once an event message is received.
 */
//--------------------------------------------------------------------------------------------------
void PlatformSomeipClient::NotifyEventMessage
(
    SvcClient_t* svcClientPtr,
    uint16_t groupId,
    uint16_t eventId,
    Payload_t* payloadPtr
)
{
    assert(svcClientPtr != NULL);
    assert(svcClientPtr->servicePtr != NULL);

    le_dls_Link_t* linkPtr = le_dls_Peek(&svcClientPtr->eventHandlerList);
    while (linkPtr != NULL)
    {
        EventHandler_t* handlerPtr = CONTAINER_OF(linkPtr, EventHandler_t, link);

        if ((handlerPtr != NULL) && (handlerPtr->handlefunc != NULL) &&
            (handlerPtr->groupId == groupId))
        {
            syslog(LOG_DEBUG,"Notify service(%u:0x%x/0x%x)event(0x%x) for handlerRef(%p) of serviceRef(%p)",
                     svcClientPtr->servicePtr->routingId, svcClientPtr->servicePtr->serviceId,
                     svcClientPtr->servicePtr->instanceId, eventId, handlerPtr->ref,
                     svcClientPtr->ref);

            if (payloadPtr != NULL)
            {
                handlerPtr->handlefunc(handlerPtr->serviceRef, eventId,
                                       payloadPtr->data, payloadPtr->size, handlerPtr->context);
            }
            else
            {
                handlerPtr->handlefunc(handlerPtr->serviceRef, eventId,
                                       NULL, 0, handlerPtr->context);
            }
        }

        linkPtr = le_dls_PeekNext(&svcClientPtr->eventHandlerList, linkPtr);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Find an event group object in a list.
 */
//--------------------------------------------------------------------------------------------------
SomeipClnt_Group_t* PlatformSomeipClient::FindGroup
(
    le_dls_List_t* groupListPtr,
    uint16_t groupId
)
{
    assert(groupListPtr != NULL);

    // Find the group in the group list.
    le_dls_Link_t* linkPtr = le_dls_Peek(groupListPtr);
    while (linkPtr != NULL)
    {
        SomeipClnt_Group_t* groupPtr = CONTAINER_OF(linkPtr, SomeipClnt_Group_t, link);

        if ((groupPtr != NULL) && (groupId == groupPtr->groupId))
        {
            return groupPtr;
        }

        linkPtr = le_dls_PeekNext(groupListPtr, linkPtr);
    }

    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * Find an event object in a service's event list.
 */
//--------------------------------------------------------------------------------------------------
SomeipClnt_Event_t* PlatformSomeipClient::FindEvent
(
    le_dls_List_t* eventListPtr,
    uint16_t eventId
)
{
    assert(eventListPtr != NULL);

    // Find the group in the group list.
    le_dls_Link_t* linkPtr = le_dls_Peek(eventListPtr);
    while (linkPtr != NULL)
    {
        SomeipClnt_Event_t* eventPtr = CONTAINER_OF(linkPtr, SomeipClnt_Event_t, link);

        if ((eventPtr != NULL) && (eventId == eventPtr->eventId))
        {
            return eventPtr;
        }

        linkPtr = le_dls_PeekNext(eventListPtr, linkPtr);
    }

    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * Clear all events in a group.
 */
//--------------------------------------------------------------------------------------------------
void PlatformSomeipClient::ClearEventsInGroup
(
    le_dls_List_t* eventListPtr,
    uint16_t groupId,
    le_msg_SessionRef_t clientSessionRef
)
{
    le_dls_Link_t* linkPtr = le_dls_Peek(eventListPtr);

    while (linkPtr != NULL)
    {
        SomeipClnt_Event_t* eventPtr = CONTAINER_OF(linkPtr, SomeipClnt_Event_t, link);
        linkPtr = le_dls_PeekNext(eventListPtr, linkPtr);

        if ((eventPtr != NULL) && (eventPtr->groupId == groupId) &&
            (eventPtr->clientRef == clientSessionRef))
        {
            syslog(LOG_DEBUG,"Deleted event(eventId=0x%x, groupId=0x%x).", eventPtr->eventId, groupId);
            le_dls_Remove(eventListPtr, &eventPtr->link);
            le_mem_Release(eventPtr);
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Clear all groups created by the given client.
 */
//--------------------------------------------------------------------------------------------------
void PlatformSomeipClient::ClearGroups
(
    le_dls_List_t* groupListPtr,
    le_msg_SessionRef_t clientSessionRef
)
{
    assert(groupListPtr != NULL);

    // Find the group in the group list.
    le_dls_Link_t* linkPtr = le_dls_Peek(groupListPtr);
    while (linkPtr != NULL)
    {
        SomeipClnt_Group_t* groupPtr = CONTAINER_OF(linkPtr, SomeipClnt_Group_t, link);
        linkPtr = le_dls_PeekNext(groupListPtr, linkPtr);

        if ((groupPtr != NULL) && (groupPtr->servicePtr != NULL) &&
            (clientSessionRef == groupPtr->clientRef))
        {
            syslog(LOG_DEBUG,"Deleted group(0x%x) for service(%u:0x%x/0x%x)", groupPtr->groupId,
                     groupPtr->servicePtr->routingId, groupPtr->servicePtr->serviceId,
                     groupPtr->servicePtr->instanceId);
            if (groupPtr->isSubscribed)
            {
                VSOMEIPUnsubscribeEventGroup(groupPtr);
            }

            le_dls_Remove(groupListPtr, &groupPtr->link);
            le_mem_Release(groupPtr);
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Update Service state and notify all clients.
 */
//--------------------------------------------------------------------------------------------------
void PlatformSomeipClient::ProcessStateChangeMessage
(
    VsState_t serviceState
)
{
    // Get the service state.
    uint8_t routingId = serviceState.routingId;
    uint16_t serviceId = serviceState.serviceId;
    uint16_t instanceId = serviceState.instanceId;
    uint8_t majVer = serviceState.majVer;
    uint32_t minVer = serviceState.minVer;
    bool isAvailable = serviceState.isAvailable;

    // Get service version from VSOMEIP stack if the service is available.
    if (isAvailable)
    {
        if (VSOMEIPGetServiceInfo(routingId, serviceId, instanceId, &majVer, &minVer) == false)
        {
            syslog(LOG_ERR,"Failed to get the VERSION of service(%u:0x%x/0x%x).",
                     routingId, serviceId, instanceId);
            return;
        }

        syslog(LOG_INFO,"Service(%u:0x%x/0x%x) is available with Version(%u.%u).",
                routingId, serviceId, instanceId, majVer, minVer);
    }

    // Search the service.
    SomeipClnt_Service_t* servicePtr = FindService(routingId, serviceId, instanceId);

    if (servicePtr != NULL)
    {
        uint8_t reqMajVer = servicePtr->reqMajVer;
        uint32_t reqMinVer = servicePtr->reqMinVer;

        // Check if the version is we requested.
        if (isAvailable &&
            (((reqMajVer != PLATFORM_SOMEIPDEF_ANY_MAJOR) && (reqMajVer != majVer)) ||
            ((reqMinVer != PLATFORM_SOMEIPDEF_ANY_MINOR) && (reqMinVer != minVer))))
        {
            syslog(LOG_ERR,"Service(%u:0x%x/0x%x) versions don't match. reqVersion=(%u.%u)," \
                     "curVersion=(%u.%u).", routingId, serviceId, instanceId,
                     reqMajVer, reqMinVer, majVer, minVer);
            return;
        }

        // Update the service state and version.
        teleplat_someipClnt_State_t state =
            isAvailable ? TELEPLAT_SOMEIPCLNT_AVAILABLE : TELEPLAT_SOMEIPCLNT_UNAVAILABLE;
        servicePtr->state = isAvailable ? SERVICE_STATE_AVAILABLE : SERVICE_STATE_UNAVAILABLE;
        servicePtr->majorVersion = majVer;
        servicePtr->minorVersion = minVer;

        // Update/Notify all clients associated with this service.
        le_ref_IterRef_t iterRef = le_ref_GetIterator(SvcClientRefMap);
        while (le_ref_NextNode(iterRef) == LE_OK)
        {
            SvcClient_t* svcClientPtr = (SvcClient_t*)le_ref_GetValue(iterRef);
            assert(svcClientPtr != NULL);

            if ((svcClientPtr->servicePtr == servicePtr) &&
                (svcClientPtr->state != servicePtr->state))
            {
                svcClientPtr->state = servicePtr->state;
                NotifyServiceStateChange(svcClientPtr, state);
            }
        }

        // Subscribe/unsubscribe event groups once service state gets changed.
        le_dls_Link_t* linkPtr = le_dls_Peek(&servicePtr->groupList);
        while (linkPtr != NULL)
        {
            SomeipClnt_Group_t* groupPtr = CONTAINER_OF(linkPtr, SomeipClnt_Group_t, link);
            linkPtr = le_dls_PeekNext(&servicePtr->groupList, linkPtr);

            if ((groupPtr != NULL) && (groupPtr->setForSubscription))
            {
                if ((servicePtr->state == SERVICE_STATE_AVAILABLE) && (!groupPtr->isSubscribed))
                {
                    VSOMEIPSubscribeEventGroup(groupPtr);
                }
                else if((servicePtr->state == SERVICE_STATE_UNAVAILABLE) &&(groupPtr->isSubscribed))
                {
                    VSOMEIPUnsubscribeEventGroup(groupPtr);
                }
            }
        }
    }
    else
    {
        syslog(LOG_WARNING,"Can not find service(%u:0x%x/0x%x), dropped the state message.",
                routingId, serviceId, instanceId);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Process the repsonse message.
 */
//--------------------------------------------------------------------------------------------------
void PlatformSomeipClient::ProcessResponseMessage
(
    void* msgPtr
)
{
    SomeipClnt_RespMsg_t* respPtr = (SomeipClnt_RespMsg_t*)msgPtr;
    assert(respPtr != NULL);

    // Get the message header of the response.
    CommonMsgHdr_t* msgHdrPtr = &respPtr->msgHdr;

    // Get the TxnId by msgHdr of the response from the hash map.
    Txn_t* txnIdPtr = (Txn_t*)le_hashmap_Get(TxnHashMapRef, msgHdrPtr);

    if (txnIdPtr == NULL)
    {
        syslog(LOG_WARNING,"Unknown response(msgHdr=%u:0x%x/0x%x/0x%x/0x%x/0x%x/0x%x), dropped it.",
                msgHdrPtr->routingId, msgHdrPtr->serviceId, msgHdrPtr->instanceId,
                msgHdrPtr->methodId, msgHdrPtr->sessionId, msgHdrPtr->clientId,
                (uint16_t)msgHdrPtr->interfaceVer);

        // Free the response object.
        if (respPtr->payloadPtr != NULL)
        {
            le_mem_Release(respPtr->payloadPtr);
            respPtr->payloadPtr = NULL;
        }
        le_mem_Release(respPtr);
        return;
    }

    // Get the payload part of the response.
    Payload_t* payloadPtr = respPtr->payloadPtr;

    // Get the service reference and client-service-session object of the TxnId.
    teleplat_someipClnt_ServiceRef_t serviceRef = txnIdPtr->serviceRef;
    SvcClient_t* svcClientPtr = (SvcClient_t*)le_ref_Lookup(SvcClientRefMap, serviceRef);

    // Sanity check.
    assert(svcClientPtr != NULL);
    assert(svcClientPtr->ref == serviceRef);
    assert(txnIdPtr->respHandleFunc != NULL);

    // Call the response handler.
    if (payloadPtr != NULL)
    {
        txnIdPtr->respHandleFunc(LE_OK, respPtr->isErrResp, respPtr->returnCode,
                                 payloadPtr->data, payloadPtr->size,
                                 txnIdPtr->contextPtr);
    }
    else
    {
        txnIdPtr->respHandleFunc(LE_OK, respPtr->isErrResp, respPtr->returnCode,
                                 NULL, 0, txnIdPtr->contextPtr);
    }

    syslog(LOG_DEBUG,"Freed txnIdPtr(%p) for serviceRef(%p).", txnIdPtr, txnIdPtr->serviceRef);

    // Remove the txnId from the txn list.
    le_dls_Remove(&svcClientPtr->txnIdList, &txnIdPtr->link);

    // Delete the response expiry timer if have.
    if (txnIdPtr->timerRef != NULL)
    {
        le_timer_Delete(txnIdPtr->timerRef);
        txnIdPtr->timerRef = NULL;
    }

    // Remove the txnId from the hash map.
    le_hashmap_Remove(TxnHashMapRef, &txnIdPtr->msgHdr);

    // Free the txnId object.
    le_mem_Release(txnIdPtr);

    // Free the response object.
    if (respPtr->payloadPtr != NULL)
    {
        le_mem_Release(respPtr->payloadPtr);
        respPtr->payloadPtr = NULL;
    }
    le_mem_Release(respPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Process the event message.
 */
//--------------------------------------------------------------------------------------------------
void PlatformSomeipClient::ProcessEventMessage
(
    void* msgPtr
)
{
    SomeipClnt_EventMsg_t* eventMsgPtr = (SomeipClnt_EventMsg_t*)msgPtr;
    assert(eventMsgPtr != NULL);

    // Get the message header of the event.
    CommonMsgHdr_t* msgHdrPtr = &eventMsgPtr->msgHdr;

    // Get the payload of the event.
    Payload_t* payloadPtr = eventMsgPtr->payloadPtr;

    // Search the service.
    SomeipClnt_Service_t* servicePtr = FindService(msgHdrPtr->routingId, msgHdrPtr->serviceId,
                                                   msgHdrPtr->instanceId);
    if (servicePtr != NULL)
    {
        // Search the event in service's event list.
        SomeipClnt_Event_t* eventPtr = FindEvent(&servicePtr->eventList, msgHdrPtr->methodId);
        if (eventPtr != NULL)
        {
            uint16_t groupId = eventPtr->groupId;
            uint16_t eventId = eventPtr->eventId;

            // Search the group in service's group list.
            SomeipClnt_Group_t* groupPtr = FindGroup(&servicePtr->groupList, groupId);
            assert(groupPtr != NULL);

            if (!groupPtr->isSubscribed)
            {
                syslog(LOG_WARNING,"The group(0x%x) of service(%u:0x%x/0x%x) isn't subscribed, dropped event.",
                groupId, msgHdrPtr->routingId, msgHdrPtr->serviceId, msgHdrPtr->instanceId);
                goto RELEASE_MSG;
            }

            // Call all event message handlers based on the groupId.
            le_ref_IterRef_t iterRef = le_ref_GetIterator(SvcClientRefMap);
            while (le_ref_NextNode(iterRef) == LE_OK)
            {
                SvcClient_t* svcClientPtr = (SvcClient_t*)le_ref_GetValue(iterRef);
                assert(svcClientPtr != NULL);

                if (svcClientPtr->servicePtr == servicePtr)
                {
                    NotifyEventMessage(svcClientPtr, groupId, eventId, payloadPtr);
                }
            }

            goto RELEASE_MSG;
        }

        syslog(LOG_WARNING,"Can not find event(0x%x) of service(%u:0x%x/0x%x), dropped the event.",
                msgHdrPtr->methodId, msgHdrPtr->routingId, msgHdrPtr->serviceId,
                msgHdrPtr->instanceId);
        goto RELEASE_MSG;
    }

    syslog(LOG_WARNING,"Can not find service(%u:0x%x/0x%x) for event(0x%x), dropped the event.",
            msgHdrPtr->routingId, msgHdrPtr->serviceId, msgHdrPtr->instanceId, msgHdrPtr->methodId);

RELEASE_MSG:
    if (payloadPtr != NULL)
    {
        le_mem_Release(payloadPtr);
    }
    le_mem_Release(eventMsgPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Txn timer expiry handler in service layer.
 */
//--------------------------------------------------------------------------------------------------
void PlatformSomeipClient::TxnTimerExpiryHandler
(
    le_timer_Ref_t timerRef
)
{
    // Get the txnIdPtr from the context of the timer.
    Txn_t* txnIdPtr = (Txn_t*)le_timer_GetContextPtr(timerRef);
    assert(txnIdPtr != NULL);

    CommonMsgHdr_t* msgHdrPtr = &txnIdPtr->msgHdr;

    syslog(LOG_WARNING,"TxnId timeout(%d ms) (msgHdr=%u:0x%x/0x%x/0x%x/0x%x/0x%x/0x%x).",
            le_timer_GetMsInterval(timerRef), msgHdrPtr->routingId, msgHdrPtr->serviceId,
            msgHdrPtr->instanceId, msgHdrPtr->methodId, msgHdrPtr->sessionId, msgHdrPtr->clientId,
            (uint16_t)msgHdrPtr->interfaceVer);

    teleplat_someipClnt_ServiceRef_t serviceRef = txnIdPtr->serviceRef;
    SvcClient_t* svcClientPtr = (SvcClient_t*)le_ref_Lookup(SvcClientRefMap, serviceRef);

    // Sanity check.
    assert(txnIdPtr->respHandleFunc != NULL);
    assert(txnIdPtr->timerRef == timerRef);
    assert(svcClientPtr != NULL);
    assert(svcClientPtr->ref == serviceRef);

    // Call response handler of this txnId.
    txnIdPtr->respHandleFunc(LE_TIMEOUT, true, PLATFORM_SOMEIPDEF_E_TIMEOUT,
                             NULL, 0, txnIdPtr->contextPtr);

    syslog(LOG_DEBUG,"Freed txnId(%p) for serviceRef(%p).", txnIdPtr, txnIdPtr->serviceRef);

    // Remove the txnId from the txn list.
    le_dls_Remove(&svcClientPtr->txnIdList, &txnIdPtr->link);

    // Remove the txnId from the hash map.
    le_hashmap_Remove(TxnHashMapRef, &txnIdPtr->msgHdr);

    // Delete this timer.
    le_timer_Delete(txnIdPtr->timerRef);
    txnIdPtr->timerRef = NULL;

    // Delete the txnId object.
    le_mem_Release(txnIdPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Wrapper interface to VSOMEIP request service.
 */
//--------------------------------------------------------------------------------------------------
void PlatformSomeipClient::VSOMEIPRequestService
(
    SomeipClnt_Service_t* servicePtr
)
{
    // Sanity check for the service.
    assert(servicePtr != NULL);

    // Get corresponding vsomeip routing manager instance.
    std::shared_ptr<vsomeip::application> routingApp =
        someip_GetRoutingManager(servicePtr->routingId);

    // Register availability handler for this service instance.
    routingApp->register_availability_handler(servicePtr->serviceId,
                                              servicePtr->instanceId,
                                              std::bind(&PlatformSomeipClient::VSOMEIPStateHandler,
                                              this, servicePtr->routingId, std::placeholders::_1,
                                              std::placeholders::_2,
                                              std::placeholders::_3),
                                              servicePtr->reqMajVer, servicePtr->reqMinVer);

    // Request the service.
    routingApp->request_service(servicePtr->serviceId, servicePtr->instanceId,
                                servicePtr->reqMajVer, servicePtr->reqMinVer);

    syslog(LOG_INFO,"VSOMEIP Requested Service(%u:0x%x/0x%x) with Ver(%u.%u).", servicePtr->routingId,
            servicePtr->serviceId, servicePtr->instanceId,
            servicePtr->reqMajVer, servicePtr->reqMinVer);
}

//--------------------------------------------------------------------------------------------------
/**
 * Wrapper interface to VSOME/IP release service.
 */
//--------------------------------------------------------------------------------------------------
void PlatformSomeipClient::VSOMEIPReleaseService
(
    SomeipClnt_Service_t* servicePtr
)
{
    // Sanity check for the service.
    assert(servicePtr != NULL);

    // Get corresponding vsomeip routing manager instance.
    std::shared_ptr<vsomeip::application> routingApp =
        someip_GetRoutingManager(servicePtr->routingId);

    // Unregister availability handler for this service instance.
    routingApp->unregister_availability_handler(servicePtr->serviceId, servicePtr->instanceId,
                                                servicePtr->reqMajVer, servicePtr->reqMinVer);

    // Release the service.
    routingApp->release_service(servicePtr->serviceId, servicePtr->instanceId);

    syslog(LOG_INFO,"VSOMEIP Released Service(%u:0x%x/0x%x) with Ver(%u.%u).", servicePtr->routingId,
            servicePtr->serviceId, servicePtr->instanceId,
            servicePtr->reqMajVer, servicePtr->reqMinVer);
}

//--------------------------------------------------------------------------------------------------
/**
 * Wrapper interface to VSOME/IP send request message.
 */
//--------------------------------------------------------------------------------------------------
void PlatformSomeipClient::VSOMEIPSendRequest
(
    CommonMsgHdr_t* msgHdrPtr,
    bool isNonRetMsg,
    bool isReliable,
    Payload_t* payloadPtr
)
{
    // Sanity check.
    assert(msgHdrPtr != NULL);

    // Set the request header.
    std::shared_ptr<vsomeip::message> request = vsomeip::runtime::get()->create_request();
    request->set_service(msgHdrPtr->serviceId);
    request->set_instance(msgHdrPtr->instanceId);
    request->set_method(msgHdrPtr->methodId);
    request->set_client(msgHdrPtr->clientId);
    request->set_interface_version(msgHdrPtr->interfaceVer);
    request->set_reliable(isReliable);

    if (isNonRetMsg)
    {
        request->set_message_type(vsomeip::message_type_e::MT_REQUEST_NO_RETURN);
    }

    // Set the request payload.
    if ((payloadPtr != NULL) && (payloadPtr->size != 0))
    {
        std::shared_ptr<vsomeip::payload> vsPayload = vsomeip::runtime::get()->create_payload();
        vsPayload->set_data(payloadPtr->data, payloadPtr->size);
        request->set_payload(vsPayload);
    }

    // Get corresponding vsomeip routing manager instance.
    std::shared_ptr<vsomeip::application> routingApp =
        someip_GetRoutingManager(msgHdrPtr->routingId);

    // Send the request.
    routingApp->send(request);

    // The sessionId is automatically filled by VSOMEIP stack after sending.
    // So we save the sessionId here, which can be used to track the response.
    msgHdrPtr->sessionId = request->get_session();
}

//--------------------------------------------------------------------------------------------------
/**
 * Wrapper interface to VSOME/IP get service state. Return true if the service is avaiable,
 * otherwise return false.
 */
//--------------------------------------------------------------------------------------------------
bool PlatformSomeipClient::VSOMEIPGetServiceInfo
(
    uint8_t routingId,
    uint16_t serviceId,
    uint16_t instanceId,
    uint8_t* majVerPtr,
    uint32_t* minVerPtr
)
{
    bool isAvailable = false;
    uint8_t majVer = 0;
    uint32_t minVer = 0;

    // Check input parameters.
    if (((serviceId == 0xFFFF) || (serviceId == 0x0000)) ||
        ((instanceId == 0xFFFF) || (instanceId == 0x0000)))
    {
        syslog(LOG_ERR,"Invalid serviceId or instanceId.");
        return false;
    }

    // Get corresponding vsomeip routing manager instance.
    std::shared_ptr<vsomeip::application> routingApp = someip_GetRoutingManager(routingId);

    // Check if the service is available and get the service version.
    vsomeip::application::available_t are_available;
    if (routingApp->are_available(are_available, serviceId, instanceId,
                                  vsomeip::ANY_MAJOR, vsomeip::ANY_MINOR))
    {
        auto foundService = are_available.find(serviceId);
        if(foundService != are_available.end())
        {
            auto foundInstance = foundService->second.find(instanceId);
            if(foundInstance != foundService->second.end())
            {
                auto foundVersion = foundInstance->second.begin();
                if (foundVersion != foundInstance->second.end())
                {
                    // Set the service version.
                    majVer = foundVersion->first;
                    minVer = foundVersion->second;
                    isAvailable = true;
                    syslog(LOG_DEBUG,"VSOMEIP Service(%u:0x%x/0x%x): Version=(%u.%u)",
                             routingId, serviceId, instanceId, majVer, minVer);
                }
            }
        }
    }

    if (isAvailable)
    {
        if (majVerPtr != NULL)
        {
            *majVerPtr = majVer;
        }

        if (minVerPtr != NULL)
        {
            *minVerPtr = minVer;
        }
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
/**
 * Wrapper interface to VSOME/IP subscribe event group.
 */
//--------------------------------------------------------------------------------------------------
void PlatformSomeipClient::VSOMEIPSubscribeEventGroup
(
    SomeipClnt_Group_t* groupPtr
)
{
    assert(groupPtr != NULL);
    assert(!groupPtr->isSubscribed);

    SomeipClnt_Service_t* servicePtr = groupPtr->servicePtr;
    assert(servicePtr != NULL);

    // Get corresponding vsomeip routing manager instance.
    std::shared_ptr<vsomeip::application> routingApp =
        someip_GetRoutingManager(servicePtr->routingId);

    uint16_t groupId = groupPtr->groupId;

    std::set<vsomeip::eventgroup_t> its_groups;
    its_groups.insert(groupId);

    // Request each event in the group.
    le_dls_Link_t* linkPtr = le_dls_Peek(&servicePtr->eventList);
    while (linkPtr != NULL)
    {
        SomeipClnt_Event_t* eventPtr = CONTAINER_OF(linkPtr, SomeipClnt_Event_t, link);

        if ((eventPtr != NULL) && (groupId == eventPtr->groupId))
        {
            if (eventPtr->eventType == PLATFORM_SOMEIPDEF_ET_FIELD)
            {
                routingApp->request_event(servicePtr->serviceId, servicePtr->instanceId,
                                          eventPtr->eventId, its_groups,
                                          vsomeip::event_type_e::ET_FIELD);
            }
            else
            {
                routingApp->request_event(servicePtr->serviceId, servicePtr->instanceId,
                                          eventPtr->eventId, its_groups,
                                          vsomeip::event_type_e::ET_EVENT);
            }
            syslog(LOG_INFO,"VSOMEIP Request event(0x%x) of group(0x%x) for service(%u:0x%x/0x%x).",
                     eventPtr->eventId, groupId, servicePtr->routingId, servicePtr->serviceId,
                     servicePtr->instanceId);
        }

        linkPtr = le_dls_PeekNext(&servicePtr->eventList, linkPtr);
    }

    // Subscribe the group.
    routingApp->subscribe(servicePtr->serviceId, servicePtr->instanceId,
                          groupId, servicePtr->majorVersion);
    syslog(LOG_INFO,"VSOMEIP Subscribe group(0x%x) for service(%u:0x%x/0x%x) of interfaceVer(%u).",
             groupId, servicePtr->routingId, servicePtr->serviceId, servicePtr->instanceId,
             servicePtr->majorVersion);

    // Mark the group of the service is subscribed.
    groupPtr->isSubscribed = true;
}

//--------------------------------------------------------------------------------------------------
/**
 * Wrapper interface to VSOME/IP unsubscribe event group.
 */
//--------------------------------------------------------------------------------------------------
void PlatformSomeipClient::VSOMEIPUnsubscribeEventGroup
(
    SomeipClnt_Group_t* groupPtr
)
{
    assert(groupPtr != NULL);
    assert(groupPtr->isSubscribed);

    SomeipClnt_Service_t* servicePtr = groupPtr->servicePtr;
    assert(servicePtr != NULL);

    // Get corresponding vsomeip routing manager instance.
    std::shared_ptr<vsomeip::application> routingApp =
        someip_GetRoutingManager(servicePtr->routingId);

    uint16_t groupId = groupPtr->groupId;

    // Unsubscribe the group.
    routingApp->unsubscribe(servicePtr->serviceId, servicePtr->instanceId, groupId);
    syslog(LOG_INFO,"VSOMEIP Unsubscribe group(0x%x) for service(%u:0x%x/0x%x).",
            groupId, servicePtr->routingId, servicePtr->serviceId, servicePtr->instanceId);

    // Release each event in the group.
    le_dls_Link_t* linkPtr = le_dls_Peek(&servicePtr->eventList);
    while (linkPtr != NULL)
    {
        SomeipClnt_Event_t* eventPtr = CONTAINER_OF(linkPtr, SomeipClnt_Event_t, link);

        if ((eventPtr != NULL) && (groupId == eventPtr->groupId))
        {
            routingApp->release_event(servicePtr->serviceId, servicePtr->instanceId,
                                      eventPtr->eventId);
            syslog(LOG_INFO,"VSOMEIP Release event(0x%x) of group(0x%x) for service(%u:0x%x/0x%x).",
                     eventPtr->eventId, groupId, servicePtr->routingId, servicePtr->serviceId,
                     servicePtr->instanceId);
        }

        linkPtr = le_dls_PeekNext(&servicePtr->eventList, linkPtr);
    }

    // Mark the group of the service is unsubscribed.
    groupPtr->isSubscribed = false;
}

//--------------------------------------------------------------------------------------------------
/**
 * Search a SOME/IP service object in service list.
 */
//--------------------------------------------------------------------------------------------------
SomeipClnt_Service_t* PlatformSomeipClient::FindService
(
    uint8_t routingId,
    uint16_t serviceId,
    uint16_t instanceId
)
{
    le_dls_Link_t* linkPtr = le_dls_Peek(&ServiceList);

    while (linkPtr != NULL)
    {
        SomeipClnt_Service_t* servicePtr = CONTAINER_OF(linkPtr, SomeipClnt_Service_t, link);
        assert(servicePtr != NULL);

        if ((servicePtr->serviceId == serviceId) && (servicePtr->instanceId == instanceId) &&
            (servicePtr->routingId == routingId))
        {
            return servicePtr;
        }

        linkPtr = le_dls_PeekNext(&ServiceList, linkPtr);
    }

    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * Search a client-service-session object in client-service-session list.
 */
//--------------------------------------------------------------------------------------------------
SvcClient_t* PlatformSomeipClient::FindSvcClient
(
    SomeipClnt_Service_t* servicePtr,
    le_msg_SessionRef_t clientSessionRef
)
{
    assert(servicePtr != NULL);

    le_ref_IterRef_t iterRef = le_ref_GetIterator(SvcClientRefMap);
    while (le_ref_NextNode(iterRef) == LE_OK)
    {
        SvcClient_t* svcClientPtr = (SvcClient_t*)le_ref_GetValue(iterRef);
        assert((svcClientPtr != NULL) && (svcClientPtr->servicePtr != NULL));

        if ((svcClientPtr->servicePtr == servicePtr) &&
            (svcClientPtr->clientRef == clientSessionRef))
        {
            return svcClientPtr;
        }
    }

    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * Check if a client-service-session can be released safely.
 */
//--------------------------------------------------------------------------------------------------
bool PlatformSomeipClient::IsSvcClientReleasable
(
    SvcClient_t* svcClientPtr
)
{
    assert(svcClientPtr != NULL);

    // Check no active handlers.
    // Check no Tx messages.
    // Check no Txn IDs.
    if(le_dls_IsEmpty(&svcClientPtr->stateHandlerList) &&
       le_dls_IsEmpty(&svcClientPtr->eventHandlerList) &&
       le_dls_IsEmpty(&svcClientPtr->txMsgList) &&
       le_dls_IsEmpty(&svcClientPtr->txnIdList))
    {
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
/**
 * Clean up a client-service-session associated resources(handlerList, txMsgList and TxnIdList).
 */
//--------------------------------------------------------------------------------------------------
void PlatformSomeipClient::CleanupClientResources
(
    SvcClient_t* svcClientPtr
)
{
    assert(svcClientPtr != NULL);
    assert(svcClientPtr->servicePtr != NULL);

    syslog(LOG_DEBUG,"Cleanup serviceRef(%p).", svcClientPtr->ref);

    // Empty the event handler list of this client-service-session.
    le_dls_Link_t* linkPtr = le_dls_Pop(&svcClientPtr->eventHandlerList);
    while (linkPtr != NULL)
    {
        EventHandler_t* eventHandlerPtr = CONTAINER_OF(linkPtr, EventHandler_t, link);
        assert(eventHandlerPtr != NULL);
        assert(svcClientPtr->ref == eventHandlerPtr->serviceRef);

        syslog(LOG_DEBUG,"Freed eventHandlerRef(%p) for groupId(0x%x) of serviceRef(%p)",
                eventHandlerPtr->ref, eventHandlerPtr->groupId, eventHandlerPtr->serviceRef);

        le_ref_DeleteRef(EventHandlerRefMap, eventHandlerPtr->ref);
        le_mem_Release(eventHandlerPtr);

        linkPtr = le_dls_Pop(&svcClientPtr->eventHandlerList);
    }

    // Empty the state handler list of this client-service-session.
    linkPtr = le_dls_Pop(&svcClientPtr->stateHandlerList);
    while (linkPtr != NULL)
    {
        StateHandler_t* stateHandlerPtr = CONTAINER_OF(linkPtr, StateHandler_t, link);
        assert(stateHandlerPtr != NULL);
        assert(svcClientPtr->ref == stateHandlerPtr->serviceRef);

        syslog(LOG_DEBUG,"Freed stateHandlerRef(%p) for serviceRef(%p)",
                stateHandlerPtr->ref, stateHandlerPtr->serviceRef);

        le_ref_DeleteRef(StateHandlerRefMap, stateHandlerPtr->ref);
        le_mem_Release(stateHandlerPtr);

        linkPtr = le_dls_Pop(&svcClientPtr->stateHandlerList);
    }

    // Empty the txMsg list of this client.
    linkPtr = le_dls_Pop(&svcClientPtr->txMsgList);
    while (linkPtr != NULL)
    {
        SomeipClnt_TxMsg_t* txMsgPtr = CONTAINER_OF(linkPtr, SomeipClnt_TxMsg_t, link);
        assert(txMsgPtr != NULL);
        assert(svcClientPtr->ref == txMsgPtr->serviceRef);

        if (txMsgPtr->payloadPtr != NULL)
        {
            le_mem_Release(txMsgPtr->payloadPtr);
            txMsgPtr->payloadPtr = NULL;
            syslog(LOG_DEBUG,"Freed payload data of msgRef(%p).", txMsgPtr->ref);
        }
        syslog(LOG_DEBUG,"Freed msgRef(%p) for serviceRef(%p)", txMsgPtr->ref, txMsgPtr->serviceRef);

        le_ref_DeleteRef(TxMsgRefMap, txMsgPtr->ref);
        le_mem_Release(txMsgPtr);

        linkPtr = le_dls_Pop(&svcClientPtr->txMsgList);
    }

    // Empty the TxnId list of this client.
    linkPtr = le_dls_Pop(&svcClientPtr->txnIdList);
    while (linkPtr != NULL)
    {
        Txn_t* txnIdPtr = CONTAINER_OF(linkPtr, Txn_t, link);
        assert(txnIdPtr != NULL);
        assert(txnIdPtr->respHandleFunc != NULL);
        assert(svcClientPtr->ref == txnIdPtr->serviceRef);

        syslog(LOG_DEBUG,"Freed txnIdPtr(%p) for serviceRef(%p).", txnIdPtr, txnIdPtr->serviceRef);

        // Still need call the handler to free the resources allocated in IPC server stub code.
        txnIdPtr->respHandleFunc(LE_COMM_ERROR, false, 0, NULL, 0, txnIdPtr->contextPtr);

        // Delete the timer if have.
        if (txnIdPtr->timerRef != NULL)
        {
            le_timer_Delete(txnIdPtr->timerRef);
            txnIdPtr->timerRef = NULL;
        }

        // Remove the txnId from the hash map.
        le_hashmap_Remove(TxnHashMapRef, &txnIdPtr->msgHdr);

        // Delete the txnId object.
        le_mem_Release(txnIdPtr);

        linkPtr = le_dls_Pop(&svcClientPtr->txnIdList);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Dedicated client diconnection handler.
 */
//--------------------------------------------------------------------------------------------------
void PlatformSomeipClient::ClientDisconnection
(
    le_msg_SessionRef_t sessionRef,
    void *contextPtr
)
{
    LE_UNUSED(contextPtr);

    // Unsubscribe and remove all groups and events that created by this client.
    le_dls_Link_t* linkPtr = le_dls_Peek(&ServiceList);
    while (linkPtr != NULL)
    {
        SomeipClnt_Service_t* servicePtr = CONTAINER_OF(linkPtr, SomeipClnt_Service_t, link);
        linkPtr = le_dls_PeekNext(&ServiceList, linkPtr);

        if (servicePtr != NULL)
        {
            ClearGroups(&servicePtr->groupList, sessionRef);
        }
    }

    // Go through the client-service-session list and cleanup all
    // resources(handlers/messages) that created by this client.
    le_ref_IterRef_t iterRef = le_ref_GetIterator(SvcClientRefMap);
    while (le_ref_NextNode(iterRef) == LE_OK)
    {
        SvcClient_t* svcClientPtr = (SvcClient_t*)le_ref_GetValue(iterRef);
        assert((svcClientPtr != NULL) && (svcClientPtr->servicePtr != NULL));
        assert(svcClientPtr->ref == le_ref_GetSafeRef(iterRef));

        if (svcClientPtr->clientRef == sessionRef)
        {
            CleanupClientResources(svcClientPtr);
            assert(IsSvcClientReleasable(svcClientPtr));

            le_mem_Release(svcClientPtr->servicePtr);

            le_ref_DeleteRef(SvcClientRefMap, svcClientPtr->ref);
            le_mem_Release(svcClientPtr);
        }
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Dedicated service destructor.
 */
//--------------------------------------------------------------------------------------------------
void PlatformSomeipClient::ServiceDestructor
(
    void* objPtr
)
{
    if (objPtr != NULL)
    {
        SomeipClnt_Service_t* servicePtr = (SomeipClnt_Service_t*)objPtr;

        // Sanity check.
        assert(le_dls_IsEmpty(&servicePtr->groupList) && le_dls_IsEmpty(&servicePtr->eventList));

        // Remove the service from the service list.
        le_dls_Remove(&ServiceList, &servicePtr->link);

        // Release the service in VSOMEIP layer.
        VSOMEIPReleaseService(servicePtr);
        syslog(LOG_DEBUG,"Freed SOME/IP servicePtr(%p) for Service(%u:0x%x/0x%x).",
                servicePtr, servicePtr->routingId, servicePtr->serviceId, servicePtr->instanceId);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Dedicated group destructor.
 */
//--------------------------------------------------------------------------------------------------
void PlatformSomeipClient::GroupDestructor
(
    void* objPtr
)
{
    if (objPtr != NULL)
    {
        // Remove the group from the service list.
        SomeipClnt_Group_t* groupPtr = (SomeipClnt_Group_t*)objPtr;
        SomeipClnt_Service_t* servicePtr = groupPtr->servicePtr;
        assert(servicePtr != NULL);

        ClearEventsInGroup(&servicePtr->eventList, groupPtr->groupId, groupPtr->clientRef);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Request a service and return the client-service-session reference.
 */
//--------------------------------------------------------------------------------------------------
teleplat_someipClnt_ServiceRef_t PlatformSomeipClient::RequestService
(
    uint8_t routingId,
    uint16_t serviceId,
    uint16_t instanceId,
    uint8_t majVer,
    uint32_t minVer
)
{
    // Check the valid serviceId and instanceId according to [PRS_SOMEIPSD_00515] and
    // [PRS_SOMEIPSD_00516] of <SOME/IP Service Discovery Protocol Specification AUTOSAR FO R21-11>
    if (((serviceId == 0xFFFF) || (serviceId == 0xFFFE) || (serviceId == 0x0000)) ||
        ((instanceId == 0xFFFF) || (instanceId == 0x0000)))
    {
        syslog(LOG_ERR,"Invalid serviceId or instarnceId.");
        return NULL;
    }

    // Get and save the client session ref.
    le_msg_SessionRef_t clientSessionRef = teleplat_someipClnt_GetClientSessionRef();
    bool isServiceCreated = false;

    // Search the SOME/IP client-service-instance.
    SomeipClnt_Service_t* servicePtr = FindService(routingId, serviceId, instanceId);

    // Create a client-service-instance object if it's not in the service list.
    if (servicePtr == NULL)
    {
        uint8_t curMajVer;
        uint32_t curMinVer;

        isServiceCreated = true;

        servicePtr = (SomeipClnt_Service_t*)le_mem_ForceAlloc(ServicePool);
        memset(servicePtr, 0, sizeof(SomeipClnt_Service_t));

        // Init the routingId, serviceId and instanceId.
        servicePtr->routingId = routingId;
        servicePtr->serviceId = serviceId;
        servicePtr->instanceId = instanceId;

        // Init requested version.
        servicePtr->reqMajVer = majVer;
        servicePtr->reqMinVer = minVer;

        // Init the service version.
        servicePtr->majorVersion = PLATFORM_SOMEIPDEF_DEFAULT_MAJOR;
        servicePtr->minorVersion = PLATFORM_SOMEIPDEF_DEFAULT_MINOR;

        // Init the lists.
        servicePtr->groupList = LE_DLS_LIST_INIT;
        servicePtr->eventList = LE_DLS_LIST_INIT;

        // Init the state.
        servicePtr->state = SERVICE_STATE_UNKNOWN;

        // Update the service state if it's available.
        if (VSOMEIPGetServiceInfo(routingId, serviceId, instanceId, &curMajVer, &curMinVer) == true)
        {
            // Check if the version is we requested.
            if (((majVer != PLATFORM_SOMEIPDEF_ANY_MAJOR) && (majVer != curMajVer)) ||
                ((minVer != PLATFORM_SOMEIPDEF_ANY_MINOR) && (minVer != curMinVer)))
            {
                syslog(LOG_ERR,"Service(%u:0x%x/0x%x) versions don't match. reqVersion=(%u.%u)," \
                         "curVersion=(%u.%u).", routingId, serviceId, instanceId,
                         majVer, minVer, curMajVer, curMinVer);

                le_mem_Release(servicePtr);
                return NULL;
            }

            servicePtr->state = SERVICE_STATE_AVAILABLE;
            servicePtr->majorVersion = curMajVer;
            servicePtr->minorVersion = curMinVer;

            syslog(LOG_INFO,"Service(%u:0x%x/0x%x) is available with Version(%u.%u).",
                    routingId, serviceId, instanceId, curMajVer, curMinVer);
        }

        // Add to the service list.
        servicePtr->link = LE_DLS_LINK_INIT;
        le_dls_Queue(&ServiceList, &servicePtr->link);

        syslog(LOG_DEBUG,"Created SOME/IP servicePtr(%p) for Service(%u:0x%x/0x%x).",
                servicePtr, routingId, serviceId, instanceId);
    }

    // Check if requested versions match.
    if ((servicePtr->reqMajVer != majVer) || (servicePtr->reqMinVer != minVer))
    {
        syslog(LOG_ERR,"Service(%u:0x%x/0x%x) versions don't match. OldReqVersion=(%u.%u)," \
                 "NewReqVersion=(%u.%u).", routingId, serviceId, instanceId,
                 servicePtr->reqMajVer, servicePtr->reqMinVer, majVer, minVer);

        return NULL;
    }

    // Search the client-service-session object in the list.
    //
    // Note one SOME/IP client-service-instance can be associcated with multiple clients.
    // A client-service-session represents a client that requests a dedicated SOME/IP
    // client-service-instance through SOME/IP GW service.
    SvcClient_t* svcClientPtr = FindSvcClient(servicePtr, clientSessionRef);

    // Create a client-service-session object if it's not in the list.
    if (svcClientPtr == NULL)
    {
        svcClientPtr = (SvcClient_t*)le_mem_ForceAlloc(SvcClientPool);
        memset(svcClientPtr, 0, sizeof(SvcClient_t));

        // Init the client-service-session object.
        svcClientPtr->clientRef = clientSessionRef;
        svcClientPtr->servicePtr = servicePtr;
        svcClientPtr->state = servicePtr->state;
        svcClientPtr->eventHandlerList = LE_DLS_LIST_INIT;
        svcClientPtr->stateHandlerList = LE_DLS_LIST_INIT;
        svcClientPtr->txMsgList = LE_DLS_LIST_INIT;
        svcClientPtr->txnIdList = LE_DLS_LIST_INIT;
        svcClientPtr->ref =
            (teleplat_someipClnt_ServiceRef_t)le_ref_CreateRef(SvcClientRefMap, svcClientPtr);

        // Increase the SOME/IP service reference count for each client-service-session object.
        le_mem_AddRef(servicePtr);
        if (isServiceCreated)
        {
            // Release the first copy, so that the client-service-instance can be freed after
            // all associcated client-service-session objects get freed.
            le_mem_Release(servicePtr);

            // Call VSOMEIP request service API if the service instance is newly created.
            VSOMEIPRequestService(servicePtr);

            isServiceCreated = false;
        }

        syslog(LOG_DEBUG,"Created serviceRef(%p) for ClientSession(%p) and Service(%u:0x%x/0x%x).",
                svcClientPtr->ref, clientSessionRef, routingId, serviceId, instanceId);
    }

    // Return the client-service-session reference for this dedicated client.
    return svcClientPtr->ref;
}

//--------------------------------------------------------------------------------------------------
/**
 * Release a client-service-session reference.
 */
//--------------------------------------------------------------------------------------------------
le_result_t PlatformSomeipClient::ReleaseService
(
    teleplat_someipClnt_ServiceRef_t serviceRef
)
{
    le_msg_SessionRef_t clientSessionRef = teleplat_someipClnt_GetClientSessionRef();

    // Find the client-service-sessions object in the list.
    SvcClient_t* svcClientPtr = (SvcClient_t*)le_ref_Lookup(SvcClientRefMap, serviceRef);
    if (svcClientPtr == NULL)
    {
        syslog(LOG_ERR,"Bad parameters.");
        return LE_BAD_PARAMETER;
    }

    // Do sanity check.
    assert(svcClientPtr->ref == serviceRef);
    assert(svcClientPtr->servicePtr != NULL);

    // Check if the client-service-session reference is created by this client.
    if (svcClientPtr->clientRef != clientSessionRef)
    {
        syslog(LOG_ERR,"The serviceRef(%p) is not created by this client(%p).",
                 serviceRef, clientSessionRef);
        return LE_NOT_PERMITTED;
    }

    // Check if the client-service-session object can be released.
    if (!IsSvcClientReleasable(svcClientPtr))
    {
        syslog(LOG_ERR,"There are active handlers or pending messages for serviceRef(%p)", serviceRef);
        return LE_BUSY;
    }

    // Release all events/groups of the service instance created by this client.
    ClearGroups(&svcClientPtr->servicePtr->groupList, clientSessionRef);

    // Release the service instance object(decrease the reference counter of the service).
    le_mem_Release(svcClientPtr->servicePtr);

    // Release the client-service-session object.
    le_ref_DeleteRef(SvcClientRefMap, svcClientPtr->ref);
    le_mem_Release(svcClientPtr);

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the SOME/IP client ID.
 */
//--------------------------------------------------------------------------------------------------
uint16_t PlatformSomeipClient::GetClientId
(
    uint8_t routingId
)
{
    return someip_GetClientId(routingId);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get service state.
 */
//--------------------------------------------------------------------------------------------------
le_result_t PlatformSomeipClient::GetState
(
    teleplat_someipClnt_ServiceRef_t serviceRef,
    teleplat_someipClnt_State_t* statePtr
)
{
    le_msg_SessionRef_t clientSessionRef = teleplat_someipClnt_GetClientSessionRef();

    // Find the client-service-session object in the list.
    SvcClient_t* svcClientPtr = (SvcClient_t*)le_ref_Lookup(SvcClientRefMap, serviceRef);
    if ((svcClientPtr == NULL) || (statePtr == NULL))
    {
        syslog(LOG_ERR,"Bad parameters.");
        return LE_BAD_PARAMETER;
    }

    // Do sanity check.
    assert(svcClientPtr->ref == serviceRef);
    assert(svcClientPtr->servicePtr != NULL);

    // Check if the client-service-session reference is created by this client.
    if (svcClientPtr->clientRef != clientSessionRef)
    {
        syslog(LOG_ERR,"The serviceRef(%p) is not created by this client(%p).",
                 serviceRef, clientSessionRef);
        return LE_NOT_PERMITTED;
    }

    if (svcClientPtr->state == SERVICE_STATE_AVAILABLE)
    {
        *statePtr = TELEPLAT_SOMEIPCLNT_AVAILABLE;
    }
    else
    {
        *statePtr = TELEPLAT_SOMEIPCLNT_UNAVAILABLE;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get service version.
 */
//--------------------------------------------------------------------------------------------------
le_result_t PlatformSomeipClient::GetVersion
(
    teleplat_someipClnt_ServiceRef_t serviceRef,
    uint8_t* majVerPtr,
    uint32_t* minVerPtr
)
{
    le_msg_SessionRef_t clientSessionRef = teleplat_someipClnt_GetClientSessionRef();

    // Find the client-service-session object in the list.
    SvcClient_t* svcClientPtr = (SvcClient_t*)le_ref_Lookup(SvcClientRefMap, serviceRef);
    if ((svcClientPtr == NULL) || (majVerPtr == NULL) || (minVerPtr == NULL))
    {
        syslog(LOG_ERR,"Bad parameters.");
        return LE_BAD_PARAMETER;
    }

    // Do sanity check.
    assert(svcClientPtr->ref == serviceRef);
    assert(svcClientPtr->servicePtr != NULL);

    // Check if the client-service-session reference is created by this client.
    if (svcClientPtr->clientRef != clientSessionRef)
    {
        syslog(LOG_ERR,"The serviceRef(%p) is not created by this client(%p).",
                 serviceRef, clientSessionRef);
        return LE_NOT_PERMITTED;
    }

    // Check if the service is available.
    if (svcClientPtr->state != SERVICE_STATE_AVAILABLE)
    {
        syslog(LOG_ERR,"Service(%u:0x%x/0x%x) is unavailable.", svcClientPtr->servicePtr->routingId,
                 svcClientPtr->servicePtr->serviceId, svcClientPtr->servicePtr->instanceId);
        return LE_UNAVAILABLE;
    }

    // Copy the service version once the service is available.
    *majVerPtr = svcClientPtr->servicePtr->majorVersion;
    *minVerPtr = svcClientPtr->servicePtr->minorVersion;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Add a state change handler for a given client-service-session reference.
 */
//--------------------------------------------------------------------------------------------------
teleplat_someipClnt_StateChangeHandlerRef_t PlatformSomeipClient::AddStateChangeHandler
(
    teleplat_someipClnt_ServiceRef_t serviceRef,
    teleplat_someipClnt_StateChangeHandlerFunc_t handlerPtr,
    void* contextPtr
)
{
    le_msg_SessionRef_t clientSessionRef = teleplat_someipClnt_GetClientSessionRef();

    // Find the client-service-session object in the list.
    SvcClient_t* svcClientPtr = (SvcClient_t*)le_ref_Lookup(SvcClientRefMap, serviceRef);
    if ((svcClientPtr == NULL) || (handlerPtr == NULL))
    {
        syslog(LOG_ERR,"Bad parameters.");
        return NULL;
    }

    // Do sanity check.
    assert(svcClientPtr->ref == serviceRef);
    assert(svcClientPtr->servicePtr != NULL);

    // Check if the client-service-session reference is created by this client.
    if (svcClientPtr->clientRef != clientSessionRef)
    {
        syslog(LOG_ERR,"The serviceRef(%p) is not created by this client(%p).",
                 serviceRef, clientSessionRef);
        return NULL;
    }

    // Create a stateHandler object.
    StateHandler_t* stateHandlerPtr = (StateHandler_t*)le_mem_ForceAlloc(StateHandlerPool);
    memset(stateHandlerPtr, 0, sizeof(StateHandler_t));

    // Init the fields.
    stateHandlerPtr->serviceRef = serviceRef;
    stateHandlerPtr->handlefunc = handlerPtr;
    stateHandlerPtr->context = contextPtr;
    stateHandlerPtr->ref =
        (teleplat_someipClnt_StateChangeHandlerRef_t)le_ref_CreateRef(StateHandlerRefMap,
                                                                 stateHandlerPtr);
    // Add the handler into client-service-session's stateHandler list.
    stateHandlerPtr->link = LE_DLS_LINK_INIT;
    le_dls_Queue(&svcClientPtr->stateHandlerList, &stateHandlerPtr->link);
    syslog(LOG_DEBUG,"Created a state handler(Ref=%p) for serviceRef(%p)",
            stateHandlerPtr->ref, serviceRef);

    return stateHandlerPtr->ref;
}

//--------------------------------------------------------------------------------------------------
/**
 * Remove a state change handler.
 */
//--------------------------------------------------------------------------------------------------
void PlatformSomeipClient::RemoveStateChangeHandler
(
    teleplat_someipClnt_StateChangeHandlerRef_t handlerRef
)
{
    le_msg_SessionRef_t clientSessionRef = teleplat_someipClnt_GetClientSessionRef();

    // Find the state handler object in the list.
    StateHandler_t* stateHandlerPtr =
        (StateHandler_t*)le_ref_Lookup(StateHandlerRefMap, handlerRef);

    if (stateHandlerPtr == NULL)
    {
        syslog(LOG_ERR,"Bad handlerRef.");
        return;
    }

    // Sanity check.
    assert(stateHandlerPtr->ref == handlerRef);

    // Find the client-service-session object in the list.
    SvcClient_t* svcClientPtr =
        (SvcClient_t*)le_ref_Lookup(SvcClientRefMap, stateHandlerPtr->serviceRef);

    if (svcClientPtr == NULL)
    {
        syslog(LOG_ERR,"Bad serviceRef.");
        return;
    }

    // Sanity check.
    assert(svcClientPtr->servicePtr != NULL);
    assert(svcClientPtr->ref == stateHandlerPtr->serviceRef);

    if (svcClientPtr->clientRef != clientSessionRef)
    {
        syslog(LOG_ERR,"The handlerRef(%p) is not created by this client(%p).",
                 handlerRef, clientSessionRef);
        return;
    }

    syslog(LOG_DEBUG,"Removed the state handler(Ref=%p) for serviceRef(%p)", handlerRef, svcClientPtr->ref);

    // Remove the handler from the client-service-session's stateHandler list.
    le_dls_Remove(&svcClientPtr->stateHandlerList, &stateHandlerPtr->link);

    // Release the state handler object.
    le_ref_DeleteRef(StateHandlerRefMap, handlerRef);
    le_mem_Release(stateHandlerPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Create a Tx Message and return the message reference.
 */
//--------------------------------------------------------------------------------------------------
teleplat_someipClnt_TxMsgRef_t PlatformSomeipClient::CreateMsg
(
    teleplat_someipClnt_ServiceRef_t serviceRef,
    uint16_t methodId
)
{
    if ((methodId & PLATFORM_SOMEIPDEF_EVENT_MASK) || (methodId == 0x0000) || (methodId == 0x7FFF))
    {
        syslog(LOG_ERR,"Invalid methodId.");
        return NULL;
    }

    le_msg_SessionRef_t clientSessionRef = teleplat_someipClnt_GetClientSessionRef();

    // Find the client-service-session object in the list.
    SvcClient_t* svcClientPtr = (SvcClient_t*)le_ref_Lookup(SvcClientRefMap, serviceRef);
    if (svcClientPtr == NULL)
    {
        syslog(LOG_ERR,"Bad parameters.");
        return NULL;
    }

    // Do sanity check.
    assert(svcClientPtr->ref == serviceRef);
    assert(svcClientPtr->servicePtr != NULL);

    // Check if the client-service-session reference is created by this client.
    if (svcClientPtr->clientRef != clientSessionRef)
    {
        syslog(LOG_ERR,"The serviceRef(%p) is not created by this client(%p).",
                 serviceRef, clientSessionRef);
        return NULL;
    }

    // Create a Tx message object.
    SomeipClnt_TxMsg_t* txMsgPtr = (SomeipClnt_TxMsg_t*)le_mem_ForceAlloc(TxMsgPool);
    memset(txMsgPtr, 0, sizeof(SomeipClnt_TxMsg_t));

    // Init fields.
    txMsgPtr->serviceRef = serviceRef;
    txMsgPtr->methodId = methodId;
    txMsgPtr->isNonRet = false;
    txMsgPtr->isReliable = false;
    txMsgPtr->timeoutMs = 30000;    // TBD: Should be configurable.
    txMsgPtr->payloadPtr = NULL;
    txMsgPtr->ref = (teleplat_someipClnt_TxMsgRef_t)le_ref_CreateRef(TxMsgRefMap, txMsgPtr);

    // Add the message into client-service-session's txMsg list.
    txMsgPtr->link = LE_DLS_LINK_INIT;
    le_dls_Queue(&svcClientPtr->txMsgList, &txMsgPtr->link);
    syslog(LOG_DEBUG,"Created a txMsg (Ref=%p) for serviceRef(%p)", txMsgPtr->ref, serviceRef);

    return txMsgPtr->ref;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the Tx message as a non-return request message.
 */
//--------------------------------------------------------------------------------------------------
le_result_t PlatformSomeipClient::SetNonRet
(
    teleplat_someipClnt_TxMsgRef_t msgRef
)
{
    le_msg_SessionRef_t clientSessionRef = teleplat_someipClnt_GetClientSessionRef();

    // Find the Tx message object in the list.
    SomeipClnt_TxMsg_t* txMsgPtr = (SomeipClnt_TxMsg_t*)le_ref_Lookup(TxMsgRefMap, msgRef);
    if (txMsgPtr == NULL)
    {

        syslog(LOG_ERR,"Bad msgRef.");
        return LE_BAD_PARAMETER;
    }

    // Sanity check.
    assert(txMsgPtr->ref == msgRef);

    // Find the client-service-session object in the list.
    SvcClient_t* svcClientPtr = (SvcClient_t*)le_ref_Lookup(SvcClientRefMap, txMsgPtr->serviceRef);
    if (svcClientPtr == NULL)
    {
        syslog(LOG_ERR,"Bad serviceRef.");
        return LE_BAD_PARAMETER;
    }

    // Do sanity check.
    assert(svcClientPtr->servicePtr != NULL);
    assert(svcClientPtr->ref == txMsgPtr->serviceRef);

    // Check if the client-service-session reference is created by this client.
    if (svcClientPtr->clientRef != clientSessionRef)
    {
        syslog(LOG_ERR,"The msgRef(%p) is not created by this client(%p).", msgRef, clientSessionRef);
        return LE_NOT_PERMITTED;
    }

    // Set the txMsg as a non-return request message.
    txMsgPtr->isNonRet = true;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Send the Tx message through TCP socket.
 */
//--------------------------------------------------------------------------------------------------
le_result_t PlatformSomeipClient::SetReliable
(
    teleplat_someipClnt_TxMsgRef_t msgRef
)
{
    le_msg_SessionRef_t clientSessionRef = teleplat_someipClnt_GetClientSessionRef();

    // Find the Tx message object in the list.
    SomeipClnt_TxMsg_t* txMsgPtr = (SomeipClnt_TxMsg_t*)le_ref_Lookup(TxMsgRefMap, msgRef);
    if (txMsgPtr == NULL)
    {
        syslog(LOG_ERR,"Bad msgRef.");
        return LE_BAD_PARAMETER;
    }

    // Sanity check.
    assert(txMsgPtr->ref == msgRef);

    // Find the client-service-session object in the list.
    SvcClient_t* svcClientPtr = (SvcClient_t*)le_ref_Lookup(SvcClientRefMap, txMsgPtr->serviceRef);
    if (svcClientPtr == NULL)
    {
        syslog(LOG_ERR,"Bad serviceRef.");
        return LE_BAD_PARAMETER;
    }

    // Do sanity check.
    assert(svcClientPtr->servicePtr != NULL);
    assert(svcClientPtr->ref == txMsgPtr->serviceRef);

    // Check if the client-service-session reference is created by this client.
    if (svcClientPtr->clientRef != clientSessionRef)
    {
        syslog(LOG_ERR,"The msgRef(%p) is not created by this client(%p).", msgRef, clientSessionRef);
        return LE_NOT_PERMITTED;
    }

    // Send the Tx message through TCP socket.
    txMsgPtr->isReliable = true;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the timeout milliseconds waiting for the response of the Tx message.
 */
//--------------------------------------------------------------------------------------------------
le_result_t PlatformSomeipClient::SetTimeout
(
    teleplat_someipClnt_TxMsgRef_t msgRef,
    uint32_t timeOut
)
{
    le_msg_SessionRef_t clientSessionRef = teleplat_someipClnt_GetClientSessionRef();

    // Find the Tx message object in the list.
    SomeipClnt_TxMsg_t* txMsgPtr = (SomeipClnt_TxMsg_t*)le_ref_Lookup(TxMsgRefMap, msgRef);
    if (txMsgPtr == NULL)
    {

        syslog(LOG_ERR,"Bad msgRef.");
        return LE_BAD_PARAMETER;
    }

    // Sanity check.
    assert(txMsgPtr->ref == msgRef);

    // Find the client-service-session object in the list.
    SvcClient_t* svcClientPtr = (SvcClient_t*)le_ref_Lookup(SvcClientRefMap, txMsgPtr->serviceRef);
    if (svcClientPtr == NULL)
    {
        syslog(LOG_ERR,"Bad serviceRef.");
        return LE_BAD_PARAMETER;
    }

    // Do sanity check.
    assert(svcClientPtr->servicePtr != NULL);
    assert(svcClientPtr->ref == txMsgPtr->serviceRef);

    // Check if the client-service-session reference is created by this client.
    if (svcClientPtr->clientRef != clientSessionRef)
    {
        syslog(LOG_ERR,"The msgRef(%p) is not created by this client(%p).", msgRef, clientSessionRef);
        return LE_NOT_PERMITTED;
    }

    // Send the timeout value.
    txMsgPtr->timeoutMs = timeOut;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the payload data of the Tx message.
 */
//--------------------------------------------------------------------------------------------------
le_result_t PlatformSomeipClient::SetPayload
(
    teleplat_someipClnt_TxMsgRef_t msgRef,
    const uint8_t* dataPtr,
    size_t dataSize
)
{
    le_msg_SessionRef_t clientSessionRef = teleplat_someipClnt_GetClientSessionRef();

    // Find the Tx message object in the list.
    SomeipClnt_TxMsg_t* txMsgPtr = (SomeipClnt_TxMsg_t*)le_ref_Lookup(TxMsgRefMap, msgRef);
    if (txMsgPtr == NULL)
    {
        syslog(LOG_ERR,"Bad msgRef.");
        return LE_BAD_PARAMETER;
    }

    // Sanity check.
    assert(txMsgPtr->ref == msgRef);

    // Find the client-service-session object in the list.
    SvcClient_t* svcClientPtr = (SvcClient_t*)le_ref_Lookup(SvcClientRefMap, txMsgPtr->serviceRef);
    if (svcClientPtr == NULL)
    {
        syslog(LOG_ERR,"Bad serviceRef.");
        return LE_BAD_PARAMETER;
    }

    // Do sanity check.
    assert(svcClientPtr->servicePtr != NULL);
    assert(svcClientPtr->ref == txMsgPtr->serviceRef);

    // Check if the client-service-session reference is created by this client.
    if (svcClientPtr->clientRef != clientSessionRef)
    {
        syslog(LOG_ERR,"The msgRef(%p) is not created by this client(%p).", msgRef, clientSessionRef);
        return LE_NOT_PERMITTED;
    }

    // Release the old payload data if it contains.
    if (txMsgPtr->payloadPtr != NULL)
    {
        le_mem_Release(txMsgPtr->payloadPtr);
        txMsgPtr->payloadPtr = NULL;

        syslog(LOG_INFO,"Released old payload data for msgRef(%p).", msgRef);
    }

    // Set the payload data.
    if ((dataPtr != NULL) && (dataSize != 0) && (dataSize <= PLATFORM_SOMEIPDEF_MAX_PAYLOAD_SIZE))
    {
        txMsgPtr->payloadPtr = (Payload_t*)le_mem_ForceAlloc(PayloadPool);
        uint8_t* srcPtr = txMsgPtr->payloadPtr->data;
        memcpy(srcPtr, dataPtr, dataSize);
        txMsgPtr->payloadPtr->size = dataSize;
    }
    else
    {
        syslog(LOG_WARNING,"Set NULL payload for msgRef(%p).", msgRef);
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Delete a Tx message.
 */
//--------------------------------------------------------------------------------------------------
le_result_t PlatformSomeipClient::DeleteMsg
(
    teleplat_someipClnt_TxMsgRef_t msgRef
)
{
    le_msg_SessionRef_t clientSessionRef = teleplat_someipClnt_GetClientSessionRef();

    // Find the Tx message object in the list.
    SomeipClnt_TxMsg_t* txMsgPtr = (SomeipClnt_TxMsg_t*)le_ref_Lookup(TxMsgRefMap, msgRef);
    if (txMsgPtr == NULL)
    {

        syslog(LOG_ERR,"Bad msgRef.");
        return LE_BAD_PARAMETER;
    }

    // Sanity check.
    assert(txMsgPtr->ref == msgRef);

    // Find the client-service-session object in the list.
    SvcClient_t* svcClientPtr = (SvcClient_t*)le_ref_Lookup(SvcClientRefMap, txMsgPtr->serviceRef);
    teleplat_someipClnt_ServiceRef_t serviceRef = txMsgPtr->serviceRef;

    if (svcClientPtr == NULL)
    {
        syslog(LOG_ERR,"Bad serviceRef.");
        return LE_BAD_PARAMETER;
    }

    // Do sanity check.
    assert(svcClientPtr->servicePtr != NULL);
    assert(svcClientPtr->ref == txMsgPtr->serviceRef);

    // Check if the client-service-session reference is created by this client.
    if (svcClientPtr->clientRef != clientSessionRef)
    {
        syslog(LOG_ERR,"The msgRef(%p) is not created by this client(%p).", msgRef, clientSessionRef);
        return LE_NOT_PERMITTED;
    }

    // Release the old payload data if have.
    if (txMsgPtr->payloadPtr != NULL)
    {
        le_mem_Release(txMsgPtr->payloadPtr);
        txMsgPtr->payloadPtr = NULL;
        syslog(LOG_DEBUG,"Released payload data of msgRef(%p).", msgRef);
    }

    // Release the Tx message object.
    syslog(LOG_DEBUG,"Freed txMsg(Ref=%p) for serviceRef(%p)", msgRef, serviceRef);
    le_dls_Remove(&svcClientPtr->txMsgList, &txMsgPtr->link);
    le_ref_DeleteRef(TxMsgRefMap, msgRef);
    le_mem_Release(txMsgPtr);

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Send a Tx message and register the response callback handler. The callback will be called
 * once the corresponding response is received.
 */
//--------------------------------------------------------------------------------------------------
void PlatformSomeipClient::RequestResponse
(
    teleplat_someipClnt_TxMsgRef_t msgRef,
    teleplat_someipClnt_RespMsgHandlerFunc_t handlerPtr,
    void* contextPtr
)
{
    le_result_t result = LE_OK;
    uint8_t vsRetCode = PLATFORM_SOMEIPDEF_E_OK;
    bool isErrResp = false;
    le_msg_SessionRef_t clientSessionRef = teleplat_someipClnt_GetClientSessionRef();
    SvcClient_t* svcClientPtr = NULL;
    Txn_t* txnIdPtr = NULL;
    Txn_t* oldTxnIdPtr = NULL;
    bool isNonRet = false;

    // Find the Tx message object in the list.
    SomeipClnt_TxMsg_t* txMsgPtr = (SomeipClnt_TxMsg_t*)le_ref_Lookup(TxMsgRefMap, msgRef);
    if (txMsgPtr == NULL)
    {
        syslog(LOG_ERR,"Bad msgRef.");
        result = LE_BAD_PARAMETER;
        vsRetCode = PLATFORM_SOMEIPDEF_E_NOT_OK;
        isErrResp = true;
        goto ERR_EXIT;
    }

    // Do sanity check.
    assert(txMsgPtr->ref == msgRef);

    // Find the client-service-session object in the list.
    svcClientPtr = (SvcClient_t*)le_ref_Lookup(SvcClientRefMap, txMsgPtr->serviceRef);

    // Do sanity check.
    assert(svcClientPtr != NULL);
    assert(svcClientPtr->servicePtr != NULL);
    assert(svcClientPtr->ref == txMsgPtr->serviceRef);

    // Check if the client-service-session of the msg is created by this client.
    if (svcClientPtr->clientRef != clientSessionRef)
    {
        syslog(LOG_ERR,"The msgRef(%p) is not created by this client(%p).", msgRef, clientSessionRef);
        result = LE_NOT_PERMITTED;
        vsRetCode = PLATFORM_SOMEIPDEF_E_NOT_OK;
        isErrResp = true;
        goto ERR_EXIT;
    }

    // Check if the service of the client-service-session is available.
    if (svcClientPtr->state != SERVICE_STATE_AVAILABLE)
    {
        syslog(LOG_WARNING,"Service(%u:0x%x/0x%x) is unavailable.",
                 svcClientPtr->servicePtr->routingId, svcClientPtr->servicePtr->serviceId,
                 svcClientPtr->servicePtr->instanceId);
        result = LE_UNAVAILABLE;
        vsRetCode = PLATFORM_SOMEIPDEF_E_NOT_REACHABLE;
        isErrResp = true;

        // Release the old payload data if have.
        if (txMsgPtr->payloadPtr != NULL)
        {
            le_mem_Release(txMsgPtr->payloadPtr);
            txMsgPtr->payloadPtr = NULL;
            syslog(LOG_DEBUG,"Freed payload data of msgRef(%p).", msgRef);
        }
        // Release the Tx message object.
        syslog(LOG_DEBUG,"Freed msgRef(%p) for serviceRef(%p)", txMsgPtr->ref, txMsgPtr->serviceRef);

        le_dls_Remove(&svcClientPtr->txMsgList, &txMsgPtr->link);
        le_ref_DeleteRef(TxMsgRefMap, msgRef);
        le_mem_Release(txMsgPtr);

        goto ERR_EXIT;
    }

    // Create a TxnId to track the request-response pair.
    txnIdPtr = (Txn_t*)le_mem_ForceAlloc(TxnPool);
    memset(txnIdPtr, 0, sizeof(Txn_t));

    // Init the fields.
    txnIdPtr->msgHdr.routingId = svcClientPtr->servicePtr->routingId;
    txnIdPtr->msgHdr.serviceId = svcClientPtr->servicePtr->serviceId;
    txnIdPtr->msgHdr.instanceId = svcClientPtr->servicePtr->instanceId;
    txnIdPtr->msgHdr.interfaceVer = svcClientPtr->servicePtr->majorVersion;
    txnIdPtr->msgHdr.methodId = txMsgPtr->methodId;
    txnIdPtr->msgHdr.clientId = someip_GetClientId(svcClientPtr->servicePtr->routingId);

    // Save the serviceRef and response handler.
    txnIdPtr->serviceRef = txMsgPtr->serviceRef;
    txnIdPtr->respHandleFunc = handlerPtr;
    txnIdPtr->contextPtr = contextPtr;
    txnIdPtr->timerRef = NULL;
    txnIdPtr->link = LE_DLS_LINK_INIT;

    syslog(LOG_DEBUG,"Created txnIdPtr(%p).", txnIdPtr);

    // Create a Txn timer for this TxnId if needed.
    isNonRet = txMsgPtr->isNonRet;
    if ((txMsgPtr->timeoutMs != 0) && (!isNonRet))
    {
        char timerName[128] = { 0 };
        assert((size_t)snprintf(timerName, sizeof(timerName), "TxnTimer-0x%x-0x%x",
                  txnIdPtr->msgHdr.sessionId, txMsgPtr->timeoutMs) < sizeof(timerName));
        txnIdPtr->timerRef = le_timer_Create((const char*)timerName);
        le_timer_SetMsInterval(txnIdPtr->timerRef, txMsgPtr->timeoutMs);
        le_timer_SetHandler(txnIdPtr->timerRef, PlatformSomeipClient::PlatformTxnTimerExpiryHandler);
        le_timer_SetWakeup(txnIdPtr->timerRef, false);

        // Save the TxnId in timer's context.
        le_timer_SetContextPtr(txnIdPtr->timerRef, txnIdPtr);

        // Start the timer.
        le_timer_Start(txnIdPtr->timerRef);
    }

    // Send the SOME/IP request through the VSOME/IP interface.
    VSOMEIPSendRequest(&txnIdPtr->msgHdr, txMsgPtr->isNonRet,
                       txMsgPtr->isReliable, txMsgPtr->payloadPtr);

    // Release the old payload data if have.
    if (txMsgPtr->payloadPtr != NULL)
    {
        le_mem_Release(txMsgPtr->payloadPtr);
        txMsgPtr->payloadPtr = NULL;
        syslog(LOG_DEBUG,"Freed payload data of msgRef(%p).", msgRef);
    }
    // Release the Tx message object.
    syslog(LOG_DEBUG,"Freed msgRef(%p) for serviceRef(%p)", txMsgPtr->ref, txMsgPtr->serviceRef);

    le_dls_Remove(&svcClientPtr->txMsgList, &txMsgPtr->link);
    le_ref_DeleteRef(TxMsgRefMap, msgRef);
    le_mem_Release(txMsgPtr);

    // Check if it's a non-return message.
    if (!isNonRet)
    {
        // Add the TxnId into the Txn list.
        le_dls_Queue(&svcClientPtr->txnIdList, &txnIdPtr->link);

        // Put the TxnId into the hash table, using the msgHdr as the key.
        oldTxnIdPtr = (Txn_t*)le_hashmap_Put(TxnHashMapRef, &txnIdPtr->msgHdr, txnIdPtr);

        if (oldTxnIdPtr != NULL)
        {
           // A same TxnId object is found in the hash map, which should not happen.
           // But let's simply call the response handler with LE_TIMEOUT for it since
           // we assume it stays too long.
           teleplat_someipClnt_ServiceRef_t serviceRef = oldTxnIdPtr->serviceRef;
           SvcClient_t* svcClientPtr = (SvcClient_t*)le_ref_Lookup(SvcClientRefMap, serviceRef);

           // Sanity check.
           assert(oldTxnIdPtr->respHandleFunc != NULL);
           assert(svcClientPtr != NULL);
           assert(svcClientPtr->servicePtr != NULL);
           assert(svcClientPtr->ref == serviceRef);

           // Call response handler of this txnId.
           oldTxnIdPtr->respHandleFunc(LE_TIMEOUT, true, PLATFORM_SOMEIPDEF_E_TIMEOUT,
                                       NULL, 0, oldTxnIdPtr->contextPtr);

           syslog(LOG_WARNING,"Freed old txnId(%p) for serviceRef(%p).", oldTxnIdPtr, oldTxnIdPtr->serviceRef);

           // Remove the txnId from the txn list.
           le_dls_Remove(&svcClientPtr->txnIdList, &oldTxnIdPtr->link);

           // Delete this timer.
           if (oldTxnIdPtr->timerRef != NULL)
           {
               le_timer_Delete(oldTxnIdPtr->timerRef);
               oldTxnIdPtr->timerRef = NULL;
           }

           // Delete the txnId object.
           le_mem_Release(oldTxnIdPtr);
        }

        return;
    }

    // For non-return message, free the TxnId object now.
    syslog(LOG_DEBUG,"Freed txnIdPtr(%p) for serviceRef(%p).", txnIdPtr, txnIdPtr->serviceRef);
    le_mem_Release(txnIdPtr);

ERR_EXIT:
    if (handlerPtr != NULL)
    {
        // Call response handler immedidately for non-return request, or if some errors
        // happen before sending the message.
        handlerPtr(result, isErrResp, vsRetCode, NULL, 0, contextPtr);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Enable an group by adding an event into it. This can be called for multiple times if adding more
 * than one event into a group is needed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t PlatformSomeipClient::EnableEventGroup
(
    teleplat_someipClnt_ServiceRef_t serviceRef,
    uint16_t groupId,
    uint16_t eventId,
    platform_someipDef_EventType_t eventType
)
{
    bool isGroupCreated = false;
    bool isEventCreated = false;

    // Check the vaild eventId and eventGroupId according to [PRS_SOMEIPSD_00517] and
    // [PRS_SOMEIPSD_00531] of <SOME/IP Service Discovery Protocol Specification AUTOSAR FO R21-11>.
    if (((!(eventId & PLATFORM_SOMEIPDEF_EVENT_MASK)) || (eventId == PLATFORM_SOMEIPDEF_EVENT_MASK) ||
          (eventId == 0xFFFF)) || ((groupId == 0x0000) || (groupId == 0xFFFF)))
    {
        syslog(LOG_ERR,"Invalid eventId or groupId.");
        return LE_BAD_PARAMETER;
    }

    le_msg_SessionRef_t clientSessionRef = teleplat_someipClnt_GetClientSessionRef();

    // Find the client-service-session object in the list.
    SvcClient_t* svcClientPtr = (SvcClient_t*)le_ref_Lookup(SvcClientRefMap, serviceRef);
    if (svcClientPtr == NULL)
    {
        syslog(LOG_ERR,"Bad parameters.");
        return LE_BAD_PARAMETER;
    }

    // Do sanity check.
    assert(svcClientPtr->ref == serviceRef);
    assert(svcClientPtr->servicePtr != NULL);

    // Check if the client-service-session reference is created by this client.
    if (svcClientPtr->clientRef != clientSessionRef)
    {
        syslog(LOG_ERR,"The serviceRef(%p) is not created by this client(%p).",
                 serviceRef, clientSessionRef);
        return LE_NOT_PERMITTED;
    }

    // Get the service object.
    SomeipClnt_Service_t* servicePtr = svcClientPtr->servicePtr;

    // Get the group object.
    SomeipClnt_Group_t* groupPtr = FindGroup(&servicePtr->groupList, groupId);
    if (groupPtr == NULL)
    {
        // Create a new group for this service.
        isGroupCreated = true;
        groupPtr = (SomeipClnt_Group_t*)le_mem_ForceAlloc(GroupPool);
        memset(groupPtr, 0, sizeof(SomeipClnt_Group_t));

        // Init the fields.
        groupPtr->clientRef = clientSessionRef;
        groupPtr->groupId = groupId;
        groupPtr->setForSubscription = false;
        groupPtr->isSubscribed = false;
        groupPtr->servicePtr = servicePtr;
        groupPtr->link = LE_DLS_LINK_INIT;

        // Add the group into the service's group list.
        le_dls_Queue(&servicePtr->groupList, &groupPtr->link);
        syslog(LOG_DEBUG,"Created group(0x%x) for service(%u:0x%x/0x%x).",
                 groupId, servicePtr->routingId, servicePtr->serviceId, servicePtr->instanceId);
    }

    // Check if the group of the service is created by this client.
    if (groupPtr->clientRef != clientSessionRef)
    {
        syslog(LOG_ERR,"The group(0x%x) of service(%u:0x%x/0x%x) is not created by this client(%p).",
                 groupId, servicePtr->routingId, servicePtr->serviceId, servicePtr->instanceId,
                 clientSessionRef);
        return LE_NOT_PERMITTED;
    }

    // Check if the group is set for subscription, which is not allowed to add an event.
    if (groupPtr->setForSubscription)
    {
        syslog(LOG_ERR,"The group(0x%x) of service(%u:0x%x/0x%x) is already set for subscription.",
                 groupId, servicePtr->routingId, servicePtr->serviceId, servicePtr->instanceId);
        return LE_NOT_PERMITTED;
    }

    // Get the event object.
    SomeipClnt_Event_t* eventPtr = FindEvent(&servicePtr->eventList, eventId);
    if (eventPtr == NULL)
    {
        // Create a new event for this service.
        isEventCreated = true;
        eventPtr = (SomeipClnt_Event_t*)le_mem_ForceAlloc(EventPool);
        memset(eventPtr, 0, sizeof(SomeipClnt_Event_t));

        // Init the fields.
        eventPtr->clientRef = clientSessionRef;
        eventPtr->eventId = eventId;
        eventPtr->eventType = eventType;
        eventPtr->groupId = groupId;
        eventPtr->servicePtr = servicePtr;
        eventPtr->link = LE_DLS_LINK_INIT;

        // Add the event to the service's event list.
        le_dls_Queue(&servicePtr->eventList, &eventPtr->link);

        syslog(LOG_DEBUG,"Created event(0x%x) for service(%u:0x%x/0x%x).",
                 eventId, servicePtr->routingId, servicePtr->serviceId, servicePtr->instanceId);
    }

    if (!isEventCreated)
    {
        // Check if the event of the service is created by this client.
        if (eventPtr->clientRef != clientSessionRef)
        {
            syslog(LOG_ERR,"The event(0x%x) of service(%u:0x%x/0x%x) is not created by this client(%p).",
                     eventId, servicePtr->routingId, servicePtr->serviceId, servicePtr->instanceId,
                     clientSessionRef);
            if (isGroupCreated)
            {
                // Remove the new created group object because we can not add the event to it.
                le_dls_Remove(&servicePtr->groupList, &groupPtr->link);
                le_mem_Release(groupPtr);
            }

            return LE_NOT_PERMITTED;
        }

        if (eventPtr->groupId != groupId)
        {
            syslog(LOG_ERR,"The event(0x%x) of service(%u:0x%x/0x%x) is already in another group(0x%x).",
                     eventId, servicePtr->routingId, servicePtr->serviceId, servicePtr->instanceId,
                     eventPtr->groupId);
            if (isGroupCreated)
            {
                // Remove the new created group object because we can not add the event to it.
                le_dls_Remove(&servicePtr->groupList, &groupPtr->link);
                le_mem_Release(groupPtr);
            }

            return LE_NOT_PERMITTED;
        }

        syslog(LOG_WARNING,"The event(0x%x) of service(%u:0x%x/0x%x) is already in this group(0x%x).",
                eventId, servicePtr->routingId, servicePtr->serviceId, servicePtr->instanceId,
                groupId);
        return LE_DUPLICATE;
    }

    syslog(LOG_DEBUG,"Added the event(0x%x) of service(%u:0x%x/0x%x) to the group(0x%x).", eventId,
             servicePtr->routingId, servicePtr->serviceId, servicePtr->instanceId, groupId);

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Disable an group of a service.
 */
//--------------------------------------------------------------------------------------------------
le_result_t PlatformSomeipClient::DisableEventGroup
(
    teleplat_someipClnt_ServiceRef_t serviceRef,
    uint16_t groupId
)
{
    if ((groupId == 0x0000) || (groupId == 0xFFFF))
    {
        syslog(LOG_ERR,"Invalid groupId.");
        return LE_BAD_PARAMETER;
    }

    le_msg_SessionRef_t clientSessionRef = teleplat_someipClnt_GetClientSessionRef();

    // Find the client-service-session object in the list.
    SvcClient_t* svcClientPtr = (SvcClient_t*)le_ref_Lookup(SvcClientRefMap, serviceRef);
    if (svcClientPtr == NULL)
    {
        syslog(LOG_ERR,"Bad parameters.");
        return LE_BAD_PARAMETER;
    }

    // Do sanity check.
    assert(svcClientPtr->ref == serviceRef);
    assert(svcClientPtr->servicePtr != NULL);

    // Check if the client-service-session reference is created by this client.
    if (svcClientPtr->clientRef != clientSessionRef)
    {
        syslog(LOG_ERR,"The serviceRef(%p) is not created by this client(%p).",
                 serviceRef, clientSessionRef);
        return LE_NOT_PERMITTED;
    }

    // Get the service object.
    SomeipClnt_Service_t* servicePtr = svcClientPtr->servicePtr;

    // Get the group object.
    SomeipClnt_Group_t* groupPtr = FindGroup(&servicePtr->groupList, groupId);
    if (groupPtr == NULL)
    {
        syslog(LOG_WARNING,"The group(0x%x) of Service(%u:0x%x/0x%x) is already disabled.",
                groupId, servicePtr->routingId, servicePtr->serviceId, servicePtr->instanceId);
        return LE_DUPLICATE;
    }

    // Check if the group of the service is created by this client.
    if (groupPtr->clientRef != clientSessionRef)
    {
        syslog(LOG_ERR,"The group(0x%x) of service(%u:0x%x/0x%x) is not created by this client(%p).",
                 groupId, servicePtr->routingId, servicePtr->serviceId, servicePtr->instanceId,
                 clientSessionRef);
        return LE_NOT_PERMITTED;
    }

    // Check if the group is already set for subscription.
    if (groupPtr->setForSubscription)
    {
        syslog(LOG_ERR,"The group(0x%x) of service(%u:0x%x/0x%x) is already set for subscription.",
                 groupId, servicePtr->routingId, servicePtr->serviceId, servicePtr->instanceId);
        return LE_NOT_PERMITTED;
    }

    syslog(LOG_DEBUG,"Deleted group(0x%x) for service(%u:0x%x/0x%x).",
             groupId, servicePtr->routingId, servicePtr->serviceId, servicePtr->instanceId);

    // Remove the group from service group list.
    le_dls_Remove(&servicePtr->groupList, &groupPtr->link);
    le_mem_Release(groupPtr);

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Subscribe an event group for a service.
 */
//--------------------------------------------------------------------------------------------------
le_result_t PlatformSomeipClient::SubscribeEventGroup
(
    teleplat_someipClnt_ServiceRef_t serviceRef,
    uint16_t groupId
)
{
    if ((groupId == 0x0000) || (groupId == 0xFFFF))
    {
        syslog(LOG_ERR,"Invalid groupId.");
        return LE_BAD_PARAMETER;
    }

    le_msg_SessionRef_t clientSessionRef = teleplat_someipClnt_GetClientSessionRef();

    // Find the client-service-session object in the list.
    SvcClient_t* svcClientPtr = (SvcClient_t*)le_ref_Lookup(SvcClientRefMap, serviceRef);
    if (svcClientPtr == NULL)
    {
        syslog(LOG_ERR,"Bad parameters.");
        return LE_BAD_PARAMETER;
    }

    // Do sanity check.
    assert(svcClientPtr->ref == serviceRef);
    assert(svcClientPtr->servicePtr != NULL);

    // Check if the client-service-session reference is created by this client.
    if (svcClientPtr->clientRef != clientSessionRef)
    {
        syslog(LOG_ERR,"The serviceRef(%p) is not created by this client(%p).",
                 serviceRef, clientSessionRef);
        return LE_NOT_PERMITTED;
    }

    // Get the service object.
    SomeipClnt_Service_t* servicePtr = svcClientPtr->servicePtr;

    // Get the group object.
    SomeipClnt_Group_t* groupPtr = FindGroup(&servicePtr->groupList, groupId);
    if (groupPtr == NULL)
    {
        syslog(LOG_WARNING,"The group(0x%x) of Service(%u:0x%x/0x%x) is not enabled.",
                groupId, servicePtr->routingId, servicePtr->serviceId, servicePtr->instanceId);
        return LE_NOT_PERMITTED;
    }

    // Check if the group of the service is created by this client.
    if (groupPtr->clientRef != clientSessionRef)
    {
        syslog(LOG_ERR,"The group(0x%x) of service(%u:0x%x/0x%x) is not created by this client(%p).",
                 groupId, servicePtr->routingId, servicePtr->serviceId, servicePtr->instanceId,
                 clientSessionRef);
        return LE_NOT_PERMITTED;
    }

    // Check if the group is already set for subscription.
    if (groupPtr->setForSubscription)
    {
        syslog(LOG_WARNING,"The group(0x%x) of service(%u:0x%x/0x%x) is already set for subscription.",
                 groupId, servicePtr->routingId, servicePtr->serviceId, servicePtr->instanceId);
        return LE_DUPLICATE;
    }

    // Check if the service is available since we need specify the service interface
    // version when subscribing a group.
    if ((servicePtr->state == SERVICE_STATE_AVAILABLE) &&
        (!groupPtr->isSubscribed))
    {
        VSOMEIPSubscribeEventGroup(groupPtr);
    }

    syslog(LOG_DEBUG,"Set for subscription for group(0x%x) of service(%u:0x%x/0x%x).",
             groupId, servicePtr->routingId, servicePtr->serviceId, servicePtr->instanceId);
    groupPtr->setForSubscription = true;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Unsubscribe an event group for a service.
 */
//--------------------------------------------------------------------------------------------------
le_result_t PlatformSomeipClient::UnsubscribeEventGroup
(
    teleplat_someipClnt_ServiceRef_t serviceRef,
    uint16_t groupId
)
{
    if ((groupId == 0x0000) || (groupId == 0xFFFF))
    {
        syslog(LOG_ERR,"Invalid groupId.");
        return LE_BAD_PARAMETER;
    }

    le_msg_SessionRef_t clientSessionRef = teleplat_someipClnt_GetClientSessionRef();

    // Find the client-service-session object in the list.
    SvcClient_t* svcClientPtr = (SvcClient_t*)le_ref_Lookup(SvcClientRefMap, serviceRef);
    if (svcClientPtr == NULL)
    {
        syslog(LOG_ERR,"Bad parameters.");
        return LE_BAD_PARAMETER;
    }

    // Do sanity check.
    assert(svcClientPtr->ref == serviceRef);
    assert(svcClientPtr->servicePtr != NULL);

    // Check if the client-service-session reference is created by this client.
    if (svcClientPtr->clientRef != clientSessionRef)
    {
        syslog(LOG_ERR,"The serviceRef(%p) is not created by this client(%p).",
                 serviceRef, clientSessionRef);
        return LE_NOT_PERMITTED;
    }

    // Get the service object.
    SomeipClnt_Service_t* servicePtr = svcClientPtr->servicePtr;

    // Get the group object.
    SomeipClnt_Group_t* groupPtr = FindGroup(&servicePtr->groupList, groupId);
    if (groupPtr == NULL)
    {
        syslog(LOG_WARNING,"The group(0x%x) of Service(%u:0x%x/0x%x) is not enabled.",
                groupId, servicePtr->routingId, servicePtr->serviceId, servicePtr->instanceId);
        return LE_NOT_PERMITTED;
    }

    // Check if the group of the service is created by this client.
    if (groupPtr->clientRef != clientSessionRef)
    {
        syslog(LOG_ERR,"The group(0x%x) of service(%u:0x%x/0x%x) is not created by this client(%p).",
                 groupId, servicePtr->routingId, servicePtr->serviceId, servicePtr->instanceId,
                 clientSessionRef);
        return LE_NOT_PERMITTED;
    }

    // Check if the group is set for subscription.
    if (!groupPtr->setForSubscription)
    {
        syslog(LOG_WARNING,"The group(0x%x) of service(%u:0x%x/0x%x) is not set for subscription yet.",
                 groupId, servicePtr->routingId, servicePtr->serviceId, servicePtr->instanceId);
        return LE_DUPLICATE;
    }

    // Unsubscribe the event group in VSOMEIP layer if it's already subscribed.
    if (groupPtr->isSubscribed)
    {
        VSOMEIPUnsubscribeEventGroup(groupPtr);
    }

    syslog(LOG_DEBUG,"Unset for subsription for group(0x%x) of service(%u:0x%x/0x%x).",
             groupId, servicePtr->routingId, servicePtr->serviceId, servicePtr->instanceId);
    groupPtr->setForSubscription = false;

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Add an event message handler for a given client-service-session.
 */
//--------------------------------------------------------------------------------------------------
teleplat_someipClnt_EventMsgHandlerRef_t PlatformSomeipClient::AddEventMsgHandler
(
    teleplat_someipClnt_ServiceRef_t serviceRef,
    uint16_t groupId,
    teleplat_someipClnt_EventMsgHandlerFunc_t handlerPtr,
    void * contextPtr
)
{
    le_msg_SessionRef_t clientSessionRef = teleplat_someipClnt_GetClientSessionRef();

    // Find the client-service-session object in the list.
    SvcClient_t* svcClientPtr = (SvcClient_t*)le_ref_Lookup(SvcClientRefMap, serviceRef);
    if ((svcClientPtr == NULL) || (handlerPtr == NULL))
    {
        syslog(LOG_ERR,"Bad parameters.");
        return NULL;
    }

    // Do sanity check.
    assert(svcClientPtr->ref == serviceRef);
    assert(svcClientPtr->servicePtr != NULL);

    // Check if the service reference is created by this client.
    if (svcClientPtr->clientRef != clientSessionRef)
    {
        syslog(LOG_ERR,"The serviceRef(%p) is not created by this client(%p).",
                 serviceRef, clientSessionRef);
        return NULL;
    }

    // Create an eventHandler object.
    EventHandler_t* eventHandlerPtr = (EventHandler_t*)le_mem_ForceAlloc(EventHandlerPool);
    memset(eventHandlerPtr, 0, sizeof(EventHandler_t));

    // Init the fields.
    eventHandlerPtr->serviceRef = serviceRef;
    eventHandlerPtr->groupId = groupId;
    eventHandlerPtr->handlefunc = handlerPtr;
    eventHandlerPtr->context = contextPtr;
    eventHandlerPtr->ref =
        (teleplat_someipClnt_EventMsgHandlerRef_t)le_ref_CreateRef(EventHandlerRefMap,
                                                                 eventHandlerPtr);
    // Add the handler into client-service-session's stateHandler list.
    eventHandlerPtr->link = LE_DLS_LINK_INIT;
    le_dls_Queue(&svcClientPtr->eventHandlerList, &eventHandlerPtr->link);
    syslog(LOG_DEBUG,"Created an event handler(Ref=%p) for serviceRef(%p)",
             eventHandlerPtr->ref, serviceRef);

    return eventHandlerPtr->ref;
}

//--------------------------------------------------------------------------------------------------
/**
 * Remove an event message handler.
 */
//--------------------------------------------------------------------------------------------------
void PlatformSomeipClient::RemoveEventMsgHandler
(
    teleplat_someipClnt_EventMsgHandlerRef_t handlerRef
)
{
    le_msg_SessionRef_t clientSessionRef = teleplat_someipClnt_GetClientSessionRef();

    // Find the event handler object in the list.
    EventHandler_t* eventHandlerPtr =
        (EventHandler_t*)le_ref_Lookup(EventHandlerRefMap, handlerRef);

    if (eventHandlerPtr == NULL)
    {
        syslog(LOG_ERR,"Bad handlerRef.");
        return;
    }

    // Sanity check.
    assert(eventHandlerPtr->ref == handlerRef);

    // Find the client-service-session object in the list.
    SvcClient_t* svcClientPtr =
        (SvcClient_t*)le_ref_Lookup(SvcClientRefMap, eventHandlerPtr->serviceRef);

    if (svcClientPtr == NULL)
    {
        syslog(LOG_ERR,"Bad serviceRef.");
        return;
    }

    // Sanity check.
    assert(svcClientPtr->servicePtr != NULL);
    assert(svcClientPtr->ref ==  eventHandlerPtr->serviceRef);

    if (svcClientPtr->clientRef != clientSessionRef)
    {
        syslog(LOG_ERR,"The handlerRef(%p) is not created by this client(%p).",
                 handlerRef, clientSessionRef);
        return;
    }

    syslog(LOG_DEBUG,"Removed the event handler(Ref=%p) for serviceRef(%p)", handlerRef, svcClientPtr->ref);

    // Remove the handler from the client-service-session's stateHandler list.
    le_dls_Remove(&svcClientPtr->eventHandlerList, &eventHandlerPtr->link);

    // Release the handler object.
    le_ref_DeleteRef(EventHandlerRefMap, handlerRef);
    le_mem_Release(eventHandlerPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Initialization.
 */
//--------------------------------------------------------------------------------------------------
void PlatformSomeipClient::Init
(
    void
)
{
    // Init the global variables.
    ServiceList = LE_DLS_LIST_INIT;

    // Create reference maps.
    SvcClientRefMap = le_ref_CreateMap("Client SvcClientRefMap", CLNT_SERVICE_REF_CNT);
    TxMsgRefMap = le_ref_CreateMap("Client TxMsgRefMap", CLNT_TXMSG_REF_CNT);
    StateHandlerRefMap = le_ref_CreateMap("Client StateHandlerRefMap", CLNT_STATE_HANDLER_REF_CNT);
    EventHandlerRefMap = le_ref_CreateMap("Client EventHandlerRefMap", CLNT_EVENT_HANDLER_REF_CNT);

    // Create a hash map for TxnIds.
    TxnHashMapRef = le_hashmap_Create("Client TxnHashMap", CLNT_MAX_TXNID_CNT,
                                      PlatformSomeipClient::HashComputeTxnId,
                                      PlatformSomeipClient::HashCompareTxnIds);

    // Create memory pools.
    ServicePool = le_mem_CreatePool("Client ServicePool", sizeof(SomeipClnt_Service_t));
    RespMsgPool = le_mem_CreatePool("Client RespMsgPool", sizeof(SomeipClnt_RespMsg_t));
    TxMsgPool = le_mem_CreatePool("Client TxMsgPool", sizeof(SomeipClnt_TxMsg_t));
    PayloadPool = le_mem_CreatePool("Client PayloadPool", sizeof(Payload_t));
    StateHandlerPool = le_mem_CreatePool("Client StateHandlerPool", sizeof(StateHandler_t));
    SvcClientPool = le_mem_CreatePool("Client SvcClientPool", sizeof(SvcClient_t));
    TxnPool = le_mem_CreatePool("Client TxnPool", sizeof(Txn_t));
    GroupPool = le_mem_CreatePool("Client GroupPool", sizeof(SomeipClnt_Group_t));
    EventPool = le_mem_CreatePool("Client EventPool", sizeof(SomeipClnt_Event_t));
    EventHandlerPool = le_mem_CreatePool("Client EventHandlerPool", sizeof(EventHandler_t));
    EventMsgPool = le_mem_CreatePool("Client EventMessagePool", sizeof(SomeipClnt_EventMsg_t));

    // Set memory pool destructors.
    le_mem_SetDestructor(ServicePool, PlatformSomeipClient::PlatformServiceDestructor);
    le_mem_SetDestructor(GroupPool, PlatformSomeipClient::PlatformGroupDestructor);

    // Create the event and add event handler.
    VsMsgEvent = le_event_CreateId("Client VSOMEIP message event", sizeof(VsClntMsg_t));
    VsMsgEventHandlerRef = le_event_AddHandler("Client VSOMEIP message event handler",
                                               VsMsgEvent, PlatformSomeipClient::PlatformVsMsgHandler);
    // Create client session close hander.
    le_msg_AddServiceCloseHandler(teleplat_someipClnt_GetServiceRef(),
                                  PlatformSomeipClient::PlatformClientDisconnection, NULL);

    syslog(LOG_INFO,"PlatformSomeipClient Service started");
}
