#!/bin/bash
set -euo pipefail

CONF="/etc/dhcpcd.conf"

backup() {
  sudo cp "$CONF" "$CONF.bak.$(date +%F_%H%M%S)"
}

set_static_ip_if_missing() {
  local iface="$1" ip_cidr="$2" gw="$3" dns="$4"

  if grep -q "static ip_address=${ip_cidr}" "$CONF"; then
    echo "${iface}: Static IP ${ip_cidr} already set."
    return 0
  fi

  echo "${iface}: Setting static IP ${ip_cidr}."
  backup

  cat <<EOF | sudo tee -a "$CONF" >/dev/null

interface ${iface}
static ip_address=${ip_cidr}
static routers=${gw}
static domain_name_servers=${dns}
EOF

echo "${iface}: Static IP ${ip_cidr} set."
}

set_static_ip_if_missing "eth0"  "192.168.72.1/24" "192.168.1.1" "192.168.1.1 8.8.8.8"
set_static_ip_if_missing "wlan0" "192.168.72.101/24" "192.168.1.1" "192.168.1.1 8.8.8.8"

### Add arm to gui hosts
HOSTS="/etc/hosts"
IP="192.168.72.102"
NAME="OdinArm"

if grep -qE "^\s*${IP}\s+${NAME}(\s|$)" "$HOSTS"; then
    echo "${IP} ${NAME} already in $HOSTS"
else
    echo "Adding ${IP} ${NAME} to $HOSTS"
    echo "${IP} ${NAME}" | sudo tee -a "$HOSTS" >/dev/null
    echo "${NAME} ${IP} added to $HOSTS"
fi