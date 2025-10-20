# IP400 TUN Bridge

## Overview

The IP400 TUN bridge (`ip400tun`) creates a virtual network interface (TUN device) that allows standard IP networking over the IP400 amateur radio mesh network. This enables applications like ping, SSH, HTTP, and any other IP-based protocol to work transparently over the mesh.

## How It Works

```
[Linux IP Stack]
       ↕ (IP packets)
   [TUN device: ip400]
       ↕
   [ip400tun daemon]
       ↕ (IP400 SPI frames, coding=IP_ENCAPSULATED)
   [/dev/spidev0.0]
       ↕
   [STM32 IP400 Node]
       ↕ (over radio)
   [IP400 Mesh Network]
```

The daemon:
1. Reads IP packets from the TUN interface
2. Looks up routing table to find destination callsign
3. Encapsulates IP packets in IP400 SPI frames (type: `IP_ENCAPSULATED`)
4. Sends to STM32 node via SPI
5. Receives IP400 frames from STM32, extracts IP packets, writes to TUN

## Building

```bash
cd /path/to/ip400tun
make
```

## Installation

```bash
sudo make install  # (Optional: copies to /usr/local/bin)
```

## Usage

### Basic Usage

```bash
sudo ./Ip400Tun -c YOUR_CALLSIGN
```

### With Gateway (Default Route)

```bash
sudo ./Ip400Tun -c VE6VH -g VE6ABC
```

This configures VE6ABC as the gateway for all unknown destinations.

### Command Line Options

```
Required:
  -c CALLSIGN   Your amateur radio callsign

Optional:
  -s DEVICE     SPI device (default: /dev/spidev0.0)
  -t NAME       TUN device name (default: ip400)
  -g CALLSIGN   Gateway callsign for default route
  -m MTU        MTU size (default: 1500)
  -d FLAGS      Debug flags (hex):
                  0x01 - Enable logging
                  0x02 - SPI debug
                  0x04 - TUN debug
                  0x08 - Bridge debug
  -h            Show help message
```

### Example with Debug Logging

```bash
sudo ./Ip400Tun -c VE6VH -g VE6ABC -d 0x0F
```

## Network Configuration

After starting ip400tun, configure the TUN interface:

```bash
# Assign IP address to the TUN interface
sudo ip addr add 172.16.1.10/24 dev ip400

# Bring interface up
sudo ip link set ip400 up

# Add routes (optional)
sudo ip route add 172.16.2.0/24 dev ip400
```

### Example Network Setup

If you're setting up a mesh network:

**Node 1 (VE6ABC):**
```bash
sudo ./Ip400Tun -c VE6ABC
sudo ip addr add 172.16.1.1/24 dev ip400
sudo ip link set ip400 up
```

**Node 2 (VE6VH):**
```bash
sudo ./Ip400Tun -c VE6VH -g VE6ABC
sudo ip addr add 172.16.1.2/24 dev ip400
sudo ip link set ip400 up
```

**Node 3 (VE6XYZ):**
```bash
sudo ./Ip400Tun -c VE6XYZ -g VE6ABC
sudo ip addr add 172.16.1.3/24 dev ip400
sudo ip link set ip400 up
```

Now you can ping between nodes:
```bash
ping 172.16.1.1   # From VE6VH to VE6ABC
```

## Routing Table

The bridge maintains a simple routing table that maps IP addresses/networks to callsigns.

Currently supported:
- Default gateway (configured with `-g` option)
- Manual routes can be added by modifying `ip400bridge.c`

Future enhancement: Dynamic routing table updates via configuration file or runtime API.

## Integration with Existing ip400spi

Note: If you're running `ip400spi` for UDP-based applications, you cannot run `ip400tun` simultaneously on the same SPI interface. Choose one:

- **ip400spi**: For applications that use the IP400 SPI frame format directly via UDP
- **ip400tun**: For standard IP networking applications

## Troubleshooting

### TUN device not created
- Ensure `/dev/net/tun` exists: `ls -l /dev/net/tun`
- Check kernel TUN module is loaded: `lsmod | grep tun`
- If not loaded: `sudo modprobe tun`

### Permission denied
- Must run as root: `sudo ./Ip400Tun ...`
- Or set capabilities: `sudo setcap cap_net_admin+ep ./Ip400Tun`

### No route to host
- Check routing table: `ip route show`
- Verify gateway callsign is correct
- Check debug logs: run with `-d 0x0F`

### SPI errors
- Verify SPI device: `ls -l /dev/spidev0.0`
- Check connections to STM32 node
- Verify STM32 firmware is running

## Technical Details

### Frame Format

IP packets are encapsulated in IP400 SPI frames with:
- **Eyecatcher**: "IP4C"
- **Coding**: `IP_ENCAPSULATED` (value: 5)
- **Source**: Your callsign + port ENC_IP_TYPE (4)
- **Dest**: Next-hop callsign (from routing table) + port ENC_IP_TYPE (4)
- **Payload**: Raw IP packet (up to MTU size)

### Address Mapping

The bridge uses a routing table to map destination IP addresses to callsigns:
- Longest prefix match for route selection
- Default gateway for unknown destinations
- Future: Could use IP-to-callsign mapping or DNS-like system

## License

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License.

Copyright (c) 2024-25 Alberta Digital Radio Communications Society

## See Also

- IP400 SPI Specification (in Documentation/)
- ip400spi - UDP-based interface to IP400 mesh
- Node Firmware - STM32 firmware source code
