/*---------------------------------------------------------------------------
        Project:          ip400tun

        File Name:        ip400bridge.c

        Author:           Claude Code

        Creation Date:    Oct 19, 2025

        Description:      Bridge between IP packets and IP400 frames
                          Handles encapsulation, routing table, and address mapping

                          This program is free software: you can redistribute it and/or modify
                          it under the terms of the GNU General Public License as published by
                          the Free Software Foundation, either version 2 of the License, or
                          (at your option) any later version, provided this copyright notice
                          is included.

                          Copyright (c) 2024-25 Alberta Digital Radio Communications Society

---------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/ip.h>

#include "types.h"
#include "ip400bridge.h"
#include "spidefs.h"
#include "frame.h"
#include "logger.h"

// Global bridge configuration
static BRIDGE_CONFIG bridge_config;

// Simple routing table entry
typedef struct route_entry_t {
    uint32_t    dest_ip;                    // Destination IP (network byte order)
    uint32_t    netmask;                    // Netmask (network byte order)
    char        dest_call[MAX_CALL+1];      // Destination callsign
    uint16_t    dest_port;                  // Destination port
    struct route_entry_t *next;             // Next entry
} ROUTE_ENTRY;

static ROUTE_ENTRY *route_table = NULL;

// External callsign encoding functions (from ip400spi)
extern uint8_t callEncode(char *callsign, uint16_t port, IP400_FRAME *frame, uint8_t dest, uint8_t offset);
extern BOOL callDecode(IP400_CALL *encCall, char *callsign, uint16_t *port);

/*
 * Initialize the bridge
 */
BOOL bridgeInit(BRIDGE_CONFIG *config)
{
    memcpy(&bridge_config, config, sizeof(BRIDGE_CONFIG));

    logger(LOG_NOTICE, "Bridge initialized: MY_CALL=%s, MY_PORT=%d\n",
           bridge_config.my_callsign, bridge_config.my_port);

    if(bridge_config.gateway_call[0]) {
        logger(LOG_NOTICE, "Default gateway: %s:%d\n",
               bridge_config.gateway_call, bridge_config.gateway_port);

        // Add default route (0.0.0.0/0)
        add_static_route(0, 0, bridge_config.gateway_call, bridge_config.gateway_port);
    }

    return TRUE;
}

/*
 * Add a static route to the routing table
 */
BOOL add_static_route(uint32_t dest_ip, uint32_t netmask, char *dest_call, uint16_t dest_port)
{
    ROUTE_ENTRY *entry;

    entry = malloc(sizeof(ROUTE_ENTRY));
    if(!entry) {
        logger(LOG_ERROR, "Failed to allocate route entry\n");
        return FALSE;
    }

    entry->dest_ip = dest_ip;
    entry->netmask = netmask;
    strncpy(entry->dest_call, dest_call, MAX_CALL);
    entry->dest_call[MAX_CALL] = '\0';
    entry->dest_port = dest_port;
    entry->next = route_table;
    route_table = entry;

    struct in_addr ip_addr, mask_addr;
    ip_addr.s_addr = dest_ip;
    mask_addr.s_addr = netmask;

    logger(LOG_NOTICE, "Added route: %s/%s -> %s:%d\n",
           inet_ntoa(ip_addr), inet_ntoa(mask_addr),
           dest_call, dest_port);

    return TRUE;
}

/*
 * Lookup route for destination IP address
 * Returns callsign and port for next hop
 */
BOOL get_route_for_ip(uint32_t dest_ip, char *dest_call, uint16_t *dest_port)
{
    ROUTE_ENTRY *entry;
    ROUTE_ENTRY *best_match = NULL;
    uint32_t best_prefix_len = 0;

    // Find longest prefix match
    for(entry = route_table; entry != NULL; entry = entry->next) {
        if((dest_ip & entry->netmask) == (entry->dest_ip & entry->netmask)) {
            // Count bits in netmask
            uint32_t prefix_len = __builtin_popcount(ntohl(entry->netmask));

            if(prefix_len >= best_prefix_len) {
                best_prefix_len = prefix_len;
                best_match = entry;
            }
        }
    }

    if(best_match) {
        strcpy(dest_call, best_match->dest_call);
        *dest_port = best_match->dest_port;
        return TRUE;
    }

    // No route found
    struct in_addr ip_addr;
    ip_addr.s_addr = dest_ip;
    logger(LOG_DEBUG, "No route to %s\n", inet_ntoa(ip_addr));

    return FALSE;
}

/*
 * Convert IP packet to IP400 SPI frame
 */
