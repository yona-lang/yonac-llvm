#!/usr/bin/env sh

echo "Formatting source and header files"
clang-format -i include/*.h src/*.cpp test/*.cpp cli/*.cpp
echo "Done"
