/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * Main entry point for teleplat-someipgw application.
 */

#include "telePlatSomeipGWSvc.hpp"
#include <syslog.h>
#include <signal.h>
#include <unistd.h>

namespace { volatile sig_atomic_t gStop = 0; }

static void SignalHandler(int signum)
{
    gStop = 1;
}

int main(int argc, char* argv[])
{
    // Open syslog for logging
    openlog("teleplat-someipgw", LOG_PID | LOG_CONS, LOG_DAEMON);
    
    // Set up signal handlers
    signal(SIGTERM, SignalHandler);
    signal(SIGINT, SignalHandler);
    
    syslog(LOG_INFO, "teleplat-someipgw starting...");
    
    // Initialize the Platform SOME/IP GW service
    // This replaces the COMPONENT_INIT macro
    telePlatSomeipGWSvc_Init();
    
    syslog(LOG_INFO, "teleplat-someipgw initialized successfully");
    
    // Main loop - keep the application running
    while (!gStop)
    {
        sleep(1);
    }
    
    syslog(LOG_INFO, "teleplat-someipgw shutting down");
    closelog();
    
    return 0;
}
