/*---------------------------------------------------------------------------
        Project:          ip400tun

        File Name:        logger.h

        Author:           From ip400spi

        Description:      Logger definitions

                          This program is free software: you can redistribute it and/or modify
                          it under the terms of the GNU General Public License as published by
                          the Free Software Foundation, either version 2 of the License, or
                          (at your option) any later version, provided this copyright notice
                          is included.

                          Copyright (c) 2024-25 Alberta Digital Radio Communications Society

---------------------------------------------------------------------------*/
#ifndef LOGGER_H_
#define LOGGER_H_

// Debug flags
#define DEBUG_LOG       0x01
#define DEBUG_SPI       0x02
#define DEBUG_TUN       0x04
#define DEBUG_BRIDGE    0x08

// Log levels
#define LOG_DEBUG       0
#define LOG_NOTICE      1
#define LOG_ERROR       2
#define LOG_FATAL       3

// Logger functions
void openLog(BOOL debug);
void logger(int severity, char *format, ...);

#endif /* LOGGER_H_ */
