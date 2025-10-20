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

// Bridge configuration
typedef struct bridge_config_t {
    char        my_callsign[MAX_CALL+1];    // My amateur radio callsign
    uint16_t    my_port;                    // My IP400 port (default: ENC_IP_TYPE)
    char        gateway_call[MAX_CALL+1];   // Gateway callsign (for default route)
    uint16_t    gateway_port;               // Gateway port
    uint8_t     debug;                      // Debug flags
} BRIDGE_CONFIG;

// Bridge initialization
BOOL bridgeInit(BRIDGE_CONFIG *config);

// IP packet to IP400 frame conversion
BOOL ip_to_ip400(uint8_t *ip_packet, uint16_t ip_len, SPI_BUFFER *spi_frame);

// IP400 frame to IP packet conversion
BOOL ip400_to_ip(SPI_BUFFER *spi_frame, uint8_t *ip_packet, uint16_t *ip_len);

// Get destination callsign from IP address (routing table lookup)
BOOL get_route_for_ip(uint32_t dest_ip, char *dest_call, uint16_t *dest_port);

// Add static route
BOOL add_static_route(uint32_t dest_ip, uint32_t netmask, char *dest_call, uint16_t dest_port);

#endif /* IP400BRIDGE_H_ */
