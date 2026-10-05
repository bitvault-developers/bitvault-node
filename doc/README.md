BitVault Core
=============

Setup
---------------------
BitVault Core is the original BitVault client and it builds the backbone of the network. It downloads and, by default, stores the entire history of BitVault transactions, which requires a few hundred gigabytes of disk space. Depending on the speed of your computer and network connection, the synchronization process can take anywhere from a few hours to a day or more.

To download BitVault Core, visit [bitvault.org](https://bitvault.org/en/download/).

Running
---------------------
The following are some helpful notes on how to run BitVault Core on your native platform.

### Unix

Unpack the files into a directory and run:

- `bin/bitvault-qt` (GUI) or
- `bin/bitvaultd` (headless)

### Windows

Unpack the files into a directory, and then run bitvault-qt.exe.

### macOS

Drag BitVault Core to your applications folder, and then run BitVault Core.

### Need Help?

* See the documentation at the [BitVault Wiki](https://dgbwiki.com/)
for help and more information.
* Ask for help on [BitVault-Core](https://gitter.im/BitVault-Core) Gitter.
* Ask for help on the [BitVault Official Discussion](https://t.me/BitVaultCoin) Telegram channels.
* Ask for help on #bitvault on Libera Chat. If you don't have an IRC client, you can use [web.libera.chat](https://web.libera.chat/#bitvault).


Building
---------------------
The following are developer notes on how to build BitVault Core on your native platform. They are not complete guides, but include notes on the necessary libraries, compile flags, etc.

- [Dependencies](dependencies.md)
- [macOS Build Notes](build-osx.md)
- [Unix Build Notes](build-unix.md)
- [Windows Build Notes](build-windows.md)
- [FreeBSD Build Notes](build-freebsd.md)
- [OpenBSD Build Notes](build-openbsd.md)
- [NetBSD Build Notes](build-netbsd.md)
- [Android Build Notes](build-android.md)
- [Gitian Building Guide (External Link)](https://github.com/bitvault-core/docs/blob/master/gitian-building.md)

Development
---------------------
The BitVault repo's [root README](/README.md) contains relevant information on the development process and automated testing.

- [Developer Notes](developer-notes.md)
- [Productivity Notes](productivity.md)
- [Release Notes](release-notes.md)
- [Release Process](release-process.md)
- [Source Code Documentation (External Link)](https://doxygen.bitvault.org/)
- [Translation Process](translation_process.md)
- [Translation Strings Policy](translation_strings_policy.md)
- [JSON-RPC Interface](JSON-RPC-interface.md)
- [Unauthenticated REST Interface](REST-interface.md)
- [Shared Libraries](shared-libraries.md)
- [BIPS](bips.md)
- [Dnsseed Policy](dnsseed-policy.md)
- [Benchmarking](benchmarking.md)

### Resources
* Discuss on the [BitVaultTalk](https://bitvaulttalk.org/) forums, in the [Development & Technical Discussion board](https://bitvaulttalk.org/index.php?board=6.0).
* Discuss project-specific development on #bitvault-core-dev on Libera Chat. If you don't have an IRC client, you can use [web.libera.chat](https://web.libera.chat/#bitvault-core-dev).

### Miscellaneous

- [Assets Attribution](assets-attribution.md)
- [bitvault.conf Configuration File](bitvault-conf.md)
- [Files](files.md)
- [Fuzz-testing](fuzzing.md)
- [I2P Support](i2p.md)
- [Init Scripts (systemd/upstart/openrc)](init.md)
- [PSBT support](psbt.md)
- [Reduce Memory](reduce-memory.md)
- [Reduce Traffic](reduce-traffic.md)
- [Tor Support](tor.md)
- [ZMQ](zmq.md)

License
---------------------
Distributed under the [MIT software license](/COPYING).
