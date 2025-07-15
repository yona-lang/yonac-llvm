#!/bin/bash
cd /mnt/c/Users/kovar/source/repos/yona-lang/yonac-llvm/out/build/x64-debug-linux

echo "Testing if tests hang..."
echo "Running InterpreterBasicTest.AdditionInt"

timeout 5s ./tests --gtest_filter="InterpreterBasicTest.AdditionInt"

if [ $? -eq 124 ]; then
    echo "Test hung and was killed after 5 seconds"
else
    echo "Test completed with exit code: $?"
fi
