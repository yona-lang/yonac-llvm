import subprocess
result = subprocess.run(["seq", "1", "10000"], capture_output=True, text=True)
print(len(result.stdout))
