#!/usr/bin/env bash
# test_tunnel_benchmark.sh
# Functional + performance benchmark for the vnetd tunnel.
#
# Measures:
#   1. RTT/latency
#   2. TCP throughput (iperf3)
#   3. UDP throughput + packet loss (iperf3)
#   4. Approximate packets/sec from UDP packet size/rate
#   5. vnetd CPU usage during the UDP test
#   6. Scaling with multiple concurrent iperf3 flows
#   7. vnetd process stats (optional: pidstat/perf)
#
# Requirements:
#   iproute2, iperf3, ping
# Optional:
#   sysstat (pidstat), linux-tools/perf
#
# Run:
#   sudo ./test_tunnel_benchmark.sh
#
# The benchmark is intentionally conservative: it does NOT claim a metric
# unless the corresponding command actually produced a result.

set -u

if [ "$EUID" -ne 0 ]; then
  echo "Please run as root (sudo ./test_tunnel_benchmark.sh)"
  exit 1
fi

VNETD="./build/vnetd"
DURATION="${DURATION:-30}"
UDP_BW="${UDP_BW:-1G}"
UDP_LEN="${UDP_LEN:-512}"
FLOW_COUNTS="${FLOW_COUNTS:-1 4 8}"
RESULTS_DIR="${RESULTS_DIR:-./benchmark-results}"
RUN_ID="$(date +%Y%m%d_%H%M%S)"
RESULT_FILE="${RESULTS_DIR}/vnetd_${RUN_ID}.txt"

PID1=""
PID2=""
IPERF_PID=""

mkdir -p "$RESULTS_DIR"

cleanup() {
  set +e
  echo
  echo "Cleaning up..."

  [ -n "${IPERF_PID}" ] && kill "$IPERF_PID" 2>/dev/null
  [ -n "${PID1}" ] && kill "$PID1" 2>/dev/null
  [ -n "${PID2}" ] && kill "$PID2" 2>/dev/null

  wait "$IPERF_PID" 2>/dev/null
  wait "$PID1" 2>/dev/null
  wait "$PID2" 2>/dev/null

  ip netns del peer1 2>/dev/null
  ip netns del peer2 2>/dev/null

  echo "Done."
}
trap cleanup EXIT INT TERM

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "ERROR: '$1' is required but was not found."
    exit 1
  fi
}

require_cmd ip
require_cmd ping
require_cmd iperf3

echo "============================================================"
echo "vnetd Tunnel Benchmark"
echo "============================================================"
echo "Duration:       ${DURATION}s"
echo "UDP bandwidth:  ${UDP_BW}"
echo "UDP packet len: ${UDP_LEN} bytes"
echo "Flow counts:    ${FLOW_COUNTS}"
echo "Results:        ${RESULT_FILE}"
echo "============================================================"

{
  echo "vnetd Tunnel Benchmark"
  echo "Date: $(date)"
  echo "Duration: ${DURATION}s"
  echo "UDP bandwidth: ${UDP_BW}"
  echo "UDP packet length: ${UDP_LEN} bytes"
  echo
  echo "System:"
  uname -a
  echo
  echo "CPU:"
  lscpu 2>/dev/null | grep -E 'Model name|CPU\(s\)|Thread|Core|Socket' || true
  echo
} | tee "$RESULT_FILE"

echo
echo "[1/7] Cleaning up old namespaces..."
ip netns del peer1 2>/dev/null || true
ip netns del peer2 2>/dev/null || true

echo "[2/7] Creating network namespaces..."
ip netns add peer1
ip netns add peer2

ip link add veth1 type veth peer name veth2
ip link set veth1 netns peer1
ip link set veth2 netns peer2

ip -n peer1 addr add 10.0.1.1/24 dev veth1
ip -n peer1 link set veth1 up
ip -n peer1 link set lo up

ip -n peer2 addr add 10.0.1.2/24 dev veth2
ip -n peer2 link set veth2 up
ip -n peer2 link set lo up

echo "[3/7] Starting vnetd..."
ip netns exec peer1 "$VNETD" tun0 8200 10.0.1.2 8200 &
PID1=$!

ip netns exec peer2 "$VNETD" tun0 8200 10.0.1.1 8200 &
PID2=$!

sleep 1

if ! kill -0 "$PID1" 2>/dev/null || ! kill -0 "$PID2" 2>/dev/null; then
  echo "ERROR: vnetd did not start correctly."
  exit 1
