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

// Beacon table (linked list)
static BEACON_ENTRY *beacon_table = NULL;

// Uptime counter for beacon aging
static uint32_t uptime_seconds = 0;

// External callsign encoding functions (from ip400spi)
extern uint8_t callEncode(char *callsign, uint16_t port, IP400_FRAME *frame, uint8_t dest, uint8_t offset);
extern BOOL callDecode(IP400_CALL *encCall, char *callsign, uint16_t *port);

/*
 * Initialize the bridge
 */
BOOL bridgeInit(BRIDGE_CONFIG *config)
{
    memcpy(&bridge_config, config, sizeof(BRIDGE_CONFIG));

    logger(LOG_NOTICE, "Tunnel initialized: MY_CALL=%s\n", bridge_config.my_callsign);
    logger(LOG_NOTICE, "Gateway: %s%s\n",
           bridge_config.gateway_call,
           bridge_config.gateway_vpn_filter ? " (VPN filtered)" : " (all VPNs)");
    logger(LOG_NOTICE, "Tunnel IP: %s\n", bridge_config.tunnel_ip);
    logger(LOG_NOTICE, "Waiting for beacons to populate table...\n");

    uptime_seconds = 0;
    return TRUE;
}

/*
 * Add or update beacon table entry
 */
void add_beacon_entry(char *callsign, uint16_t vpn, int16_t rssi)
{
    BEACON_ENTRY *entry;

    // Search for existing entry with same callsign and VPN
    // Use strncmp(MAX_CALL) to match STM32 firmware callsign comparison behavior
    for(entry = beacon_table; entry != NULL; entry = entry->next) {
        if(strncmp(entry->callsign, callsign, MAX_CALL) == 0 && entry->vpn == vpn) {
            // Update existing entry
            entry->last_seen = uptime_seconds;
            entry->rssi = rssi;
            if(bridge_config.debug & DEBUG_BRIDGE) {
                logger(LOG_DEBUG, "Updated beacon: %s VPN=0x%04X RSSI=%d\n",
                       callsign, vpn, rssi);
            }
            return;
        }
    }

    // Create new entry
    entry = malloc(sizeof(BEACON_ENTRY));
    if(!entry) {
        logger(LOG_ERROR, "Failed to allocate beacon entry\n");
        return;
    }

    strncpy(entry->callsign, callsign, MAX_CALL);
    entry->callsign[MAX_CALL] = '\0';
    entry->vpn = vpn;
    entry->last_seen = uptime_seconds;
    entry->rssi = rssi;
    entry->next = beacon_table;
    beacon_table = entry;

    logger(LOG_NOTICE, "New beacon: %s VPN=0x%04X RSSI=%d\n",
           callsign, vpn, rssi);
}

/*
 * Find beacon entries matching callsign and optional VPN filter
 * Returns first match (caller should iterate via ->next to find all)
 */
BEACON_ENTRY *find_beacon_entries(char *callsign, uint16_t vpn_filter)
{
    BEACON_ENTRY *entry;
    BEACON_ENTRY *first_match = NULL;

    for(entry = beacon_table; entry != NULL; entry = entry->next) {
        // Check callsign match (use strncmp to match STM32 firmware behavior)
        if(strncmp(entry->callsign, callsign, MAX_CALL) != 0) {
            continue;
        }

        // If VPN filter specified, check VPN too
        if(vpn_filter != 0 && entry->vpn != vpn_filter) {
            continue;
        }

        // Match found
        if(first_match == NULL) {
            first_match = entry;
        }
    }

    return first_match;
}

/*
 * Count beacon entries matching callsign and optional VPN filter
 */
int count_beacon_entries(char *callsign, uint16_t vpn_filter)
{
    BEACON_ENTRY *entry;
    int count = 0;

    for(entry = beacon_table; entry != NULL; entry = entry->next) {
        // Use strncmp to match STM32 firmware callsign comparison behavior
        if(strncmp(entry->callsign, callsign, MAX_CALL) == 0) {
            if(vpn_filter == 0 || entry->vpn == vpn_filter) {
                count++;
            }
        }
    }

    return count;
}

