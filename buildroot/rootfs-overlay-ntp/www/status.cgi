#!/bin/sh
# Ramliya - read-only status endpoint.
# POSIX sh (busybox ash / mksh compatible). No bash-isms, no side effects,
# no writes, no query parsing. Emits application/json.
#
# Deploy: /www/pages/status.cgi (chmod 0755).
# lighttpd:  cgi.assign = ( ".cgi" => "/bin/sh" )
# busybox httpd: place under a cgi-bin/ dir or configure *.cgi handler.

printf 'Content-Type: application/json\r\n'
printf 'Cache-Control: no-store\r\n'
printf '\r\n'

# --- helpers ---------------------------------------------------------------
rd()  { [ -r "$1" ] && head -n1 "$1" 2>/dev/null || printf ''; }
esc() { sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/\r//g'; }
kv()  { printf '"%s":"%s",' "$1" "$(printf '%s' "$2" | esc)"; }        # string, trailing comma
kn()  { if [ -n "$2" ]; then printf '"%s":%s,' "$1" "$2"; else printf '"%s":null,' "$1"; fi; }  # number|null

CHRONYC=$(command -v chronyc 2>/dev/null)
PMC=$(command -v pmc 2>/dev/null)
PHC_CTL=$(command -v phc_ctl 2>/dev/null)
PPS=/sys/devices/platform/pps-timer

# --- firmware / banner -----------------------------------------------------
KVER_RAW=$(uname -r 2>/dev/null)         # full release incl. CONFIG_LOCALVERSION noise
KVER_BASE=${KVER_RAW%% *}                # first token only -> e.g. 5.10.168
PREEMPT=$(uname -v 2>/dev/null | grep -o 'PREEMPT[_A-Z]*' | head -n1)
DRV=$(rd "$PPS/version")                 # e.g. 1.2  (from pps-dmtimer sysfs)
FW=$(rd /etc/ramliya-version)            # optional override for the product line

KLINE="linux $KVER_BASE"
[ -n "$PREEMPT" ] && KLINE="$KLINE $PREEMPT"          # -> linux 5.10.168 PREEMPT
[ -n "$FW" ] && VLINE="$FW" || VLINE="Ramliya v$DRV"  # firmware ver == driver ver

# --- GPS / PPS capture -----------------------------------------------------
PPS_COUNT=$(rd "$PPS/count")
PPS_SPUR=$(rd "$PPS/spurious")
PPS_DELTA=$(rd "$PPS/last_delta_ns")
PPS_RATE=$(rd "$PPS/rate")
IRQ_LINE=$(grep -i 'pps-dmtimer' /proc/interrupts 2>/dev/null | head -n1)
PPS_IRQ=$(printf '%s' "$IRQ_LINE" | awk '{sub(/:/,"",$1); print $1}')
PPS_IRQ_CNT=$(printf '%s' "$IRQ_LINE" | awk '{print $2}')

# --- chrony tracking -------------------------------------------------------
TRK=""
[ -n "$CHRONYC" ] && TRK=$("$CHRONYC" -n tracking 2>/dev/null)
track() { printf '%s\n' "$TRK" | grep "^$1" | sed 's/^[^:]*: *//' | head -n1; }
REFID=$(track 'Reference ID')
STRATUM=$(track 'Stratum')
SYSTIME=$(track 'System time')
LASTOFF=$(track 'Last offset')
RMSOFF=$(track 'RMS offset')
FREQ=$(track 'Frequency')
SKEW=$(track 'Skew')
ROOTDLY=$(track 'Root delay')
ROOTDISP=$(track 'Root dispersion')
UPDINT=$(track 'Update interval')
LEAP=$(track 'Leap status')

# --- chrony sources / sourcestats (arrays) --------------------------------
SOURCES='[]'
SRCSTATS='[]'
if [ -n "$CHRONYC" ]; then
	SOURCES=$("$CHRONYC" -n sources 2>/dev/null | awk '
		NR<=2 {next}
		NF>=6 {
			if(o!="") o=o",";
			o=o sprintf("{\"ms\":\"%s\",\"name\":\"%s\",\"stratum\":\"%s\",\"poll\":\"%s\",\"reach\":\"%s\",\"lastrx\":\"%s\"}",$1,$2,$3,$4,$5,$6)
		}
		END{printf "[%s]", o}')
	SRCSTATS=$("$CHRONYC" -n sourcestats 2>/dev/null | awk '
		NR<=2 {next}
		NF>=8 {
			if(o!="") o=o",";
			o=o sprintf("{\"name\":\"%s\",\"np\":\"%s\",\"span\":\"%s\",\"freq\":\"%s\",\"skew\":\"%s\",\"offset\":\"%s\",\"stddev\":\"%s\"}",$1,$2,$4,$5,$6,$7,$8)
		}
		END{printf "[%s]", o}')
fi

# --- PTP (best effort; only if ptp4l running + pmc present) ---------------
PTP_AVAIL=false; PORTSTATE=""; CLKCLASS=""; PHC_CMP=""
if [ -n "$PMC" ]; then
	PDS=$("$PMC" -u -b 0 'GET PORT_DATA_SET' 2>/dev/null)
	if [ -n "$PDS" ]; then
		PTP_AVAIL=true
		PORTSTATE=$(printf '%s\n' "$PDS" | grep -i 'portState' | sed 's/.*portState *//; s/ *$//' | head -n1)
		GMS=$("$PMC" -u -b 0 'GET GRANDMASTER_SETTINGS_NP' 2>/dev/null)
		CLKCLASS=$(printf '%s\n' "$GMS" | grep -i 'clockClass' | sed 's/.*clockClass *//; s/ *$//' | head -n1)
	fi
fi
if [ -n "$PHC_CTL" ] && [ -e /dev/ptp0 ]; then
	PHC_CMP=$("$PHC_CTL" /dev/ptp0 cmp 2>/dev/null | grep -o '[-0-9][0-9]*ns' | head -n1)
fi

# --- system ----------------------------------------------------------------
UP=$(rd /proc/uptime | awk '{print $1}')
LOAD=$(rd /proc/loadavg | awk '{print $1" "$2" "$3}')
MEMTOTAL=$(awk '/^MemTotal:/{print $2}'     /proc/meminfo 2>/dev/null)
MEMFREE=$(awk  '/^MemFree:/{print $2}'      /proc/meminfo 2>/dev/null)
MEMAVAIL=$(awk '/^MemAvailable:/{print $2}' /proc/meminfo 2>/dev/null)

TEMP=""
for h in /sys/class/hwmon/hwmon*; do
	[ -r "$h/name" ] || continue
	if [ "$(cat "$h/name" 2>/dev/null)" = "am335x_bandgap" ] && [ -r "$h/temp1_input" ]; then
		TEMP=$(cat "$h/temp1_input"); break
	fi
done

IFACE=eth0
NETSTATE=$(rd /sys/class/net/$IFACE/operstate)
NETSPEED=$(rd /sys/class/net/$IFACE/speed)
NETCARR=$(rd /sys/class/net/$IFACE/carrier)
IP=""
if command -v ip >/dev/null 2>&1; then
	IP=$(ip -4 -o addr show "$IFACE" 2>/dev/null | awk '{print $4}' | head -n1)
elif command -v ifconfig >/dev/null 2>&1; then
	IP=$(ifconfig "$IFACE" 2>/dev/null | awk '/inet /{for(i=1;i<=NF;i++){if($i~/addr:/){sub(/addr:/,"",$i);print $i}else if($i~/^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$/){print $i;exit}}}' | head -n1)
fi

# --- emit JSON -------------------------------------------------------------
printf '{'

printf '"fw":{'
printf '"project":"Ramliya",'
kv 'kernel_base' "$KVER_BASE"
kv 'preempt'     "$PREEMPT"
kv 'driver'      "$DRV"
kv 'kernel_line' "$KLINE"
printf '"version_line":"%s"' "$(printf '%s' "$VLINE" | esc)"
printf '},'

printf '"pps":{'
kn 'count'         "$PPS_COUNT"
kn 'spurious'      "$PPS_SPUR"
kn 'last_delta_ns' "$PPS_DELTA"
kn 'rate'          "$PPS_RATE"
kv 'irq'           "$PPS_IRQ"
if [ -n "$PPS_IRQ_CNT" ]; then printf '"irq_count":%s' "$PPS_IRQ_CNT"; else printf '"irq_count":null'; fi
printf '},'

printf '"chrony":{'
kv 'refid'           "$REFID"
kv 'stratum'         "$STRATUM"
kv 'system_time'     "$SYSTIME"
kv 'last_offset'     "$LASTOFF"
kv 'rms_offset'      "$RMSOFF"
kv 'frequency'       "$FREQ"
kv 'skew'            "$SKEW"
kv 'root_delay'      "$ROOTDLY"
kv 'root_dispersion' "$ROOTDISP"
kv 'update_interval' "$UPDINT"
printf '"leap":"%s"' "$(printf '%s' "$LEAP" | esc)"
printf '},'

printf '"sources":%s,'     "$SOURCES"
printf '"sourcestats":%s,' "$SRCSTATS"

printf '"ptp":{'
printf '"available":%s,' "$PTP_AVAIL"
kv 'port_state'  "$PORTSTATE"
kv 'clock_class' "$CLKCLASS"
printf '"phc_cmp":"%s"' "$(printf '%s' "$PHC_CMP" | esc)"
printf '},'

printf '"sys":{'
kv 'uptime_s'  "$UP"
kv 'load'      "$LOAD"
kn 'mem_total_kb' "$MEMTOTAL"
kn 'mem_free_kb'  "$MEMFREE"
kn 'mem_avail_kb' "$MEMAVAIL"
kn 'temp_mdeg'    "$TEMP"
kv 'net_iface'   "$IFACE"
kv 'net_state'   "$NETSTATE"
kv 'net_speed'   "$NETSPEED"
kv 'net_carrier' "$NETCARR"
printf '"ip":"%s"' "$(printf '%s' "$IP" | esc)"
printf '}'

printf '}'
