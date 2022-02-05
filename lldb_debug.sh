#!/bin/sh
cd builddir
meson compile &&
lldb-server g :8555 ./SocketLibMain