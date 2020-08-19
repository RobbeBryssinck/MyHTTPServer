#!/bin/sh

gcc -o server server.c
sudo chown root ./server
sudo chmod u+s ./server
