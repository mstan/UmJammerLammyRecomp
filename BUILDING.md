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

The maintained release workflow requires both the cover and its source credit,
retains README screenshot assets, and packages OpenBIOS, the title mod catalog,
overlay compiler fallback and notices. It freshly regenerates the game from your
original boot EXE and runs the selective PGO build phases with a private muted
boot and normal shutdown. Profiles are build inputs, not gameplay qualification.

```powershell
scripts/package_release.ps1 --channel public --platform windows --jobs 2
```

```sh
scripts/package_release.sh --channel public --platform linux --jobs 2
```

The channel is required. Each invocation uses its own short build/profile
workspace. No disc, private settings, cards, snapshots, generated game C or
compiler profiles are shipped. Linux uses the shared AppImage dependency gates.
