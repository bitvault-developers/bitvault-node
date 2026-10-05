#!/bin/bash
echo "=== BitVault 3-Node Local Testnet ==="
for i in 1 2 3; do
    mkdir -p /tmp/bvt-node$i
    PORT=$((29432 + i))
    RPCPORT=$((18442 + i))
    cat > /tmp/bvt-node$i/bitvault.conf << CONF
regtest=1
rpcuser=bitvault
rpcpassword=CHANGE_ME
rpcport=$RPCPORT
port=$PORT
listen=1
CONF
    for j in 1 2 3; do
        [ $j -ne $i ] && echo "connect=127.0.0.1:$((29432+j))" >> /tmp/bvt-node$i/bitvault.conf
    done
    bitvaultd -regtest -datadir=/tmp/bvt-node$i -daemon
    echo "Node $i: RPC=$RPCPORT P2P=$PORT"
done
sleep 15
for i in 1 2 3; do
    RPCPORT=$((18442 + i))
    PEERS=$(bitvault-cli -regtest -rpcport=$RPCPORT -rpcuser=bitvault -rpcpassword=CHANGE_ME getpeerinfo 2>/dev/null | grep -c '"addr"')
    echo "Node $i: $PEERS peers"
done
