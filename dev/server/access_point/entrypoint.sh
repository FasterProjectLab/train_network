#!/bin/sh

envsubst < hostapd.conf.tmpl > /etc/hostapd.conf
envsubst < dnsmasq.conf.tmpl > /etc/dnsmasq.conf

ip link set ${WIFI_INTERFACE} up
ip addr flush dev ${WIFI_INTERFACE}
ip addr add ${WIFI_IP}/24 dev ${WIFI_INTERFACE}

dnsmasq -C /etc/dnsmasq.conf
exec hostapd /etc/hostapd.conf