fi

echo "[4/7] Configuring TUN interfaces..."
ip -n peer1 addr add 192.168.2.1/24 dev tun0
ip -n peer1 link set tun0 up

ip -n peer2 addr add 192.168.2.2/24 dev tun0
ip -n peer2 link set tun0 up

sleep 1

echo
echo "============================================================"
echo "Baseline connectivity / latency"
echo "============================================================"

PING_OUTPUT="$(ip netns exec peer1 ping -c 20 -i 0.05 192.168.2.2 2>&1)"
echo "$PING_OUTPUT"
{
  echo
  echo "=== LATENCY ==="
  echo "$PING_OUTPUT"
} >> "$RESULT_FILE"

echo
echo "============================================================"
echo "Starting iperf3 server inside peer2"
echo "============================================================"

ip netns exec peer2 iperf3 -s -1 >"${RESULTS_DIR}/iperf_server_${RUN_ID}.log" 2>&1 &
IPERF_PID=$!
sleep 1

if ! kill -0 "$IPERF_PID" 2>/dev/null; then
  echo "ERROR: iperf3 server failed to start."
  echo "Check ${RESULTS_DIR}/iperf_server_${RUN_ID}.log"
  exit 1
fi

echo
echo "============================================================"
echo "TCP throughput: ${DURATION}s"
echo "============================================================"

TCP_OUTPUT="$(ip netns exec peer1 iperf3 \
  -c 192.168.2.2 \
  -t "$DURATION" \
  -J 2>&1)"

echo "$TCP_OUTPUT" > "${RESULTS_DIR}/tcp_${RUN_ID}.json"

if command -v python3 >/dev/null 2>&1; then
  python3 - "$RESULTS_DIR/tcp_${RUN_ID}.json" <<'PY'
import json, sys
path = sys.argv[1]
try:
    d = json.load(open(path))
    s = d["end"]["sum_sent"]
    r = d["end"]["sum_received"]
    print(f"TCP sent:     {s['bits_per_second']/1e9:.3f} Gbps")
    print(f"TCP received: {r['bits_per_second']/1e9:.3f} Gbps")
    print(f"TCP retransmits: {s.get('retransmits', 'N/A')}")
except Exception as e:
    print(f"Could not parse TCP JSON: {e}")
PY
else
  echo "$TCP_OUTPUT"
fi

{
  echo
  echo "=== TCP THROUGHPUT ==="
  cat "${RESULTS_DIR}/tcp_${RUN_ID}.json"
} >> "$RESULT_FILE"

echo
echo "============================================================"
echo "UDP throughput + packet loss: ${DURATION}s @ ${UDP_BW}"
echo "Packet size: ${UDP_LEN} bytes"
echo "============================================================"

# Restart the one-shot server for the UDP test.
kill "$IPERF_PID" 2>/dev/null || true
wait "$IPERF_PID" 2>/dev/null || true
IPERF_PID=""

ip netns exec peer2 iperf3 -s -1 >"${RESULTS_DIR}/iperf_server_udp_${RUN_ID}.log" 2>&1 &
IPERF_PID=$!
sleep 1

UDP_OUTPUT="$(ip netns exec peer1 iperf3 \
  -c 192.168.2.2 \
  -u \
  -b "$UDP_BW" \
  -l "$UDP_LEN" \
  -t "$DURATION" \
  -J 2>&1)"

echo "$UDP_OUTPUT" > "${RESULTS_DIR}/udp_${RUN_ID}.json"

if command -v python3 >/dev/null 2>&1; then
  python3 - "$RESULTS_DIR/udp_${RUN_ID}.json" "$UDP_LEN" <<'PY'
import json, sys
path = sys.argv[1]
packet_len = int(sys.argv[2])
try:
    d = json.load(open(path))
    s = d["end"]["sum"]
    bps = s.get("bits_per_second", 0)
    lost = s.get("lost_packets", 0)
    total = s.get("packets", 0)
    loss_pct = s.get("lost_percent", 0)
    pps = bps / (packet_len * 8) if packet_len else 0

    print(f"UDP throughput: {bps/1e9:.3f} Gbps")
    print(f"Approx packet rate: {pps:,.0f} packets/sec")
    print(f"UDP packets: {total:,}")
    print(f"UDP lost: {lost:,}")
    print(f"UDP packet loss: {loss_pct:.4f}%")
