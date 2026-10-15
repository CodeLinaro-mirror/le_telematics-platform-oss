/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *  SPDX-License-Identifier: BSD-3-Clause-Clear
 */


#ifndef PLATFORMSOMEIPGWSVC_HPP
#define PLATFORMSOMEIPGWSVC_HPP

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

//--------------------------------------------------------------------------------------------------
/**
 * Get the vsomeip routing manager instance by routing ID.
 */
//--------------------------------------------------------------------------------------------------
std::shared_ptr<vsomeip::application>& someip_GetRoutingManager
(
    uint8_t id
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the vsomeip client ID by routing ID.
 */
//--------------------------------------------------------------------------------------------------
uint16_t someip_GetClientId
(
    uint8_t id
);

//--------------------------------------------------------------------------------------------------
/**
 * Initialize the Platform SOME/IP GW service component.
 * This function replaces the COMPONENT_INIT macro and should be called from main().
 */
//--------------------------------------------------------------------------------------------------
void telePlatSomeipGWSvc_Init(void);

#endif /* #ifndef PLATFORMSOMEIPGWSVC_HPP */
