
# B compiler

> [!NOTE]
> This project is currently far from being finished or "major". It's a passion project because I can.

Compiler prototype for the B programming language using LLVM as the backend, written in C & C++.

Name obviously comes from the B language and its backend.


### Current features/progress

> [!WARNING]
> General Note: The Project is fully buildable but not usable.
>
> The compilation will fail quickly with syntax errors that are false positives.
>
> Building is also very not ideal.

- Current platform can correctly be inferred
- Cross compilation is theoretically possible
- Target can be manually overridden
- generate function's signature in IR
- generate multiple functions


### Inspiration
Idea stolen from/SHoutout to:
- https://www.youtube.com/watch?v=gS_jY8iPBv8
- https://github.com/tsoding/b


### Dependencies
- `clang`: compiler backend and linking with CRT / Clib
- `llvm`: intermediate representation

To build with build.c (nob) or Makefile:
`gcc g++ build-essential clang`


### Implemented keywords
No keywords have been implemented yet.


### How to Build

Notes for Windows: To build on Windows you have multiple options:
- Build with CMake (e.g. via Visual Studio)
- Install msys2 / Mingw and build with nob or Makefile
- Install Clang (e.g. via Visual Studio) and build with it

#### Option A:

> 1. Compile `build.c` with your favourite compiler
> 2. Run the resulting binary, it is your build "script"
> 3. It should now invoke `gcc` and `g++` to build the project.
> 4. Profit.

#### Option: B
Build with CMake:

```console
cmake .
```