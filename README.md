# Yet Another Roblox Bootstrapper

YARB (Yet Another Roblox Bootstrapper) is a custom Roblox bootstrapper aimed at being feature-rich, fast and easy-to-use.


## Features
- GUI to manage both FastFlags and modifications
- Easy Flags - A user-friendly way to configure FastFlags
- File integrity - Ensures that no files are corrupt
- Efficient updates - Only download packages which changed
- Query Roblox server location
- Discord Rich Presence

## Prerequisites

- Meson
- A C++23 capable compiler
- libzip
- libcurl
- OpenSSL
- nlohmann_json
- OpenGL 3

## Compiling

1. Clone the repository: `git clone https://github.com/yooksch/yarb.git`
2. Configure Meson: `meson setup build`
3. Compile: `meson compile -C build`


> [!NOTE]
> YARB is not affiliated with Roblox in any way.