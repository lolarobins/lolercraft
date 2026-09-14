# lolercraft mc server implementation
custom notchian server protocol implementation, mainly just to have fun
would not use this lol, but i just want the code out there
note: intent is to be only for creative play with compatible/custom behaviour redstone

## features
### current
- simple dynamic string and buffer library
- json parser
- status and login handshake
- protocol encryption with authentication
### planned
- nbt parser
- simple world generation
- text component library
- mod support (using shared libs w/ dlopen)
- concise api that allows easy use for internal code + mods

## requirements
currently only works on systems with POSIX libraries
written and tested on macos arm64 with clang, not yet tested on linux, WSL, or any BSD
### dependencies
- openssl
- libcurl
### building
- `git clone https://github.com/lolarobins/lolercraft`
- `cd lolercraft`
- `make`
