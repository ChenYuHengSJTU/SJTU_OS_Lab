import os
size = 128

for i in range(15):
    if os.fork() == 0 :
        os.execl("./Copy", "./Copy", "copy.in", "copy.out", str(size))
    else:
        os.wait()
    print(size)
    size = size * 2