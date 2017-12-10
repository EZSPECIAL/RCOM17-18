#!/bin/bash

ifconfig eth0 down
ifconfig eth1 down

num1=$(expr 10 \* $1)
num2=$(expr 1 + 10 \* $1)

ifconfig eth0 172.16.$num1.254/24
ifconfig eth1 172.16.$num2.253/24

echo 1 > /proc/sys/net/ipv4/ip_forward
echo 0 > /proc/sys/net/ipv4/icmp_echo_ignore_broadcasts

route -n
