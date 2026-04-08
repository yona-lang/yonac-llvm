# Parallel exec to match Yona's let-parallelism
from concurrent.futures import ThreadPoolExecutor
import subprocess

def exec_len(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    return len(r.stdout.strip())

with ThreadPoolExecutor(3) as ex:
    futures = [ex.submit(exec_len, ["echo", w]) for w in ["hello", "world", "yona"]]
    print(sum(f.result() for f in futures))
