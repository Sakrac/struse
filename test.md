# CMake build with LLVM Clang

From the repository root, configure and build the project with LLVM Clang:

```powershell
clang --version
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build --parallel
```

This uses the LLVM Clang compiler for both C and C++ builds.
