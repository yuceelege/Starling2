#!/bin/bash
echo "Starting mavsdk server on port 50051, listening for"
nohup mavsdk_server -p 50051 udp://:14551 > mavsdk_server.log 2>&1 &

