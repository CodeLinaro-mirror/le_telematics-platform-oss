/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * platformSvcIF.hpp - Platform Service Interface
 * 
 * This file defines the base interface for platform services.
 * It provides a common interface that platform service implementations
 * can inherit from.
 */

#ifndef PLATFORMSVCIF_HPP
#define PLATFORMSVCIF_HPP

namespace platformsvc
{
    /**
     * Base interface class for platform services
     */
    class IPlatformSvc
    {
    public:
        /**
         * Virtual destructor
         */
        virtual ~IPlatformSvc() {}
        
        /**
         * Initialize the service
         */
        virtual void Init() = 0;
    };
}

#endif // PLATFORMSVCIF_HPP