
# B compiler

This project is currently far from being finished.

Compiler prototype for the B programming language using LLVM as the backend, written in C & C++.

Name obviously comes from the B language and its backend.

Inspired by
- https://github.com/tsoding/b
- https://www.youtube.com/watch?v=gS_jY8iPBv8&t=11910s


Dependencies
- clang: compiler backend and linking with CRT / Clib
- llvm: intermediate representation


Current features/progress:
- Acceptes some target triples to stick them into
- Can generate function's define in IR
- generate multiple functions
- Can munch on function parameter list


Implemented keywords:
- return