except Exception as e:
    print(f"Could not parse UDP JSON: {e}")
PY
else
  echo "$UDP_OUTPUT"
fi

{
  echo
  echo "=== UDP THROUGHPUT / PACKET RATE / LOSS ==="
  cat "${RESULTS_DIR}/udp_${RUN_ID}.json"
} >> "$RESULT_FILE"

echo
echo "============================================================"
echo "CPU measurement during UDP test"
echo "============================================================"

# iperf3 is one-shot, so start another server and run UDP in the
# background while sampling the two vnetd processes.
kill "$IPERF_PID" 2>/dev/null || true
wait "$IPERF_PID" 2>/dev/null || true
IPERF_PID=""

ip netns exec peer2 iperf3 -s -1 >"${RESULTS_DIR}/iperf_server_cpu_${RUN_ID}.log" 2>&1 &
IPERF_PID=$!
sleep 1

CPU_LOG="${RESULTS_DIR}/cpu_${RUN_ID}.log"
: > "$CPU_LOG"

if command -v pidstat >/dev/null 2>&1; then
  # pidstat sees the vnetd process from the host namespace by PID.
  pidstat -p "$PID1,$PID2" 1 > "$CPU_LOG" 2>&1 &
  PIDSTAT_PID=$!

  ip netns exec peer1 iperf3 \
    -c 192.168.2.2 \
    -u \
    -b "$UDP_BW" \
    -l "$UDP_LEN" \
    -t "$DURATION" \
    >"${RESULTS_DIR}/udp_cpu_${RUN_ID}.txt" 2>&1

  kill "$PIDSTAT_PID" 2>/dev/null || true
  wait "$PIDSTAT_PID" 2>/dev/null || true

  cat "$CPU_LOG"
  {
    echo
    echo "=== CPU (pidstat) ==="
    cat "$CPU_LOG"
  } >> "$RESULT_FILE"

  echo
  echo "The pidstat log above is the CPU measurement to report."
else
  echo "pidstat not installed; skipping CPU measurement."
  echo "Install it with your distro's sysstat package and rerun."
  echo "NOTE: do not invent a CPU percentage."
fi

echo
echo "============================================================"
echo "Concurrent-flow scaling"
echo "============================================================"

{
  echo
  echo "=== CONCURRENT FLOW SCALING ==="
} >> "$RESULT_FILE"

for FLOWS in $FLOW_COUNTS; do
  kill "$IPERF_PID" 2>/dev/null || true
  wait "$IPERF_PID" 2>/dev/null || true
  IPERF_PID=""

  ip netns exec peer2 iperf3 -s -1 >"${RESULTS_DIR}/iperf_server_p${FLOWS}_${RUN_ID}.log" 2>&1 &
  IPERF_PID=$!
  sleep 1

  echo
  echo "--- ${FLOWS} concurrent flow(s) ---"

  FLOW_OUTPUT="$(ip netns exec peer1 iperf3 \
    -c 192.168.2.2 \
    -P "$FLOWS" \
    -t "$DURATION" \
    -J 2>&1)"

  FLOW_FILE="${RESULTS_DIR}/tcp_p${FLOWS}_${RUN_ID}.json"
  echo "$FLOW_OUTPUT" > "$FLOW_FILE"

  if command -v python3 >/dev/null 2>&1; then
    python3 - "$FLOW_FILE" "$FLOWS" <<'PY'
import json, sys
path = sys.argv[1]
flows = sys.argv[2]
try:
    d = json.load(open(path))
    s = d["end"]["sum_sent"]
    print(f"{flows} flow(s): {s['bits_per_second']/1e9:.3f} Gbps")
except Exception as e:
    print(f"Could not parse {flows}-flow JSON: {e}")
PY
  else
    echo "$FLOW_OUTPUT"
  fi

  {
    echo
    echo "--- ${FLOWS} concurrent flow(s) ---"
    cat "$FLOW_FILE"
  } >> "$RESULT_FILE"
done

echo
echo "============================================================"
echo "Benchmark complete"
echo "============================================================"
echo "Human-readable results:"
echo "  $RESULT_FILE"
echo
echo "Raw JSON results:"
ls -1 "${RESULTS_DIR}"/*"${RUN_ID}"*.json 2>/dev/null || true
echo
echo "Use only measurements that completed successfully."
