import os
import subprocess
import sys 
import signal
import re

n = int(sys.argv[1])
dug = 0
seed = 0

# print(n)
arg = ["./LCM", sys.argv[1]]
stdout = 0
strerr = 0

timelimit = int(sys.argv[2])

proc = subprocess.Popen(arg, bufsize = 0, stdout = subprocess.PIPE, stdin = subprocess.PIPE, stderr=subprocess.PIPE)
try:
    stdout ,stderr = proc.communicate(timeout = timelimit)
    proc.wait(timeout = timelimit)
except TimeoutError:
    proc.send_signal(signal.SIGTERM)
    stdout , stderr = proc.communicate(timeout = timelimit)
    proc.kill()
    proc.wait()
except subprocess.TimeoutExpired:
    proc.send_signal(signal.SIGTERM)
    stdout , stderr = proc.communicate(timeout = timelimit)
    proc.kill()
    proc.wait()
    
# print(stdout.decode('utf-8'))    
stdout = stdout.decode('utf-8')
# print(len(stdout))
output = stdout.splitlines()

for line in output:
    i = re.findall(r"\d+",line)
    # print(i)
    if line[0] == 'L':
        dug += 1
        assert(dug <= n)
    elif line[0] == 'M':
        seed += 1
        dug -= 1
        assert(dug >= 0)
        assert(seed <= n)
    else:
        assert(seed >= 1)
        seed -= 1
        assert(seed >= 0)

print("OK")