#!bin/sh
cd builddir
meson compile
gdbserver :8555 ./SocketLibMain