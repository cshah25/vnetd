#!/usr/bin/env bash
# test_tunnel.sh - Automates testing the vnetd tunnel using network namespaces

if [ "$EUID" -ne 0 ]; then
  echo "Please run as root (sudo ./test_tunnel.sh)"
  exit
fi

echo "Cleaning up old namespaces..."
ip netns del peer1 2>/dev/null
ip netns del peer2 2>/dev/null

echo "Creating network namespaces..."
ip netns add peer1
ip netns add peer2

# Connect the namespaces via a veth pair so they can reach each other via UDP on 10.0.1.x
ip link add veth1 type veth peer name veth2
ip link set veth1 netns peer1
ip link set veth2 netns peer2

ip -n peer1 addr add 10.0.1.1/24 dev veth1
ip -n peer1 link set veth1 up
ip -n peer1 link set lo up

ip -n peer2 addr add 10.0.1.2/24 dev veth2
ip -n peer2 link set veth2 up
ip -n peer2 link set lo up

echo "Starting vnetd in namespaces..."
# peer1: tun0, binds 8200, sends to 10.0.1.2:8200
ip netns exec peer1 ./build/vnetd tun0 8200 10.0.1.2 8200 &
PID1=$!

# peer2: tun0, binds 8200, sends to 10.0.1.1:8200
ip netns exec peer2 ./build/vnetd tun0 8200 10.0.1.1 8200 &
PID2=$!

sleep 1

# Configure the TUN interfaces inside the namespaces
ip -n peer1 addr add 192.168.2.1/24 dev tun0
ip -n peer1 link set tun0 up

ip -n peer2 addr add 192.168.2.2/24 dev tun0
ip -n peer2 link set tun0 up

echo "------------------------------------------------"
echo "Tunnel established! Both vnetd instances are running in the background."
echo "Pinging from peer1 to peer2 across the VPN..."
echo "------------------------------------------------"
ip netns exec peer1 ping -c 3 192.168.2.2

echo "------------------------------------------------"
echo "Cleaning up..."
kill -9 $PID1 $PID2
wait $PID1 $PID2 2>/dev/null
ip netns del peer1
ip netns del peer2
echo "Done!"
