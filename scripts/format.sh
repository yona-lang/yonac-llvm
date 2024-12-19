#!/usr/bin/env sh

echo "Formatting source and header files"
clang-format -i include/* src/* test/* cli/*
echo "Formatting grammar files"
#java -jar lib/antlr-formatter-1.2.2.jar --dir=parser/
npm run antlr-format
echo "Done"
