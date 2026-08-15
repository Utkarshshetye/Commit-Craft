#!/bin/bash
set -e

echo "==============================================="
echo " STARTING COMMIT-CRAFT MASSIVE FILE TEST (1GB)"
echo "==============================================="

# 1. Compile
echo "[1/7] Compiling servers and client..."
gcc Main_Server.c -o Main_Server -DMAX_THREADS=8 -lpthread -lssl -lcrypto
gcc ProxyServer.c -o ProxyServer -lgit2 -lhiredis -lssl -lcrypto
gcc Client.c -o Client -lgit2 -lhiredis -lssl -lcrypto

# 2. Check Redis
echo "[2/7] Checking Redis server..."
if ! systemctl is-active --quiet redis-server && ! systemctl is-active --quiet redis; then
    echo "Warning: Redis might not be running. Attempting to start..."
    sudo systemctl start redis-server || true
fi

# 3. Setup Workspace
echo "[3/7] Setting up a clean workspace..."
killall Main_Server ProxyServer Client 2>/dev/null || true
ipcrm -a msg 2>/dev/null || true
rm -rf massive_repo
mkdir massive_repo
cp Main_Server ProxyServer Client massive_repo/
cd massive_repo

# 4. Start Daemons
echo "[4/7] Starting Main_Server and ProxyServer..."
./Main_Server > main_server.log 2>&1 &
MAIN_PID=$!
sleep 1

./ProxyServer > proxy_server.log 2>&1 &
PROXY_PID=$!
sleep 1

# 5. Create Dummy Commit so HEAD exists, then create 1GB file
echo "[5/7] Simulating Developer Workflow with Massive File (1GB)..."
time ./Client <<EOF
GIT_INIT
EOF
sleep 1
echo "dummy" > dummy.txt
time ./Client <<EOF
GIT_ADD
GIT_COMMIT
EOF
sleep 2

echo "Creating 1 Gigabyte file..."
fallocate -l 1000M massive_blob.bin

echo "Source File Created at: $(pwd)/massive_blob.bin"
ls -lh massive_blob.bin

# Feed the commands into the Client via stdin
echo "Pushing 1GB file to the Main_Server..."
time ./Client <<EOF
GIT_ADD
GIT_PUSH
GIT_COMMIT
EOF

sleep 60 # Give servers time to finish hashing and pushing 1GB

# 6. Verify Results
echo "[6/7] Verifying Remote Server Output..."
echo ""
echo "=== Main_Server Log ==="
cat main_server.log
echo ""
echo "=== ProxyServer Log ==="
cat proxy_server.log
echo ""

if [ -d ".git/objects" ] && [ "$(ls -A .git/objects)" ]; then
    echo "✅ SUCCESS: The .git/objects directory was created and populated by Main_Server!"
    echo "Files in .git/objects (Showing sizes):"
    ls -lhR .git/objects
else
    echo "❌ FAILURE: .git/objects was not populated correctly."
fi

# Check if Main_Server is still alive
if kill -0 $MAIN_PID 2>/dev/null; then
    echo "✅ Main_Server is still running (PID: $MAIN_PID)"
else
    echo "❌ Main_Server CRASHED during the test!"
fi

# 7. Cleanup
echo "[7/7] Cleaning up background processes..."
kill $MAIN_PID
kill $PROXY_PID
wait $MAIN_PID 2>/dev/null || true
wait $PROXY_PID 2>/dev/null || true

echo "==============================================="
echo " MASSIVE FILE TEST COMPLETE!"
echo "==============================================="
