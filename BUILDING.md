# Building locally

The public repository contains the project configuration and authored enhancement mods. Game-derived C is regenerated from your own supported US SCUS-94448 disc and is never committed.

Initialize the pinned framework and UI:

```sh
git submodule update --init --recursive
```

Use Python 3 and a native C/C++ toolchain with CMake and Ninja. Generate the game from your extracted cue/bin pair:

```sh
python psxrecomp/psxrecomp_cli.py generate --config game.toml --project-root . --disc "path/to/your.cue"
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DPSXRECOMP_REQUIRE_GAME_C=ON
cmake --build build-release --target psx-runtime
```

The framework CLI prepares the disc and builds its emitters. On Windows, use native executable paths when MSYS shims are on PATH. The local prototype was built with MinGW GCC. On Linux, use `python3` if needed.

Launch the resulting executable from its build directory. Its adjacent `assets`, `bios` and `mods` folders are part of the runtime. The CMake target stages the US cover as `assets/img/boxart.tga`.

The focused timing callback check can be run with a C99 compiler:

```sh
cc -std=c99 -Wall -Wextra -Werror -Ipsxrecomp/runtime/include tools/test_timing.c -o build-release/test-timing
build-release/test-timing
```

The release wrapper requires both the cover and its source credit. It includes the staged launcher assets and ships `BOXART_SOURCE.txt` alongside the player README. Build each platform from your own local disc, then run `scripts/package_release.sh build-release windows-x64 <recompiler-build>` or the corresponding `build-linux linux-x64` command. No generated game C or private disc input is uploaded to the source repository.
