#!/bin/bash
rm -rf .git
echo "Starting Main Server..."
./Main_Server > server_log.txt 2>&1 &
SERVER_PID=$!
sleep 1

echo "Simulating 50 concurrent developers pushing at the exact same millisecond..."
for i in {1..50}; do
  (
    # Create unique dummy file payload
    printf "%s\n" "PUSH main 1 0000000000000000000000000000000000000000" > push_$i.bin
    printf "%-256s" "13 file_$i.txt" >> push_$i.bin
    printf "Content of %02d" $i >> push_$i.bin
    
    # Send instantly to socket
    cat push_$i.bin | nc localhost 7777 > resp_$i.txt
  ) &
done

# Wait for all 50 background threads to finish
wait

echo "-----------------------------------"
SUCCESS_COUNT=$(grep -l "SUCCESS" resp_*.txt | wc -l)
CONFLICT_COUNT=$(grep -l "Conflict" resp_*.txt | wc -l)

echo "Total Pushes Sent: 50"
echo "Successful Fast-Forwards (Winners): $SUCCESS_COUNT"
echo "Rejected Conflicts (Losers): $CONFLICT_COUNT"
echo ""
echo "Final Branch Head:"
cat .git/refs/branches/main
echo "-----------------------------------"

# Cleanup
kill $SERVER_PID
rm push_*.bin resp_*.txt server_log.txt
