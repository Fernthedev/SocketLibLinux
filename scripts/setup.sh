meson setup builddir --buildtype debug
meson setup buildrelease --buildtype release
meson setup profilebuilddir -Denable_profiling=true
