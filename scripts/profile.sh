#!/bin/sh
cd profilebuilddir
meson compile &&
#pprof --pdf ./SocketLibMain SocketLibMain.prof > SocketLibMain.pdf
#google-pprof --svg ./SocketLibMain SocketLibMain.prof > SocketLibMain.pd
#gperf ./SocketLibMain
CPUPROFILE=./SocketLibMain.prof ./SocketLibMain
google-pprof --pdf ./SocketLibMain SocketLibMain.prof > SocketLibMain.pdf
google-pprof --web ./SocketLibMain SocketLibMain.prof > SocketLibMain.html