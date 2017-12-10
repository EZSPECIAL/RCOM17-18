#!/bin/bash

ifconfig eth0 down
ifconfig eth1 down

num1=$(expr 10 \* $1)
num2=$(expr 1 + 10 \* $1)

ifconfig eth0 172.16.$num2.1/24

route add -net 172.16.$num1.0/24 gw 172.16.$num2.253
route add -net default gw 172.16.$num2.254
route -n
