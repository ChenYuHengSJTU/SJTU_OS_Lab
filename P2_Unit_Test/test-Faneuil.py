import os
import subprocess
import sys
import signal
import re

arg = ["./Faneuil"]
stdout = bytes
stderr = bytes

timelimit = int(sys.argv[1])

enter_able = 1
leave_able = 1
entered = 0
checked = 0
sit = 0
confirmed = 0

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
    
stdout = stdout.decode('utf-8')
output = stdout.splitlines()

# print(stdout)

for line in output:
    i = re.findall(r"\d+", line)
    if len(i) == 0:
        continue
    str = line[len(line) - 3:]
    if line[0] == 'I':
        if str == 'ter':
            assert enter_able == 1 ,[i, line , '\n' ,output[0: output.index(line) + 1]]
            assert(entered >= 0)
            entered += 1
        elif str == "kin":
            assert(checked >= 0 and checked <= entered)
            checked += 1
            assert(checked <= entered)
        elif str == "own":
            sit += 1
            assert(sit <= checked)
        elif str == 'ave':
            assert(leave_able == 1)
    elif line[0] == 'J':
        if str == 'ter':
            enter_able = 0
            leave_able = 0
        elif str == 'ave':
            enter_able = 1
            leave_able = 1
        else:
            assert(sit == entered)
            entered = 0
            checked = 0
            sit = 0
    else:
        if str == "ter":
            assert(enter_able == 1)

    
print("OK")            
    