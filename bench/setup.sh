#!/bin/bash
# Generate benchmark test data files.
# Run once before benchmarking: bash bench/setup.sh

set -e
DATA_DIR="bench/data"
mkdir -p "$DATA_DIR"

echo "Generating benchmark test data..."

# 50MB text file (realistic log/CSV size)
if [ ! -f "$DATA_DIR/large_text.txt" ]; then
    echo "  large_text.txt (50MB)..."
    python3 -c "
import random, string
with open('$DATA_DIR/large_text.txt', 'w') as f:
    for i in range(500000):
        line = ''.join(random.choices(string.ascii_letters + string.digits + ' ', k=random.randint(50, 150)))
        f.write(f'{i:08d} {line}\n')
"
fi

# 4 separate 10MB files for parallel read (different content = different pages)
for i in 1 2 3 4; do
    if [ ! -f "$DATA_DIR/chunk_${i}.txt" ]; then
        echo "  chunk_${i}.txt (10MB)..."
        python3 -c "
import random, string
with open('$DATA_DIR/chunk_${i}.txt', 'w') as f:
    for j in range(100000):
        line = ''.join(random.choices(string.ascii_letters + ' ', k=random.randint(60, 120)))
        f.write(line + '\n')
"
    fi
done

# 5MB binary file for binary I/O benchmarks
if [ ! -f "$DATA_DIR/bench_binary.bin" ]; then
    echo "  bench_binary.bin (5MB)..."
    dd if=/dev/urandom of="$DATA_DIR/bench_binary.bin" bs=1M count=5 2>/dev/null
fi

# Summary
echo "Done."
ls -lh "$DATA_DIR"/*
