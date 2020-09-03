#!/bin/sh

gcc vuln_server.c -o vuln_server -fno-stack-protector -z execstack -no-pie
sudo chown root ./server
sudo chmod u+s ./server
