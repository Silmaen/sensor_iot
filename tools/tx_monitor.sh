#!/bin/sh
# tx_monitor.sh — logs per-station WiFi stats from an OpenWrt AP, to observe how a
# deep-sleep ESP node behaves on the air (associate -> uplink -> disassociate each
# wake) and to quantify its TRANSMIT quality (the AP-side `signal` = the device's
# uplink RSSI as received by the AP — the metric the device cannot self-report).
#
# BusyBox ash compatible. Copy to the router and run, e.g.:
#   scp tools/tx_monitor.sh root@ROUTER:/tmp/
#   ssh root@ROUTER 'sh /tmp/tx_monitor.sh 1200 1 /tmp/tx_monitor.log'          # all stations
#   ssh root@ROUTER 'sh /tmp/tx_monitor.sh 1200 1 /tmp/tx_monitor.log AA:BB:CC' # filter one MAC
# then read it back:
#   scp root@ROUTER:/tmp/tx_monitor.log .
#
# Args: [duration_s=1200] [interval_s=1] [logfile=/tmp/tx_monitor.log] [mac_filter=""]
#
# Each present-station line is timestamped to the second; GAPS in the timestamps of
# a given MAC = the device sleeping. A burst of lines = one wake window. Read the
# per-wake `sig` to see TX quality, and compare a C3 vs an ESP8266 at the same spot.

DURATION="${1:-1200}"
INTERVAL="${2:-1}"
LOG="${3:-/tmp/tx_monitor.log}"
FILTER="${4:-}"

# Discover AP-mode interfaces (fallback: every interface iw knows).
APIFS=$(iw dev 2>/dev/null | awk '/Interface/{i=$2} /type AP/{print i}')
[ -z "$APIFS" ] && APIFS=$(iw dev 2>/dev/null | awk '/Interface/{print $2}')

{
  echo "# tx_monitor start $(date '+%F %T') dur=${DURATION}s int=${INTERVAL}s ifs='${APIFS}' filter='${FILTER}'"
  echo "# columns: TIMESTAMP IFACE MAC sig=<dBm> avg=<dBm> rxrate=<Mbit uplink> txrate=<Mbit downlink> retries=<AP->dev> failed=<AP->dev> inact=<ms> conn=<s>"
} > "$LOG"

END=$(( $(date +%s) + DURATION ))
while [ "$(date +%s)" -lt "$END" ]; do
  TS=$(date '+%F %T')
  for IF in $APIFS; do
    iw dev "$IF" station dump 2>/dev/null | awk -v ts="$TS" -v ifc="$IF" '
      /^Station/        { if (mac!="") print line(); mac=$2; sig=avg=rxr=txr=tr=tf=inact=ct="?" }
      /inactive time:/  { inact=$3 }
      /^[ \t]*signal:/  { sig=$2 }
      /signal avg:/     { avg=$3 }
      /rx bitrate:/     { rxr=$3 }
      /tx bitrate:/     { txr=$3 }
      /tx retries:/     { tr=$3 }
      /tx failed:/      { tf=$3 }
      /connected time:/ { ct=$3 }
      END               { if (mac!="") print line() }
      function line() { return ts" "ifc" "mac" sig="sig" avg="avg" rxrate="rxr" txrate="txr" retries="tr" failed="tf" inact="inact" conn="ct }
    '
  done | { if [ -n "$FILTER" ]; then grep -i "$FILTER"; else cat; fi; } >> "$LOG"
  sleep "$INTERVAL"
done
echo "# tx_monitor end $(date '+%F %T')" >> "$LOG"
