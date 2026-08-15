#!/bin/bash
echo "Starting Main Server..."
./Main_Server > /dev/null 2>&1 &
SERVER_PID=$!
sleep 1

run_test() {
    CLIENTS=$1
    echo "Running with $CLIENTS concurrent clients..."
    
    # Generate payloads
    for i in $(seq 1 $CLIENTS); do
        printf "%s\n" "PUSH main 1 0000000000000000000000000000000000000000" > /tmp/push_$i.bin
        printf "%-256s" "13 file_$i.txt" >> /tmp/push_$i.bin
        printf "Content of %02d" $i >> /tmp/push_$i.bin
    done

    # Measure time to fire all connections
    start_time=$(date +%s%N)
    for i in $(seq 1 $CLIENTS); do
        cat /tmp/push_$i.bin | nc localhost 7777 > /dev/null &
    done
    wait
    end_time=$(date +%s%N)
    
    elapsed_ms=$(( (end_time - start_time) / 1000000 ))
    echo "Time for $CLIENTS clients: $elapsed_ms ms"
    rm /tmp/push_*.bin
}

run_test 1
run_test 10
run_test 50
run_test 100

kill $SERVER_PID
