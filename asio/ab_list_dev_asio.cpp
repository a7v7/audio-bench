//------------------------------------------------------------------------------
// MIT License
//
// Copyright (c) 2025 Anthony Verbeck
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//------------------------------------------------------------------------------

/*
 * ab_list_dev_asio.cpp
 * ASIO Device Lister for audio-bench
 *
 * Lists all ASIO devices and their status
 * Shows channel counts, version info for attached devices
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <popt.h>
#include "asiosys.h"
#include "asio.h"
#include "iasiodrv.h"
#include "asiodrivers.h"

//------------------------------------------------------------------------------
// ASIO Device Probe
//------------------------------------------------------------------------------

// Global ASIODriverInfo - must be global because ASIO driver keeps a reference to it
static ASIODriverInfo g_driverInfo;

struct DeviceInfo {
    bool isAttached;
    long numInputChannels;
    long numOutputChannels;
    char driverName[256];
    long asioVersion;
    long driverVersion;
};

static bool probeASIODevice(const char* driverName, DeviceInfo* info)
{
    AsioDrivers* asioDrivers = new AsioDrivers();

    // Initialize structure
    info->isAttached = false;
    info->numInputChannels = 0;
    info->numOutputChannels = 0;
    info->asioVersion = 0;
    info->driverVersion = 0;
    strncpy(info->driverName, driverName, sizeof(info->driverName) - 1);
    info->driverName[sizeof(info->driverName) - 1] = '\0';

    // Attempt to load the driver
    if (!asioDrivers->loadDriver(const_cast<char*>(driverName))) {
        delete asioDrivers;
        return false;
    }

    // Attempt to initialize the driver
    memset(&g_driverInfo, 0, sizeof(g_driverInfo));
    g_driverInfo.asioVersion = 2;
    g_driverInfo.sysRef = nullptr;

    ASIOError err = ASIOInit(&g_driverInfo);
    if (err != ASE_OK) {
        asioDrivers->removeCurrentDriver();
        delete asioDrivers;
        return false;
    }

    // Device is attached - get information
    info->isAttached = true;
    info->asioVersion = g_driverInfo.asioVersion;
    info->driverVersion = g_driverInfo.driverVersion;

    // Get channel counts
    err = ASIOGetChannels(&info->numInputChannels, &info->numOutputChannels);
    if (err != ASE_OK) {
        // Even if we can't get channels, device is still attached
        info->numInputChannels = 0;
        info->numOutputChannels = 0;
    }

    // Clean up
    // IMPORTANT: We do NOT call ASIOExit() here!
    // The ASIO driver crashes if ASIOExit() is called without first calling
    // ASIOStart() and setting up buffers properly. Since we're just probing
    // device capabilities, we simply unload the driver using removeCurrentDriver().
    asioDrivers->removeCurrentDriver();
    delete asioDrivers;

    return true;
}

//------------------------------------------------------------------------------
// Main Program
//------------------------------------------------------------------------------

int main(int argc, const char** argv)
{
    CoInitialize(nullptr);

    // Command-line options
    int versionFlag = 0;

    struct poptOption options[] = {
        {"version", 'v', POPT_ARG_NONE, &versionFlag, 0,
         "Show version information", nullptr},
        POPT_AUTOHELP
        POPT_TABLEEND
    };

    poptContext popt_ctx = poptGetContext(nullptr, argc, argv, options, 0);
    poptSetOtherOptionHelp(popt_ctx,
        "[OPTIONS]\n\n"
        "ASIO Device Lister - Lists all ASIO devices and their status\n\n"
        "Examples:\n"
        "  ab_list_dev_asio           # List all ASIO devices\n"
        "  ab_list_dev_asio --version # Show version information\n");

    int rc = poptGetNextOpt(popt_ctx);
    if (rc < -1) {
        fprintf(stderr, "Error: %s: %s\n",
                poptBadOption(popt_ctx, POPT_BADOPTION_NOALIAS),
                poptStrerror(rc));
        poptFreeContext(popt_ctx);
        CoUninitialize();
        return 1;
    }

    // Handle version mode
    if (versionFlag) {
        printf("ab_list_dev_asio version 1.0.0\n");
        printf("ASIO Device Lister for audio-bench\n");
        printf("Copyright (c) 2025 Anthony Verbeck\n");
        poptFreeContext(popt_ctx);
        CoUninitialize();
        return 0;
    }

    poptFreeContext(popt_ctx);

    printf("ASIO Device List\n");
    printf("================================================================================\n\n");

    // Get list of ASIO drivers
    char* driverNames[32];
    char driverNameBuffer[32][256];

    for (int i = 0; i < 32; i++) {
        driverNames[i] = driverNameBuffer[i];
    }

    // Create AsioDrivers object just to get names, then destroy it
    // before probing devices to avoid having multiple instances
    long numDrivers;
    {
        AsioDrivers asioDrivers;
        numDrivers = asioDrivers.getDriverNames(driverNames, 32);
    } // asioDrivers destroyed here

    if (numDrivers == 0) {
        printf("No ASIO drivers found.\n");
        printf("\nNote: ASIO drivers must be installed separately.\n");
        CoUninitialize();
        return 0;
    }

    printf("Found %ld ASIO driver(s):\n\n", numDrivers);

    // Probe each driver
    for (long i = 0; i < numDrivers; i++) {
        printf("Device %2ld: %s\n", i, driverNames[i]);

        DeviceInfo info;
        if (probeASIODevice(driverNames[i], &info)) {
            printf("           Status: ATTACHED\n");
            printf("           Input channels:  %ld\n", info.numInputChannels);
            printf("           Output channels: %ld\n", info.numOutputChannels);
            printf("           ASIO version:    %ld\n", info.asioVersion);
            printf("           Driver version:  %ld\n", info.driverVersion);
        } else {
            printf("           Status: NOT ATTACHED\n");
        }
        printf("\n");
    }

    printf("================================================================================\n");
    printf("Total devices: %ld\n", numDrivers);

    CoUninitialize();
    return 0;
}
