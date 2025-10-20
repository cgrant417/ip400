/*---------------------------------------------------------------------------
        Project:          ip400tun

        File Name:        callsign.c

        Author:           Claude Code

        Creation Date:    Oct 19, 2025

        Description:      Callsign encoding/decoding for IP400 frames
                          Simple implementation for IP encapsulation

                          This program is free software: you can redistribute it and/or modify
                          it under the terms of the GNU General Public License as published by
                          the Free Software Foundation, either version 2 of the License, or
                          (at your option) any later version, provided this copyright notice
                          is included.

                          Copyright (c) 2024-25 Alberta Digital Radio Communications Society

---------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "types.h"
#include "frame.h"

/*
 * Encode a callsign into compressed format
 * Uses simple 6-character packing into 4 bytes
 * This is a simplified version - real implementation uses excess-40 encoding
 */
uint8_t callEncode(char *callsign, uint16_t port, IP400_FRAME *frame, uint8_t dest, uint8_t offset)
{
    char padded[MAX_CALL+1];
    int i;
    IP400_CALL *target;

    // Determine target (source or dest)
    target = (dest == DEST_CALLSIGN) ? &frame->dest : &frame->source;

    // Pad callsign to 6 characters
    memset(padded, ' ', MAX_CALL);
    padded[MAX_CALL] = '\0';

    // Copy callsign (uppercase)
    for(i = 0; i < MAX_CALL && callsign[i] != '\0'; i++) {
        padded[i] = toupper(callsign[i]);
    }

    // Simple encoding: pack 6 chars into 4 bytes using base-40 encoding
    // Characters: space, 0-9, A-Z (40 characters total)
    uint32_t encoded = 0;

    for(i = 0; i < MAX_CALL; i++) {
        char c = padded[i];
        int val;

        if(c == ' ') val = 0;
        else if(c >= '0' && c <= '9') val = 1 + (c - '0');
        else if(c >= 'A' && c <= 'Z') val = 11 + (c - 'A');
        else val = 0;  // Invalid char becomes space

        encoded = encoded * 40 + val;
    }

    // Store in big-endian format
    target->callbytes.bytes[0] = (encoded >> 24) & 0xFF;
    target->callbytes.bytes[1] = (encoded >> 16) & 0xFF;
    target->callbytes.bytes[2] = (encoded >> 8) & 0xFF;
    target->callbytes.bytes[3] = encoded & 0xFF;

    // Store port
    target->port = port;

    return N_CALL;
}

/*
 * Decode a compressed callsign
 */
BOOL callDecode(IP400_CALL *encCall, char *callsign, uint16_t *port)
{
    uint32_t encoded;
    char decoded[MAX_CALL+1];
    int i;

    // Extract encoded value (big-endian)
    encoded = ((uint32_t)encCall->callbytes.bytes[0] << 24) |
              ((uint32_t)encCall->callbytes.bytes[1] << 16) |
              ((uint32_t)encCall->callbytes.bytes[2] << 8) |
              ((uint32_t)encCall->callbytes.bytes[3]);

    // Decode from base-40
    for(i = MAX_CALL - 1; i >= 0; i--) {
        int val = encoded % 40;
        char c;

        if(val == 0) c = ' ';
        else if(val >= 1 && val <= 10) c = '0' + (val - 1);
        else if(val >= 11 && val <= 36) c = 'A' + (val - 11);
        else c = ' ';

        decoded[i] = c;
        encoded /= 40;
    }
    decoded[MAX_CALL] = '\0';

    // Copy to output, trimming trailing spaces
    strcpy(callsign, decoded);
    for(i = strlen(callsign) - 1; i >= 0 && callsign[i] == ' '; i--) {
        callsign[i] = '\0';
    }

    // Extract port
    *port = encCall->port;

    return TRUE;
}
