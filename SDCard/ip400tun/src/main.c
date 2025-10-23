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
    bridge_config.gateway_vpn_filter = 0;  // 0 = send to all matching callsigns

    // Parse command line parameters
    while ((c = getopt(argc, argv, "s:t:c:g:v:i:m:d:h")) != -1) {

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
                {
                    // Pad to MAX_CALL chars to match decoded beacon callsigns
                    int len = strlen(optarg);
                    if(len > MAX_CALL) len = MAX_CALL;
                    memset(bridge_config.my_callsign, ' ', MAX_CALL);
                    memcpy(bridge_config.my_callsign, optarg, len);
                    bridge_config.my_callsign[MAX_CALL] = '\0';
                }
                break;

            // Gateway callsign (REQUIRED)
            case 'g':
                {
                    // Pad to MAX_CALL chars to match decoded beacon callsigns
                    int len = strlen(optarg);
                    if(len > MAX_CALL) len = MAX_CALL;
                    memset(bridge_config.gateway_call, ' ', MAX_CALL);
                    memcpy(bridge_config.gateway_call, optarg, len);
                    bridge_config.gateway_call[MAX_CALL] = '\0';
                }
                break;

            // Gateway VPN filter (optional)
            case 'v':
                sscanf(optarg, "%hx", &bridge_config.gateway_vpn_filter);
                break;

            // Tunnel IP address (REQUIRED)
            case 'i':
                strncpy(bridge_config.tunnel_ip, optarg, sizeof(bridge_config.tunnel_ip)-1);
                bridge_config.tunnel_ip[sizeof(bridge_config.tunnel_ip)-1] = '\0';
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

    // Validate required parameters
    if(!bridge_config.my_callsign[0]) {
        fprintf(stderr, "Error: My callsign (-c) is required\n");
        show_help(argv[0]);
        exit(100);
    }

    if(!bridge_config.gateway_call[0]) {
        fprintf(stderr, "Error: Gateway callsign (-g) is required\n");
        show_help(argv[0]);
        exit(100);
    }

    if(!bridge_config.tunnel_ip[0]) {
        fprintf(stderr, "Error: Tunnel IP address (-i) is required\n");
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
            "Usage: %s -c CALLSIGN -g GATEWAY -i TUNNEL_IP [options]\n"
            "\n"
            "IP400 TUN Bridge - Point-to-point tunnel over IP400 mesh network\n"
            "\n"
            "Required:\n"
            "  -c CALLSIGN   Your amateur radio callsign\n"
            "  -g CALLSIGN   Gateway callsign (remote tunnel endpoint)\n"
            "  -i IP/MASK    Tunnel IP address (e.g., 10.0.0.1/24)\n"
            "\n"
            "Optional:\n"
            "  -v VPN        Gateway VPN filter (hex, e.g., 0xC6AE)\n"
            "                If specified, only send to this specific VPN\n"
            "                If omitted, send to all gateways with matching callsign\n"
            "  -s DEVICE     SPI device (default: /dev/spidev0.0)\n"
            "  -t NAME       TUN device name (default: ip400)\n"
            "  -m MTU        MTU size (default: 1500)\n"
            "  -d FLAGS      Debug flags (hex):\n"
            "                  0x01 - Enable logging\n"
            "                  0x02 - SPI debug\n"
            "                  0x04 - TUN debug\n"
            "                  0x08 - Bridge debug\n"
            "  -h            Show this help message\n"
            "\n"
            "Examples:\n"
            "\n"
            "  # Node 1 (tunnel endpoint A):\n"
            "  sudo %s -c VE3CA -g VE3AC -i 10.0.0.1/24 -d 0x01\n"
            "  sudo ip link set ip400 up\n"
            "\n"
            "  # Node 2 (tunnel endpoint B):\n"
            "  sudo %s -c VE3AC -g VE3CA -i 10.0.0.2/24 -d 0x01\n"
            "  sudo ip link set ip400 up\n"
            "\n"
            "  # With VPN filter (if multiple VE3CA nodes exist):\n"
            "  sudo %s -c VE3AC -g VE3CA -v 0xC6AE -i 10.0.0.2/24\n"
            "\n"
            "After both endpoints are running, test with:\n"
            "  ping 10.0.0.1\n"
            "\n",
            name, name, name, name);
}
