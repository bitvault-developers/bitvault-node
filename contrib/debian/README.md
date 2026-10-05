
Debian
====================
This directory contains files used to package bitvaultd/bitvault-qt
for Debian-based Linux systems. If you compile bitvaultd/bitvault-qt yourself, there are some useful files here.

## bitvault: URI support ##


bitvault-qt.desktop  (Gnome / Open Desktop)
To install:

	sudo desktop-file-install bitvault-qt.desktop
	sudo update-desktop-database

If you build yourself, you will either need to modify the paths in
the .desktop file or copy or symlink your bitvault-qt binary to `/usr/bin`
and the `../../share/pixmaps/bitvault128.png` to `/usr/share/pixmaps`

bitvault-qt.protocol (KDE)

