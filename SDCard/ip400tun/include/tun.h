/*---------------------------------------------------------------------------
        Project:          ip400tun

        File Name:        tun.h

        Author:           Claude Code

        Creation Date:    Oct 19, 2025

        Description:      TUN/TAP interface definitions for IP-to-IP400 bridge

                          This program is free software: you can redistribute it and/or modify
                          it under the terms of the GNU General Public License as published by
                          the Free Software Foundation, either version 2 of the License, or
                          (at your option) any later version, provided this copyright notice
                          is included.

                          Copyright (c) 2024-25 Alberta Digital Radio Communications Society

---------------------------------------------------------------------------*/
#ifndef TUN_H_
#define TUN_H_

#include <stdint.h>
#include "types.h"

// TUN device configuration
#define TUN_DEVICE_NAME     "ip400"         // Will create ip400-tun0
#define TUN_MTU             1500            // Standard Ethernet MTU
#define TUN_MAX_PACKET      2048            // Maximum packet buffer

// TUN device structure
typedef struct tun_config_t {
    int         fd;                         // TUN device file descriptor
    char        dev_name[64];               // Device name
    uint16_t    mtu;                        // MTU size
    uint8_t     debug;                      // Debug flag
} TUN_CONFIG;

// TUN functions
int tun_create(char *dev_name, int flags);
int tun_setup(TUN_CONFIG *config, char *dev_name, uint16_t mtu, uint8_t debug);
int tun_close(TUN_CONFIG *config);
int tun_read(TUN_CONFIG *config, uint8_t *buffer, int max_len);
int tun_write(TUN_CONFIG *config, uint8_t *buffer, int len);

// TUN task
BOOL tunTaskInit(char *dev_name, uint16_t mtu, uint8_t debug);
void tunTask(void);

#endif /* TUN_H_ */
