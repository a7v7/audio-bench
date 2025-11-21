/*
 * ab_list_dev_asio.cpp - ASIO Device Lister
 * Part of the audio_bench suite
 *
 * Lists all ASIO devices and attempts to open each one to check status
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "asiosys.h"
#include "asio.h"
#include "iasiodrv.h"
#include "asiodrivers.h"

//------------------------------------------------------------------------------
// ASIO Device Probe
//------------------------------------------------------------------------------

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
    ASIODriverInfo driverInfo = {0};
    driverInfo.asioVersion = 2;
    driverInfo.sysRef = nullptr;

    ASIOError err = ASIOInit(&driverInfo);
    if (err != ASE_OK) {
        asioDrivers->removeCurrentDriver();
        delete asioDrivers;
        return false;
    }

    // Device is attached - get information
    info->isAttached = true;
    info->asioVersion = driverInfo.asioVersion;
    info->driverVersion = driverInfo.driverVersion;

    // Get channel counts
    err = ASIOGetChannels(&info->numInputChannels, &info->numOutputChannels);
    if (err != ASE_OK) {
        // Even if we can't get channels, device is still attached
        info->numInputChannels = 0;
        info->numOutputChannels = 0;
    }

    // Clean up
    ASIOExit();
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

    printf("ASIO Device List\n");
    printf("================================================================================\n\n");

    // Get list of ASIO drivers
    AsioDrivers asioDrivers;
    char* driverNames[32];
    char driverNameBuffer[32][256];

    for (int i = 0; i < 32; i++) {
        driverNames[i] = driverNameBuffer[i];
    }

    long numDrivers = asioDrivers.getDriverNames(driverNames, 32);

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
