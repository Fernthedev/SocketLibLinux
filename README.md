# SocketLib for Linux (designed for Quest)

This is a fully asynchronous library for sockets, made because _**some people**_ (cough, you know who you are, cough) were making custom web HTTP servers which is in my opinion absurd.

# Features:
- Makes use of [BlockConcurrentQueue](https://github.com/cameron314/concurrentqueue) which can be accessed in `queue` of the shared folder
- Very lightweight and simple (though spaghetti code needs fixing)
- Each client gets its own dedicated read/write thread
- The entire project was designed to be memory efficient (which may seem to overcomplicate some things)
    - The entire lifetime of objects managed by SocketHandler, which also handles the event loop.
    - The ServerSocket is created by the SocketHandler, and will only live as long as both SocketHandler lives and ServerSocket is not freed manually. If SocketHandler dies first, UB will occur and likely fail.
# Usage:
For now, look at [the test file](test/src/main.cpp)

## Testing in Linux/WSL
To test, you must have:
- Meson
- Ninja
- Clang
- (optional) gdbserver

Setup meson using `meson setup builddir` then `cd builddir`.

Once you've done that, compile using `meson compile` and run `./SocketLibMain` which is the entry point for the shared library `libsocket_lib.so`.

## Testing in Quest (or any Android device that can run bs-hooks and sc2ad's modloader)
Run `powershell .\build.ps1` and copy `libsocket_lib.so` to your `mods` or `libs` folder.

You should only copy to `mods` if you want to run the test and not as a library.