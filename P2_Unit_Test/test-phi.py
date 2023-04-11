import os
import subprocess
import sys 
import signal
import re

arg = ["./Phi"]

timelimit = 10 
t = 0
while True:
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
    if len(output) < 1000000:
        print(output[len(output) - 100:])
        break
    t = t + 1
    print("good for ", str(t))

