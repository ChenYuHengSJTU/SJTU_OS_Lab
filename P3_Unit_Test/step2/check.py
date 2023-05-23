import re
import os
import subprocess
pattern = b'w file(\d) \d+ (\w+)'

ref = open('check.txt', 'r')
file_ref = ref.readlines()
file_num = int(file_ref[0].strip())
index = 0

file_content = ["" for i in range(file_num)]

with open('fs.log', 'rb') as file:
    for line in file:
        line = line.strip()
        # print(line)
        match = re.search(pattern, line)
        if match:
            groups = match.groups()
            # print(groups[0].decode())
            index = int(groups[0].decode()) - 1
            content = groups[1].decode()
            file_content[index] = content
            # file_num += 1

with open('check.test.txt', 'w') as file:
    file.write(str(file_num))
    file.write('\n')
    for i in range(file_num):
        file.write(file_content[i])
        file.write("\n")


result = subprocess.run(['diff', 'check.txt', 'check.test.txt'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)

if result.returncode == 0:
    print("Test Success!")
else:
    print("Test Fail...")

print(result.stdout.decode('utf-8'))

# for i in range(file_num):
#     if file_content[i] != file_ref[i + 1].strip():
#         print("Wrong answer %d", i)
#         exit(1)