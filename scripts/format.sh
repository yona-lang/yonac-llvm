#!/usr/bin/env sh

echo "Formatting source and header files"
clang-format -i include/* src/* test/* cli/*
echo "Formatting grammar files"
npm run antlr-format
echo "Done"
