#!/bin/bash
echo "==============================================="
echo " STARTING COMMIT-CRAFT END-TO-END TEST"
echo "==============================================="

# 1. Compile the code
echo "[1/7] Compiling servers and client..."
gcc Main_Server.c -o Main_Server -DMAX_THREADS=8 -lpthread -lssl -lcrypto
gcc ProxyServer.c -o ProxyServer -lgit2 -lhiredis -lssl -lcrypto
gcc Client.c -o Client -lgit2 -lhiredis -lssl -lcrypto

# 2. Check/Start Redis
echo "[2/7] Checking Redis server..."
if ! pgrep -x "redis-server" > /dev/null
then
    echo "Starting redis-server..."
    redis-server --daemonize yes
    sleep 1
fi

# 3. Setup Test Directory
echo "[3/7] Setting up a clean workspace..."
rm -rf test_repo
mkdir test_repo
cp Main_Server ProxyServer Client test_repo/
cd test_repo

# 4. Start Daemons
echo "[4/7] Starting Main_Server and ProxyServer..."
./Main_Server &
MAIN_PID=$!
sleep 1

./ProxyServer &
PROXY_PID=$!
sleep 1

# 5. Create a file and feed commands to Client
echo "[5/7] Simulating Developer Workflow with Large File (50MB)..."
dd if=/dev/urandom of=large_blob.bin bs=1M count=50

# Feed the commands into the Client via stdin
./Client <<EOF
GIT_INIT
GIT_ADD
GIT_PUSH
GIT_COMMIT
EOF

sleep 2 # Give servers time to process the pushed files

# 6. Verify Results
echo "[6/7] Verifying Remote Server Output..."
if [ -d ".git/objects" ] && [ "$(ls -A .git/objects)" ]; then
    echo "✅ SUCCESS: The .git/objects directory was created and populated by Main_Server!"
    echo "Files in .git/objects (Showing sizes):"
    ls -lhR .git/objects
else
    echo "❌ FAILURE: .git/objects was not populated correctly."
fi

# 7. Cleanup
echo "[7/7] Cleaning up background processes..."
kill $MAIN_PID
kill $PROXY_PID
wait $MAIN_PID 2>/dev/null
wait $PROXY_PID 2>/dev/null

echo "==============================================="
echo " END-TO-END TEST COMPLETE!"
echo "==============================================="
