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

// Radix 40 callsign alphabet (matching STM32 firmware)
static char alphabet[40] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',  // 0-9
    ' ', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I',  // 10-19
    'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S',  // 20-29
    'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '(', ')', '-'   // 30-39
};

// Encode a character into the alphabet (matching STM32 firmware)
static uint32_t alphaEncode(char byte)
{
    int i;

    // Convert to uppercase
    if(byte >= 'a' && byte <= 'z') {
        byte = byte - 32;
    }

    // Find in alphabet
    for(i = 0; i < 40; i++) {
        if(alphabet[i] == byte) {
            return i;
        }
    }
    return 10;  // Default to space
}

// Decode a value back to ASCII (matching STM32 firmware)
static char alphaDecode(uint32_t alpha)
{
    // Numeric 0-9
    if(alpha <= 9) {
        return '0' + alpha;
    }

    // Special cases
    switch(alpha) {
        case 10: return ' ';
        case 37: return '(';
        case 38: return ')';
        case 39: return '@';
        default: return 'A' + (alpha - 11);
    }
}

/*
 * Encode a callsign into compressed format
 * Uses base-40 encoding matching STM32 firmware
 */
uint8_t callEncode(char *callsign, uint16_t port, IP400_FRAME *frame, uint8_t dest, uint8_t offset)
{
    char padded[MAX_CALL+1];
    int i;
    uint32_t encoded;
    IP400_CALL *target;

    // Determine target (source or dest)
    target = (dest == DEST_CALLSIGN) ? &frame->dest : &frame->source;

    // Pad callsign to 6 characters with spaces
    memset(padded, ' ', MAX_CALL);
    padded[MAX_CALL] = '\0';

    // Copy callsign (will be uppercased by alphaEncode)
    for(i = 0; i < MAX_CALL && callsign[i] != '\0'; i++) {
        padded[i] = callsign[i];
    }

    // Encode using base-40 (matching STM32 firmware algorithm)
    encoded = alphaEncode(padded[0]);
    for(i = 1; i < MAX_CALL; i++) {
        uint32_t current = alphaEncode(padded[i]);
        encoded = current + encoded * 40;
    }

    // Store encoded value (native byte order, will be handled by frame structure)
    target->callbytes.encoded = encoded;

    // Store port
    target->port = port;

    return N_CALL;
}

/*
 * Decode a compressed callsign (matching STM32 firmware)
 */
BOOL callDecode(IP400_CALL *encCall, char *callsign, uint16_t *port)
{
    char tmpBuf[MAX_CALL+1];
    char *p = tmpBuf;
    uint32_t encoded;
    int i;

    // Get encoded value from structure
    encoded = encCall->callbytes.encoded;

    // Decode from base-40 (matching STM32 firmware algorithm)
    for(i = 0; i < MAX_CALL; i++) {
        *p++ = alphaDecode(encoded % 40);
        encoded /= 40;
    }
    *p = '\0';

    // Reverse the string (STM32 firmware reverses during decode)
    for(i = strlen(tmpBuf) - 1; i >= 0; i--) {
        *callsign++ = tmpBuf[i];
    }
    *callsign = '\0';

    // Extract port
    if(port != NULL) {
        *port = encCall->port;
    }

    return TRUE;
}
