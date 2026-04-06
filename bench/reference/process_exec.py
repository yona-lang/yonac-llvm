import subprocess
a = subprocess.run(["echo", "hello"], capture_output=True, text=True)
b = subprocess.run(["echo", "world"], capture_output=True, text=True)
c = subprocess.run(["echo", "yona"], capture_output=True, text=True)
print(len(a.stdout.strip()) + len(b.stdout.strip()) + len(c.stdout.strip()))
