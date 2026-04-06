with open("bench/data/bench_text.txt") as f:
    count = sum(1 for _ in f)
print(count)
