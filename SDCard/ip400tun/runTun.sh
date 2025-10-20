#!/bin/bash
# Example startup script for IP400 TUN bridge
# Modify callsigns and IP addresses for your network

# Configuration
MY_CALLSIGN="VE6VH"
GATEWAY_CALLSIGN="VE6ABC"
MY_IP="172.16.1.2/24"
DEBUG_FLAGS="0x01"  # Enable logging only

# Start the bridge
sudo ./Ip400Tun -c $MY_CALLSIGN -g $GATEWAY_CALLSIGN -d $DEBUG_FLAGS &

# Wait for TUN device to be created
sleep 2

# Configure the interface
sudo ip addr add $MY_IP dev ip400
sudo ip link set ip400 up

echo "IP400 TUN bridge started"
echo "Interface: ip400"
echo "IP Address: $MY_IP"
echo "Gateway: $GATEWAY_CALLSIGN"
echo ""
echo "Test with: ping 172.16.1.1"
