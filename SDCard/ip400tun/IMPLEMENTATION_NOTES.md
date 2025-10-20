# IP400 TUN Bridge Implementation Notes

## Overview

This document describes the implementation of the IP400 TUN bridge (`ip400tun`), which was created to provide standard IP networking capabilities over the IP400 amateur radio mesh network.

## Problem Statement

The IP400 firmware already supports `IP_ENCAPSULATED` packet types (frame.h:144), but there was no Linux-side utility to:
- Create a network interface for IP applications
- Encapsulate IP packets in IP400 frames
- Extract IP packets from received IP400 frames
- Provide routing table to map IP addresses to callsigns

## Solution Architecture

### Components Created

1. **TUN Device Management** (`tun.c`, `tun.h`)
   - Creates `/dev/net/tun` device (appears as `ip400` interface)
   - Reads IP packets from Linux network stack
   - Writes IP packets back to network stack
   - Non-blocking I/O for integration with SPI polling

2. **IP-to-IP400 Bridge** (`ip400bridge.c`, `ip400bridge.h`)
   - Routing table: maps IP networks to callsigns
   - `ip_to_ip400()`: Encapsulates IP packets in IP400 SPI frames
   - `ip400_to_ip()`: Extracts IP packets from IP400 frames
   - Default gateway support for unknown destinations

3. **Callsign Encoding** (`callsign.c`)
   - Base-40 encoding: 6-character callsign compressed to 4 bytes
   - Character set: space, 0-9, A-Z (40 characters)
   - Compatible with excess-40 encoding used in node firmware

4. **SPI Task Integration** (`spitask.c`)
   - Modified from `ip400spi` to work with TUN bridge
   - Calls `process_ip400_frame()` for received frames
   - Filters for `IP_ENCAPSULATED` packet type

5. **Main Daemon** (`main.c`)
   - Combined task: runs both `spiTask()` and `tunTask()` every 100ms
   - Command-line parsing for callsign, gateway, debug options
   - Timer-based polling (same as `ip400spi`)

### Data Flow

**Outbound (Linux → Radio):**
```
Application (e.g., ping)
    ↓
Linux IP Stack
    ↓
TUN device (ip400)
    ↓
tunTask() reads IP packet
    ↓
ip_to_ip400() encapsulates:
  - Looks up destination IP in routing table
  - Finds next-hop callsign
  - Creates SPI frame header
  - Copies IP packet as payload
  - Sets coding = IP_ENCAPSULATED
    ↓
Enqueues to SPITxQueue
    ↓
spiTask() sends to STM32 via SPI
    ↓
STM32 transmits over radio
```

**Inbound (Radio → Linux):**
```
STM32 receives from radio
    ↓
spiTask() receives via SPI
    ↓
Validates frame type
    ↓
Checks: coding == IP_ENCAPSULATED?
    ↓
process_ip400_frame() calls ip400_to_ip()
    ↓
Extracts IP packet from payload
    ↓
tun_write() to TUN device
    ↓
Linux IP Stack
    ↓
Application receives packet
```

## Key Design Decisions

### 1. TUN vs TAP
- **Chose TUN**: Layer 3 (IP) interface
- TAP would be Layer 2 (Ethernet), requiring MAC address mapping
- IP400 mesh is inherently Layer 3 (uses callsigns, not MAC addresses)

### 2. Routing Table
- Simple linked-list routing table
- Longest prefix match algorithm
- Static configuration (future: dynamic updates)
- Default gateway support via `-g` option

### 3. Callsign Encoding
- Base-40 encoding: 6 chars → 4 bytes
- Formula: `value = c[0]*40^5 + c[1]*40^4 + ... + c[5]`
- Character mapping: space=0, 0-9=1-10, A-Z=11-36

### 4. MTU Consideration
- Default MTU: 1500 bytes (standard Ethernet)
- IP400 max payload: 1053 bytes
- IP packets >1053 bytes will be fragmented by IP stack before reaching TUN
- Fragmentation handled at IP layer, transparent to application

### 5. Integration with ip400spi
- **Cannot run both simultaneously** on same SPI device
- Shared code: `spi.c`, `dataq.c`, `timer.c`, `logger.c`, `errno.c`
- Use symlinks to avoid code duplication
- Choose one: `ip400spi` (UDP/raw frames) or `ip400tun` (IP networking)

## Building and Dependencies

### Build Process
```bash
make clean
make
```

### Shared Files (from ip400spi)
- Header files: symlinked from `../ip400spi/include/`
- Source files: compiled from `../ip400spi/src/` by Makefile

### Dependencies
- Linux kernel TUN/TAP support (`CONFIG_TUN`)
- SPI device driver (`spidev`)
- Standard C library
- pthreads (for timer)

## Testing Recommendations

### 1. Basic Functionality
```bash
# Terminal 1: Start daemon
sudo ./Ip400Tun -c CALL1 -d 0x0F

# Terminal 2: Configure interface
sudo ip addr add 172.16.1.1/24 dev ip400
sudo ip link set ip400 up
```

### 2. Loopback Test (without hardware)
- Modify `spiTask()` to loop TX frames back to RX
- Test IP stack integration without radio

### 3. Two-Node Test
- Node 1: `172.16.1.1` (gateway)
- Node 2: `172.16.1.2` (uses Node 1 as gateway)
- Test: `ping 172.16.1.1` from Node 2

### 4. Multi-Hop Test
- Requires 3+ nodes with mesh routing
- Test hop count increment in IP400 frames

## Known Limitations

1. **Routing Table**
   - Currently static (configured at startup)
   - No dynamic routing protocol
   - No route learning from received frames

2. **IP Address to Callsign Mapping**
   - Manual configuration required
   - No DNS-like resolution
   - Future: Could implement callsign → IP mapping protocol

3. **Performance**
   - 100ms polling interval (10 polls/second)
   - Could be optimized with interrupt-driven SPI
   - Suitable for low-bandwidth mesh networking

4. **MTU Fragmentation**
   - Linux IP stack handles fragmentation
   - IP400 mesh will carry fragments as separate packets
   - Reassembly at destination IP stack

5. **Security**
   - No encryption (amateur radio regulations)
   - No authentication (trust callsign encoding)
   - IP400 is inherently open mesh network

## Future Enhancements

1. **Dynamic Routing**
   - Implement route learning from received packets
   - Source IP → source callsign mapping
   - Automatic routing table updates

2. **Configuration File**
   - Static routes from config file
   - IP address assignment
   - Gateway configuration

3. **ARP-like Protocol**
   - Callsign resolution protocol
   - "Who has IP 172.16.1.x?" broadcasts
   - Cache discovered mappings

4. **Integration with ip400spi**
   - Multiplex both on same SPI interface
   - Route by packet type (IP_ENCAPSULATED vs others)

5. **Performance Monitoring**
   - Packet counters
   - Route statistics
   - Quality metrics

## References

- IP400 SPI Specification (Documentation/)
- Linux TUN/TAP documentation: `Documentation/networking/tuntap.txt`
- IP400 frame.h for packet types
- Original ip400spi implementation

## Author

Implementation by Claude Code (claude.ai/code)
October 19, 2025

Based on IP400 project architecture
Copyright (c) 2024-25 Alberta Digital Radio Communications Society
