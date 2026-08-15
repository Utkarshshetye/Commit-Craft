#!/bin/bash
echo "Starting redis-server..."
redis-server > /dev/null 2>&1 &
REDIS_PID=$!
sleep 1

echo "Starting Main_Server..."
rm -rf .git
./Main_Server > server_log.txt 2>&1 &
SERVER_PID=$!

echo "Starting ProxyServer..."
./ProxyServer > proxy_log.txt 2>&1 &
PROXY_PID=$!
sleep 1

echo "Generating 1GB file (massive.bin)..."
fallocate -l 1G massive.bin

echo "Sending commands to Client..."
echo -e "GIT_INIT\nGIT_ADD\nGIT_COMMIT\nGIT_PUSH\n" | ./client

echo "Waiting for ProxyServer to finish processing..."
sleep 20 # Wait for libgit2 hashing + network push

echo "-----------------------------------"
echo "Server Branch Head:"
cat .git/refs/branches/main
echo ""

echo "-----------------------------------"
echo "Cleaning up..."
kill $SERVER_PID
kill $PROXY_PID
kill $REDIS_PID
rm massive.bin
