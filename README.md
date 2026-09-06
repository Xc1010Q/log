cmake -G "MinGW Makefiles" -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 8
.\build\log.exe