BOOL ip_to_ip400(uint8_t *ip_packet, uint16_t ip_len, SPI_BUFFER *spi_frame)
{
    struct ip *ip_hdr = (struct ip *)ip_packet;
    char dest_call[MAX_CALL+1];
    uint16_t dest_port;

    // Check minimum IP header size
    if(ip_len < sizeof(struct ip)) {
        logger(LOG_ERROR, "IP packet too small: %d bytes\n", ip_len);
        return FALSE;
    }

    // Check if packet fits in IP400 frame
    if(ip_len > PAYLOAD_MAX) {
        logger(LOG_ERROR, "IP packet too large: %d bytes (max %d)\n", ip_len, PAYLOAD_MAX);
        return FALSE;
    }

    // Lookup route for destination IP
    if(!get_route_for_ip(ip_hdr->ip_dst.s_addr, dest_call, &dest_port)) {
        // Use gateway if configured
        if(bridge_config.gateway_call[0]) {
            strcpy(dest_call, bridge_config.gateway_call);
            dest_port = bridge_config.gateway_port;
        } else {
            logger(LOG_ERROR, "No route and no gateway configured\n");
            return FALSE;
        }
    }

    // Build SPI header
    memset(spi_frame, 0, sizeof(SPI_BUFFER));

    // Set eyecatcher
    spi_frame->spiData.hdr.eye[0] = 'I';
    spi_frame->spiData.hdr.eye[1] = 'P';
    spi_frame->spiData.hdr.eye[2] = '4';
    spi_frame->spiData.hdr.eye[3] = 'C';

    // Set status
    spi_frame->spiData.hdr.status = SINGLE_FRAME;

    // Set length and offset
    spi_frame->spiData.hdr.length_hi = (ip_len >> 8) & 0xFF;
    spi_frame->spiData.hdr.length_lo = ip_len & 0xFF;
    spi_frame->spiData.hdr.offset_hi = 0;
    spi_frame->spiData.hdr.offset_lo = 0;

    // Encode source callsign (my callsign)
    IP400_FRAME temp_frame;
    callEncode(bridge_config.my_callsign, bridge_config.my_port, &temp_frame, SRC_CALLSIGN, 0);
    memcpy(spi_frame->spiData.hdr.fromCall, temp_frame.source.callbytes.bytes, N_CALL);
    spi_frame->spiData.hdr.fromPort[0] = (bridge_config.my_port >> 8) & 0xFF;
    spi_frame->spiData.hdr.fromPort[1] = bridge_config.my_port & 0xFF;

    // Encode destination callsign
    callEncode(dest_call, dest_port, &temp_frame, DEST_CALLSIGN, 0);
    memcpy(spi_frame->spiData.hdr.toCall, temp_frame.dest.callbytes.bytes, N_CALL);
    spi_frame->spiData.hdr.toPort[0] = (dest_port >> 8) & 0xFF;
    spi_frame->spiData.hdr.toPort[1] = dest_port & 0xFF;

    // Set packet type
    spi_frame->spiData.hdr.coding = IP_ENCAPSULATED;

    // Set hop count and flags
    spi_frame->spiData.hdr.hopCount = 0;
    spi_frame->spiData.hdr.flags = 0;

    // Copy IP packet data
    memcpy(spi_frame->spiData.buffer, ip_packet, ip_len);

    if(bridge_config.debug & DEBUG_BRIDGE) {
        struct in_addr src_ip, dst_ip;
        src_ip.s_addr = ip_hdr->ip_src.s_addr;
        dst_ip.s_addr = ip_hdr->ip_dst.s_addr;

        logger(LOG_DEBUG, "Encapsulated: %s -> %s (%d bytes) to %s:%d\n",
               inet_ntoa(src_ip), inet_ntoa(dst_ip), ip_len,
               dest_call, dest_port);
    }

    return TRUE;
}

/*
 * Convert IP400 SPI frame to IP packet
 */
BOOL ip400_to_ip(SPI_BUFFER *spi_frame, uint8_t *ip_packet, uint16_t *ip_len)
{
    uint16_t payload_len;
    char src_call[MAX_CALL+1];
    uint16_t src_port;

    // Check if this is an IP encapsulated packet
    if(spi_frame->spiData.hdr.coding != IP_ENCAPSULATED) {
        return FALSE;
    }

    // Get payload length
    payload_len = (spi_frame->spiData.hdr.length_hi << 8) | spi_frame->spiData.hdr.length_lo;

    // Validate length
    if(payload_len > PAYLOAD_MAX || payload_len < sizeof(struct ip)) {
        logger(LOG_ERROR, "Invalid IP packet length: %d\n", payload_len);
        return FALSE;
    }

    // Extract IP packet
    memcpy(ip_packet, spi_frame->spiData.buffer, payload_len);
    *ip_len = payload_len;

    // Decode source callsign for logging
    IP400_CALL temp_call;
    memcpy(temp_call.callbytes.bytes, spi_frame->spiData.hdr.fromCall, N_CALL);
    temp_call.port = (spi_frame->spiData.hdr.fromPort[0] << 8) | spi_frame->spiData.hdr.fromPort[1];
    callDecode(&temp_call, src_call, &src_port);

    if(bridge_config.debug & DEBUG_BRIDGE) {
        struct ip *ip_hdr = (struct ip *)ip_packet;
        struct in_addr src_ip, dst_ip;
        src_ip.s_addr = ip_hdr->ip_src.s_addr;
        dst_ip.s_addr = ip_hdr->ip_dst.s_addr;

        logger(LOG_DEBUG, "Extracted: %s -> %s (%d bytes) from %s:%d\n",
               inet_ntoa(src_ip), inet_ntoa(dst_ip), payload_len,
               src_call, src_port);
    }

    return TRUE;
}
