/*---------------------------------------------------------------------------
        Project:          ip400tun

        File Name:        tun.c

        Author:           Claude Code

        Creation Date:    Oct 19, 2025

        Description:      TUN device creation and management

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
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>

#include "types.h"
#include "tun.h"
#include "logger.h"
#include "dataq.h"
#include "spidefs.h"
#include "ip400bridge.h"

// Global TUN configuration
static TUN_CONFIG tun_config;

// TUN RX/TX queues (reuse dataq from ip400spi)
extern FRAME_QUEUE SPITxQueue;

// External function to enqueue SPI frames
extern BOOL EnqueSPIFrame(void *spiFrame);

/*
 * Create a TUN device
 * Returns file descriptor or -1 on error
 */
int tun_create(char *dev_name, int flags)
{
    struct ifreq ifr;
    int fd, err;

    // Open the TUN/TAP clone device
    if((fd = open("/dev/net/tun", O_RDWR)) < 0) {
        logger(LOG_ERROR, "Cannot open /dev/net/tun: %m\n");
        return -1;
    }

    // Prepare the structure
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = flags;

    if(dev_name && *dev_name) {
        strncpy(ifr.ifr_name, dev_name, IFNAMSIZ);
    }

    // Create the device
    if((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0) {
        logger(LOG_ERROR, "Cannot create TUN device %s: %m\n", dev_name);
        close(fd);
        return err;
    }

    // Copy the actual device name back
    if(dev_name) {
        strcpy(dev_name, ifr.ifr_name);
    }

    logger(LOG_NOTICE, "TUN device %s created successfully\n", ifr.ifr_name);
    return fd;
}

/*
 * Setup TUN device with configuration
 */
int tun_setup(TUN_CONFIG *config, char *dev_name, uint16_t mtu, uint8_t debug)
{
    config->debug = debug;
    config->mtu = mtu;

    strncpy(config->dev_name, dev_name, sizeof(config->dev_name)-1);
    config->dev_name[sizeof(config->dev_name)-1] = '\0';

    // Create TUN device (IFF_TUN for IP layer, IFF_NO_PI for no packet info)
    config->fd = tun_create(config->dev_name, IFF_TUN | IFF_NO_PI);

    if(config->fd < 0) {
        logger(LOG_ERROR, "Failed to create TUN device\n");
        return -1;
    }

    // Set non-blocking mode
    int flags = fcntl(config->fd, F_GETFL, 0);
    if(flags >= 0) {
        fcntl(config->fd, F_SETFL, flags | O_NONBLOCK);
    }

    logger(LOG_NOTICE, "TUN device %s configured (MTU: %d)\n",
           config->dev_name, config->mtu);

    return config->fd;
}

/*
 * Close TUN device
 */
int tun_close(TUN_CONFIG *config)
{
    if(config->fd >= 0) {
        close(config->fd);
        config->fd = -1;
        logger(LOG_NOTICE, "TUN device %s closed\n", config->dev_name);
    }
    return 0;
}

/*
 * Read IP packet from TUN device
 */
int tun_read(TUN_CONFIG *config, uint8_t *buffer, int max_len)
{
    int nread;

    nread = read(config->fd, buffer, max_len);

    if(nread < 0) {
        if(errno != EAGAIN && errno != EWOULDBLOCK) {
            logger(LOG_ERROR, "Error reading from TUN device: %m\n");
        }
        return -1;
    }

    if(config->debug & DEBUG_TUN) {
        logger(LOG_DEBUG, "TUN read %d bytes\n", nread);
    }

    return nread;
}

/*
 * Write IP packet to TUN device
 */
int tun_write(TUN_CONFIG *config, uint8_t *buffer, int len)
{
    int nwrite;

    nwrite = write(config->fd, buffer, len);

    if(nwrite < 0) {
        logger(LOG_ERROR, "Error writing to TUN device: %m\n");
        return -1;
    }

    if(config->debug & DEBUG_TUN) {
        logger(LOG_DEBUG, "TUN wrote %d bytes\n", nwrite);
    }

    return nwrite;
}

/*
 * Initialize TUN task
 */
BOOL tunTaskInit(char *dev_name, uint16_t mtu, uint8_t debug)
{
    if(tun_setup(&tun_config, dev_name, mtu, debug) < 0) {
        return FALSE;
    }

    return TRUE;
}

/*
 * TUN task - called periodically
 * Reads IP packets from TUN, encapsulates in IP400 frames, sends to SPI
 */
void tunTask(void)
{
    static uint8_t ip_packet[TUN_MAX_PACKET];
    static SPI_BUFFER spi_frame;
    int nread;

    // Read IP packet from TUN device
    nread = tun_read(&tun_config, ip_packet, sizeof(ip_packet));

    if(nread > 0) {
        // Convert IP packet to IP400 SPI frame
        if(ip_to_ip400(ip_packet, nread, &spi_frame)) {
            // Enqueue for transmission to STM32
            SPI_DATA_FRAME *txFrame;

            if((txFrame = malloc(sizeof(SPI_DATA_FRAME))) != NULL) {
                uint8_t *frameBuffer;
                int frame_len = nread + sizeof(struct spi_hdr_t);

                if((frameBuffer = malloc(frame_len)) != NULL) {
                    memcpy(frameBuffer, spi_frame.rawData, frame_len);
                    txFrame->buffer = frameBuffer;
                    txFrame->length = frame_len;

                    if(!EnqueSPIFrame(txFrame)) {
                        logger(LOG_ERROR, "Failed to enqueue IP400 frame\n");
                        free(frameBuffer);
                        free(txFrame);
                    } else {
                        if(tun_config.debug & DEBUG_TUN) {
                            logger(LOG_DEBUG, "IP packet (%d bytes) encapsulated and queued\n", nread);
                        }
                    }
                } else {
                    free(txFrame);
                }
            }
        }
    }
}

/*
 * Process received IP400 frame from SPI, extract IP packet, write to TUN
 */
BOOL process_ip400_frame(SPI_BUFFER *spi_frame)
{
    static uint8_t ip_packet[TUN_MAX_PACKET];
    uint16_t ip_len;

    // Check if this is an IP encapsulated frame
    if(spi_frame->spiData.hdr.coding != IP_ENCAPSULATED) {
        return FALSE;
    }

    // Convert IP400 frame to IP packet
    if(ip400_to_ip(spi_frame, ip_packet, &ip_len)) {
        // Write to TUN device
        if(tun_write(&tun_config, ip_packet, ip_len) > 0) {
            if(tun_config.debug & DEBUG_TUN) {
                logger(LOG_DEBUG, "IP packet (%d bytes) extracted and written to TUN\n", ip_len);
            }
            return TRUE;
        }
    }

    return FALSE;
}
