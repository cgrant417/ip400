/*---------------------------------------------------------------------------
        Project:          ip400tun

        File Name:        main.c

        Author:           Claude Code

        Creation Date:    Oct 19, 2025

        Description:      Main program for IP400 TUN bridge daemon
                          Creates TUN interface for IP networking over IP400 mesh

                          This program is free software: you can redistribute it and/or modify
                          it under the terms of the GNU General Public License as published by
                          the Free Software Foundation, either version 2 of the License, or
                          (at your option) any later version, provided this copyright notice
                          is included.

                          Copyright (c) 2024-25 Alberta Digital Radio Communications Society

---------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include "types.h"
#include "logger.h"
#include "spidefs.h"
#include "tun.h"
#include "ip400bridge.h"
#include "timer.h"

// Configuration
char spiDev[20];
char tunDev[64];
int debugFlag;
BRIDGE_CONFIG bridge_config;

// Forward refs
void show_help(char *name);
void combinedTask(void);

// Catch a control-c or other termination
void sighandler(int s)
{
    logger(LOG_ERROR, "\nCaught signal %d\n", s);
    stopTimer();
    exit(1);
}

int main(int argc, char *argv[])
{
    int c;
    uint16_t tun_mtu = TUN_MTU;

    // Set defaults
    strcpy(spiDev, SPI_0_DEV_0);
    strcpy(tunDev, TUN_DEVICE_NAME);
    debugFlag = FALSE;
    memset(&bridge_config, 0, sizeof(bridge_config));
    bridge_config.my_port = ENC_IP_TYPE;
    bridge_config.gateway_port = ENC_IP_TYPE;

    // Parse command line parameters
    while ((c = getopt(argc, argv, "s:t:c:g:m:d:h")) != -1) {

        switch((char)c) {

            // SPI device
            case 's':
                strcpy(spiDev, optarg);
                break;

            // TUN device name
            case 't':
                strcpy(tunDev, optarg);
                break;

            // My callsign
            case 'c':
                strncpy(bridge_config.my_callsign, optarg, MAX_CALL);
                bridge_config.my_callsign[MAX_CALL] = '\0';
                break;

            // Gateway callsign (default route)
            case 'g':
                strncpy(bridge_config.gateway_call, optarg, MAX_CALL);
                bridge_config.gateway_call[MAX_CALL] = '\0';
                break;

            // MTU size
            case 'm':
                sscanf(optarg, "%hd", &tun_mtu);
                break;

            // Debug
            case 'd':
                sscanf(optarg, "%x", &debugFlag);
                break;

            // Help
            case 'h':
                show_help(argv[0]);
                exit(0);
                break;
        }
    }

    // Validate callsign
    if(!bridge_config.my_callsign[0]) {
        fprintf(stderr, "Error: My callsign (-c) is required\n");
        show_help(argv[0]);
        exit(100);
    }

    // Set debug flags
    bridge_config.debug = debugFlag;

    // Open the log
    openLog(debugFlag & DEBUG_LOG);
    logger(LOG_DEBUG, "Debug logging enabled\n");

    // Initialize bridge (routing table, etc.)
    if(!bridgeInit(&bridge_config)) {
        logger(LOG_FATAL, "Failed to initialize bridge\n");
        exit(100);
    }

    // Setup SPI interface
    int spiDevNum = spi_lookup(spiDev);
    if(spiDevNum == -1) {
        logger(LOG_FATAL, "Cannot find SPI device name %s\n", spiDev);
        exit(100);
    }

    spi_setup(spiDevNum, SPI_MODE_0, SPI_NBITS, SPI_SPEED, debugFlag & DEBUG_SPI);

    if(!spiTaskInit(spiDevNum)) {
        logger(LOG_FATAL, "Cannot open SPI device %s\n", spiDev);
        exit(100);
    }

    // Setup TUN interface
    if(!tunTaskInit(tunDev, tun_mtu, debugFlag)) {
        logger(LOG_FATAL, "Cannot create TUN device %s\n", tunDev);
        exit(100);
    }

    logger(LOG_NOTICE, "IP400 TUN bridge started\n");
    logger(LOG_NOTICE, "  SPI device: %s\n", spiDev);
    logger(LOG_NOTICE, "  TUN device: %s\n", tunDev);
    logger(LOG_NOTICE, "  Callsign:   %s\n", bridge_config.my_callsign);
    logger(LOG_NOTICE, "  Gateway:    %s\n",
           bridge_config.gateway_call[0] ? bridge_config.gateway_call : "(none)");

    // Set up signal handler for clean exit
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = sighandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

    // Start timer that calls both SPI and TUN tasks
    if(!startTimer(TIMER_VALUE, &combinedTask)) {
        logger(LOG_FATAL, "Could not start the timer task\n");
        exit(101);
    }

    return EXIT_SUCCESS;
}

/*
 * Combined task - called every TIMER_VALUE ms
 * Runs both SPI task and TUN task
 */
void combinedTask(void)
{
    spiTask();      // Handle SPI communication with STM32
    tunTask();      // Handle TUN device (IP packets)
}

void show_help(char *name)
{
    fprintf(stderr,
            "Usage: %s -c CALLSIGN [options]\n"
            "\n"
            "Required:\n"
            "  -c CALLSIGN   Your amateur radio callsign\n"
            "\n"
            "Optional:\n"
            "  -s DEVICE     SPI device (default: /dev/spidev0.0)\n"
            "  -t NAME       TUN device name (default: ip400)\n"
            "  -g CALLSIGN   Gateway callsign for default route\n"
            "  -m MTU        MTU size (default: 1500)\n"
            "  -d FLAGS      Debug flags (hex):\n"
            "                  0x01 - Enable logging\n"
            "                  0x02 - SPI debug\n"
            "                  0x04 - TUN debug\n"
            "                  0x08 - Bridge debug\n"
            "  -h            Show this help message\n"
            "\n"
            "Example:\n"
            "  sudo %s -c VE6VH -g VE6ABC -d 0x01\n"
            "\n"
            "After starting, configure the TUN interface:\n"
            "  sudo ip addr add 172.16.1.10/24 dev ip400\n"
            "  sudo ip link set ip400 up\n"
            "\n",
            name, name);
}
