import subprocess

# p = subprocess.Popen('./fs', stdout=subprocess.PIPE)
# # p.stdin.write(b'ls\n')
# # p.stdin.flush()
# # p.wait(1)
# out, err = p.communicate(input='ls/n'.encode())
# print(out.decode())

# p = subprocess.Popen('./fs', stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
# p = subprocess.Popen('./fs', stdout=subprocess.PIPE, stderr=subprocess.PIPE)
p = subprocess.Popen('./fs', stdin = subprocess.PIPE)
# p.stdin.write(b'ls\n')
# p.stdin.flush()
out, err = p.communicate()
print("communicating...")
out, err = p.communicate(input='ls\n'.encode())
print("communicating...")
print(out.decode())
# p.stdin.close()
# p.wait()
p.stdin.write(b'mk f1\n')
p.stdin.flush()

p.stdin.write(b'ls\n')
p.stdin.flush()
p.stdin.write(b'e\n')
p.stdin.flush()
# p.stdin.close()
# out, err = p.communicate()
# print(out.decode())
p.wait()
# p.sleep(1)
# p.terminate()