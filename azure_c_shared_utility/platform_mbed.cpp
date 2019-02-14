// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "azure_c_shared_utility/platform.h"
#include "NetworkInterface.h"
#include "BG96Interface.h"
#include "GNSSInterface.h"

#include "NTPClient.h"
#include "azure_c_shared_utility/optimize_size.h"
#include "azure_c_shared_utility/xio.h"
#include "azure_c_shared_utility/tlsio_wolfssl.h"

NetworkInterface* platform_network;
BG96Interface* bg96;
GNSSInterface* platform_gnss;

int setupRealTime(void)
{
    int result;
    bg96 = new BG96Interface();
    result = bg96->connect();
    if (result) {
        platform_network = bg96;
        platform_gnss = bg96;
    } else {
        platform_network = NULL;
        platform_gnss = NULL;
    }
    if (platform_network == NULL) {
        result = __FAILURE__;
    } 
    else
    {
        // NTPClient ntp(platform_network);
        // char server[16]="time.google.com";
        // ntp.set_server(server, 123);
        // time_t timestamp=ntp.get_timestamp();
        // if (timestamp < 0) {
        //     result = __FAILURE__;
        // } else {
        //     set_time(timestamp);
        //     printf("[ Platform  ] Time set to %s\n",ctime(&timestamp));
        //     result = 0;
        // }
        result = 0;
    }

    return result;
}

int platform_init(void)
{
    if (setupRealTime() != 0)
    {
        return __FAILURE__;
    } 
    else
    {
        return 0;
    }
}

const IO_INTERFACE_DESCRIPTION* platform_get_default_tlsio(void)
{
    return tlsio_wolfssl_get_interface_description();
}

STRING_HANDLE platform_get_platform_info(void)
{
    // Expected format: "(<runtime name>; <operating system name>; <platform>)"

    return STRING_construct("(testmqtt; mbed; NUCLEO_L476RG)");
}

void platform_deinit(void)
{
    platform_network = NULL;
    platform_gnss = NULL;
    delete(bg96);
}
