# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

IP400 is an amateur radio mesh networking project that provides firmware for STM32-based nodes and supporting utilities for Raspberry Pi HAT integration. The system uses a custom MAC layer (IP400_MAC) with compressed callsign encoding and supports multiple packet types including text, audio, video, beacon, and various digital voice modes.

**Current revision level:** 1.4

## Repository Structure

### Node Firmware (`Node Firmware/`)
Platform-agnostic firmware source code for IP400 nodes running on STM32 microcontrollers with FreeRTOS.

- **`IP400/`** - Core protocol implementation shared across all node types
  - `Inc/` - Header files defining the IP400 protocol
  - `Src/` - Implementation files for frame handling, mesh routing, SPI interface, etc.
  - Key modules:
    - `frame.c/h` - Frame encoding/decoding, MAC addressing, packet types
    - `mesh.c` - Mesh routing table management
    - `spi.c/h` - SPI interface for Raspberry Pi communication
    - `ip.c/h` - IP address generation from MAC addresses
    - `beacon.c` - Beacon frame transmission
    - `chat.c` - Text messaging application
    - `callsign.c` - Amateur radio callsign encoding (excess-40 compression)
    - `tod.c` - Time-of-day management
    - `dataq.c` - Data queue management
    - `logger.c` - Debug logging
    - `menu.c` - Console menu interface

- Two platform-specific implementations available as CubeIDE projects (in ZIP files):
  - **NucleoCC2** - STM32WL33 Nucleo experimenter board
  - **PIZero_Ichiban_E04** - Mini-node for Raspberry Pi HAT

### SDCard (`SDCard/`)
Utilities and code for Raspberry Pi systems with IP400 HAT nodes. These tools run on the Pi to interact with the attached STM32 node.

- **`stm32flash/`** - STM32 firmware flashing utility via I2C
  - Custom version supporting I2C interface for in-circuit programming
  - Build: `make` then `sudo make install`
  - Uses GPIO pins for bootloader control

- **`eeprom/`** - Raspberry Pi HAT EEPROM tools
  - `eepmake` - Generate EEPROM binary from settings file
  - `eepdump` - Read/display EEPROM contents
  - Build: `make`
  - Flash EEPROM: `make flash` (run as root)
  - Configuration: `STMwl33.txt` -> `STMwl33.eep`

- **`ip400spi/`** - SPI bridge between Pi and STM32 node
  - Implements SPI protocol defined in "IP400 SPI Specification" document
  - Creates UDP socket interface for sending/receiving IP400 frames
  - Build: `make` (produces `Ip400Spi` executable)
  - Run: `./runSpi.sh`

- **`ip400tun/`** - **NEW** TUN/TAP bridge for IP networking over IP400 mesh
  - Creates virtual network interface for standard IP applications
  - Encapsulates IP packets in IP400 frames (type: `IP_ENCAPSULATED`)
  - Enables ping, SSH, HTTP, and other IP protocols over the mesh
  - Build: `make` (produces `Ip400Tun` executable)
  - Run: `sudo ./Ip400Tun -c CALLSIGN -g GATEWAY`
  - See `ip400tun/README.md` for detailed usage

- **`code/`** - Convenience scripts for node management
  - `ip400flash.sh <filename>` - Flash firmware to attached node
  - `minicom.sh` - Open serial console to node (115200 baud)

### Documentation (`Documentation/`)
PDF documentation for hardware variants, protocols, and assembly guides. See individual PDFs for detailed specifications.

### Wireshark (`Wireshark/`)
Protocol dissector scripts for IP400 packet analysis using Wireshark Generic Dissector (WSGD).

## Common Development Tasks

### Building stm32flash utility
```bash
cd SDCard/stm32flash
make
sudo make install
```

### Building EEPROM tools
```bash
cd SDCard/eeprom
make
# To generate and flash EEPROM:
make flash  # (requires root)
```

### Building IP400 SPI bridge
```bash
cd SDCard/ip400spi
make
```

### Building IP400 TUN bridge (for IP networking)
```bash
cd SDCard/ip400tun
make
```

### Flashing firmware to node
```bash
# On Raspberry Pi with attached HAT node:
cd SDCard/code
sudo ./ip400flash.sh PIZero_Ichiban_E04.bin
```

### Connecting to node console
```bash
cd SDCard/code
./minicom.sh  # 115200 baud on /dev/serial0
```

## Architecture Notes

### Frame Structure
IP400 uses a custom MAC layer with the following key concepts:

- **MAC Address** (`IP400_MAC`): 4-byte compressed amateur radio callsign + 2-byte VPN/port identifier
- **Callsign encoding**: Excess-40 compression for 6-character callsigns into 4 bytes
- **Frame types**: Text, beacon, echo request/response, data, various voice codec encapsulation
- **Hop table support**: Optional hop routing for mesh networking
- **Packet sizes**: 56 to 1053 byte payload

### IP Address Generation
Three modes defined in `ip.h`:
- `IP_10_GROUP` - Generate IP 10.x.x.x addresses
- `IP_172_GROUP` - Generate IP 172.x.x.x addresses
- `IP_172_ID` - Generate IP 172.x.x.x from node ID (default)

### SPI Protocol (Pi-to-Node Communication)
Frames transferred via SPI use custom header structure (`struct spi_hdr_t`) with:
- 4-byte eyecatcher "IP4X"
- Status byte (NO_FRAME, SINGLE_FRAME, FRAME_FRAGMENT, LAST_FRAGMENT)
- Offset and length fields for fragmentation
- Source/dest callsigns and ports
- Coding and hop count fields
- Max 400 bytes per SPI transfer

### Mesh Routing
Mesh table tracks learned routes. Frames can be repeated with hop count limiting (max 15 hops).

## Platform-Specific Notes

### STM32 Firmware Development
- Use STM32CubeIDE to import projects from ZIP files
- See `Node Firmware/How to import code into STM32CubeIDE.pdf`
- Common IP400 source is compiled for both platforms (Nucleo CC2 and Mini-Node)
- Uses FreeRTOS for task management

### Raspberry Pi HAT Nodes
- GPIO 20 = nRESET (pin 532)
- GPIO 21 = BOOT_ENA (pin 533)
- Serial interface: `/dev/serial0` at 115200 baud
- SPI interface for data frames
- I2C interface for firmware flashing

### Cross-compilation
For Windows builds from Linux:
```bash
cd SDCard/stm32flash
make CC=x86_64-w64-mingw32-gcc AR=x86_64-w64-mingw32-ar
```

## Important Safety Notes

The voltage regulator on mini-nodes can become very hot during operation. Ensure adequate ventilation and consider adding a heatsink. Do not touch the regulator during operation.

## External Resources

Project website: ip400.adrcs.org
- Hardware documentation
- Getting Started guide with SD card disk image download
- Release notes in "IP400 Node Software" document