/*
 * Process received beacon packet
 */
void process_beacon(SPI_BUFFER *spi_frame)
{
    IP400_CALL src_call;
    char callsign[MAX_CALL+1];
    uint16_t vpn;

    // Decode source callsign and VPN from beacon
    // Copy encoded callsign bytes
    memcpy(src_call.callbytes.bytes, spi_frame->spiData.hdr.fromCall, N_CALL);
    // Copy VPN from fromPort field (2 bytes, little-endian - STM32 native byte order)
    src_call.port = (spi_frame->spiData.hdr.fromPort[1] << 8) | spi_frame->spiData.hdr.fromPort[0];
    // Decode to get callsign string and VPN value
    callDecode(&src_call, callsign, &vpn);

    // Add to beacon table (RSSI would need to be extracted from payload if available)
    add_beacon_entry(callsign, vpn, 0);

    // Check if this is our own beacon (auto-detect VPN)
    // Use strncmp to match STM32 firmware callsign comparison behavior
    if(!bridge_config.vpn_detected && strncmp(callsign, bridge_config.my_callsign, MAX_CALL) == 0) {
        bridge_config.my_vpn = vpn;
        bridge_config.vpn_detected = TRUE;
        logger(LOG_NOTICE, "Auto-detected my VPN address: 0x%04X\n", vpn);
    }
}

/*
 * Convert IP packet to IP400 SPI frame and send to gateway(s)
 * This function builds frame(s) with destination = gateway callsign from beacon table
 * Returns number of frames created (0 = error/no gateway found)
 */
