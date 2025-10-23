/*---------------------------------------------------------------------------
        Project:          ip400tun

        File Name:        ip400bridge.h

        Author:           Claude Code

        Creation Date:    Oct 19, 2025

        Description:      Bridge between TUN device and IP400 SPI interface

                          This program is free software: you can redistribute it and/or modify
                          it under the terms of the GNU General Public License as published by
                          the Free Software Foundation, either version 2 of the License, or
                          (at your option) any later version, provided this copyright notice
                          is included.

                          Copyright (c) 2024-25 Alberta Digital Radio Communications Society

---------------------------------------------------------------------------*/
#ifndef IP400BRIDGE_H_
#define IP400BRIDGE_H_

#include <stdint.h>
#include "types.h"
#include "spidefs.h"
#include "frame.h"

// Beacon table entry
typedef struct beacon_entry_t {
    char        callsign[MAX_CALL+1];       // Station callsign
    uint16_t    vpn;                        // VPN address (last 2 bytes of IP)
    uint32_t    last_seen;                  // Timestamp (seconds since start)
    int16_t     rssi;                       // Signal strength
    struct beacon_entry_t *next;            // Next entry in list
} BEACON_ENTRY;

// Bridge configuration
typedef struct bridge_config_t {
    char        my_callsign[MAX_CALL+1];    // My amateur radio callsign
    uint16_t    my_vpn;                     // My VPN address (auto-detected from STM32)
    BOOL        vpn_detected;               // TRUE when VPN has been detected
    char        gateway_call[MAX_CALL+1];   // Gateway callsign (REQUIRED)
    uint16_t    gateway_vpn_filter;         // Gateway VPN filter (0 = send to all)
    char        tunnel_ip[20];              // Tunnel IP address (e.g., "10.0.0.1/24")
    uint8_t     debug;                      // Debug flags
} BRIDGE_CONFIG;

// Bridge initialization
BOOL bridgeInit(BRIDGE_CONFIG *config);

// IP packet to IP400 frame conversion (returns number of frames created, 0=error)
int ip_to_ip400(uint8_t *ip_packet, uint16_t ip_len, SPI_BUFFER *spi_frame);

// IP400 frame to IP packet conversion
BOOL ip400_to_ip(SPI_BUFFER *spi_frame, uint8_t *ip_packet, uint16_t *ip_len);

// Beacon table management
void add_beacon_entry(char *callsign, uint16_t vpn, int16_t rssi);
BEACON_ENTRY *find_beacon_entries(char *callsign, uint16_t vpn_filter);
int count_beacon_entries(char *callsign, uint16_t vpn_filter);
void process_beacon(SPI_BUFFER *spi_frame);

#endif /* IP400BRIDGE_H_ */
