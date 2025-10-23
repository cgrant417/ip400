#!/bin/bash
# Startup script for IP400 TUN bridge - Node 1 (VE3CA)
# Point-to-point tunnel over IP400 mesh network

# Configuration - MODIFY THESE FOR YOUR SETUP
MY_CALLSIGN="VE3CA"           # My callsign
GATEWAY_CALLSIGN="VE3AC"      # Remote tunnel endpoint callsign
TUNNEL_IP="10.0.0.1/24"       # My tunnel IP address
GATEWAY_VPN=""                # Gateway VPN filter (leave empty for broadcast to all)
DEBUG_FLAGS="0x0F"            # Debug: 0x01=log, 0x02=SPI, 0x04=TUN, 0x08=bridge, 0x0F=all

# Build command line
CMD="sudo ./Ip400Tun -c $MY_CALLSIGN -g $GATEWAY_CALLSIGN -i $TUNNEL_IP -d $DEBUG_FLAGS"

# Add VPN filter if specified
if [ -n "$GATEWAY_VPN" ]; then
    CMD="$CMD -v $GATEWAY_VPN"
fi

echo "Starting IP400 TUN bridge..."
echo "  My callsign: $MY_CALLSIGN"
echo "  Gateway: $GATEWAY_CALLSIGN"
echo "  Tunnel IP: $TUNNEL_IP"
if [ -n "$GATEWAY_VPN" ]; then
    echo "  VPN filter: $GATEWAY_VPN (unicast)"
else
    echo "  VPN filter: none (broadcast to all $GATEWAY_CALLSIGN nodes)"
fi
echo ""

# Start the bridge daemon
$CMD &
DAEMON_PID=$!

# Wait for TUN device to be created
echo "Waiting for TUN device..."
sleep 2

# Configure the interface
echo "Configuring ip400 interface..."
sudo ip addr add $TUNNEL_IP dev ip400
sudo ip link set ip400 up

echo ""
echo "✓ IP400 TUN bridge started (PID: $DAEMON_PID)"
echo "✓ Interface ip400 configured"
echo ""
echo "Waiting for gateway beacon (this may take up to 30 seconds)..."
echo "Watch daemon output for: 'New beacon: $GATEWAY_CALLSIGN'"
echo ""
echo "Once gateway beacon received, test with:"
echo "  ping 10.0.0.2"
echo ""
echo "To stop: sudo killall Ip400Tun"