int ip_to_ip400(uint8_t *ip_packet, uint16_t ip_len, SPI_BUFFER *spi_frame)
{
    struct ip *ip_hdr = (struct ip *)ip_packet;
    BEACON_ENTRY *entry;
    int frame_count = 0;

    // Check minimum IP header size
    if(ip_len < sizeof(struct ip)) {
        logger(LOG_ERROR, "IP packet too small: %d bytes\n", ip_len);
        return 0;
    }

    // Check if packet fits in IP400 frame
    if(ip_len > PAYLOAD_MAX) {
        logger(LOG_ERROR, "IP packet too large: %d bytes (max %d)\n", ip_len, PAYLOAD_MAX);
        return 0;
    }

    // Determine destination VPN based on filter setting
    uint16_t dest_vpn;

    if(bridge_config.gateway_vpn_filter != 0) {
        // VPN filter specified: send to specific VPN only
        // Verify gateway with this VPN exists in beacon table
        entry = find_beacon_entries(bridge_config.gateway_call, bridge_config.gateway_vpn_filter);
        if(!entry) {
            if(bridge_config.debug & DEBUG_BRIDGE) {
                logger(LOG_DEBUG, "Gateway %s VPN=0x%04X not found in beacon table (packet dropped)\n",
                       bridge_config.gateway_call, bridge_config.gateway_vpn_filter);
            }
            return 0;
        }
        dest_vpn = bridge_config.gateway_vpn_filter;
    } else {
        // No VPN filter: broadcast to ALL nodes with gateway callsign
        // Check if ANY gateway exists in beacon table
        int gateway_count = count_beacon_entries(bridge_config.gateway_call, 0);
        if(gateway_count == 0) {
            if(bridge_config.debug & DEBUG_BRIDGE) {
                logger(LOG_DEBUG, "Gateway %s not found in beacon table (packet dropped)\n",
                       bridge_config.gateway_call);
                // Debug: show what we're comparing
                logger(LOG_DEBUG, "  Looking for: '%s' (len=%d)\n",
                       bridge_config.gateway_call, (int)strlen(bridge_config.gateway_call));
                // Debug: dump beacon table
                BEACON_ENTRY *debug_entry;
                int i = 0;
                for(debug_entry = beacon_table; debug_entry != NULL; debug_entry = debug_entry->next) {
                    logger(LOG_DEBUG, "  Beacon[%d]: '%s' (len=%d) VPN=0x%04X\n",
                           i++, debug_entry->callsign, (int)strlen(debug_entry->callsign), debug_entry->vpn);
                }
            }
            return 0;
        }
        // Use broadcast VPN so all nodes with matching callsign accept
        dest_vpn = 0xFFFF;  // IP_BROADCAST
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

    // Encode source callsign (my callsign) with my VPN
    uint16_t source_vpn = bridge_config.vpn_detected ? bridge_config.my_vpn : 0;
    IP400_FRAME temp_frame;
    callEncode(bridge_config.my_callsign, source_vpn, &temp_frame, SRC_CALLSIGN, 0);
    memcpy(spi_frame->spiData.hdr.fromCall, temp_frame.source.callbytes.bytes, N_CALL);
    // Write VPN in little-endian format (STM32 native byte order)
    spi_frame->spiData.hdr.fromPort[0] = source_vpn & 0xFF;         // Low byte
    spi_frame->spiData.hdr.fromPort[1] = (source_vpn >> 8) & 0xFF;  // High byte

    // Encode destination callsign with determined VPN
    // (either specific VPN from filter, or 0xFFFF for broadcast to all matching callsigns)
    callEncode(bridge_config.gateway_call, dest_vpn, &temp_frame, DEST_CALLSIGN, 0);
    memcpy(spi_frame->spiData.hdr.toCall, temp_frame.dest.callbytes.bytes, N_CALL);
    // Write VPN in little-endian format (STM32 native byte order)
    spi_frame->spiData.hdr.toPort[0] = dest_vpn & 0xFF;         // Low byte
    spi_frame->spiData.hdr.toPort[1] = (dest_vpn >> 8) & 0xFF;  // High byte

    // Set packet type
    spi_frame->spiData.hdr.coding = IP_ENCAPSULATED;

    // Set hop count and flags
    spi_frame->spiData.hdr.hopCount = 0;
    spi_frame->spiData.hdr.flags = 0;

    // Copy IP packet data
    memcpy(spi_frame->spiData.buffer, ip_packet, ip_len);

    frame_count = 1;

    if(bridge_config.debug & DEBUG_BRIDGE) {
        struct in_addr src_ip, dst_ip;
        char src_str[INET_ADDRSTRLEN], dst_str[INET_ADDRSTRLEN];

        src_ip.s_addr = ip_hdr->ip_src.s_addr;
        dst_ip.s_addr = ip_hdr->ip_dst.s_addr;

        strncpy(src_str, inet_ntoa(src_ip), INET_ADDRSTRLEN);
        strncpy(dst_str, inet_ntoa(dst_ip), INET_ADDRSTRLEN);

        logger(LOG_DEBUG, "Encapsulated: %s -> %s (%d bytes) to %s VPN=0x%04X%s\n",
               src_str, dst_str, ip_len,
               bridge_config.gateway_call, dest_vpn,
               (dest_vpn == 0xFFFF) ? " (BROADCAST)" : "");
    }

    return frame_count;
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
    temp_call.port = (spi_frame->spiData.hdr.fromPort[1] << 8) | spi_frame->spiData.hdr.fromPort[0];
    callDecode(&temp_call, src_call, &src_port);

    if(bridge_config.debug & DEBUG_BRIDGE) {
        struct ip *ip_hdr = (struct ip *)ip_packet;
        struct in_addr src_ip, dst_ip;
        char src_str[INET_ADDRSTRLEN], dst_str[INET_ADDRSTRLEN];

        src_ip.s_addr = ip_hdr->ip_src.s_addr;
        dst_ip.s_addr = ip_hdr->ip_dst.s_addr;

        // Must copy strings since inet_ntoa uses static buffer
        strncpy(src_str, inet_ntoa(src_ip), INET_ADDRSTRLEN);
        strncpy(dst_str, inet_ntoa(dst_ip), INET_ADDRSTRLEN);

        logger(LOG_DEBUG, "Extracted: %s -> %s (%d bytes) from %s:%d\n",
               src_str, dst_str, payload_len,
               src_call, src_port);
    }

    return TRUE;
}
