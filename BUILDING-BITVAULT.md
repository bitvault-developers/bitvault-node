# Building BitVault Core (bitvaultd)

Linux x86_64 (tested on Ubuntu 22.04 / 24.04). Ready-made binaries are on the Releases page; verify them with `sha256sum -c SHA256SUMS`.

## 1. System packages
```
sudo apt-get install -y build-essential libtool autotools-dev automake pkg-config bsdmainutils python3 libevent-dev libboost-dev libsqlite3-dev libssl-dev libzmq3-dev cmake git
```

## 2. RandomX (installed to /usr/local)
```
git clone https://github.com/tevador/RandomX && cd RandomX && mkdir build && cd build
cmake .. && make -j"$(nproc)" && sudo make install
```

## 3. liboqs (built in ~/liboqs/build, static)
```
git clone https://github.com/open-quantum-safe/liboqs ~/liboqs && cd ~/liboqs && git checkout 0.15.0
mkdir build && cd build && cmake -DBUILD_SHARED_LIBS=OFF .. && make -j"$(nproc)"
```

## 4. BitVault Core
```
./autogen.sh
./configure --enable-static --disable-shared --with-gui=no LDFLAGS="-static-libstdc++ -static-libgcc" LIBS=-lcrypto --with-pic --enable-benchmark=no --enable-module-recovery --disable-module-ecdh --disable-tests --disable-bench --disable-fuzz-binary
make -j"$(nproc)"
```
Binaries: src/bitvaultd, src/bitvault-cli, src/bitvault-tx, src/bitvault-wallet, src/bitvault-util.

## Network
Mainnet since 28 August 2026 · multi-algorithm proof of work · 60-second blocks. Official site, explorer and supply: https://bitvault.club/listings/
