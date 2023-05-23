# command = ["ls", "mk f1", "ls", "w f1 10 hellojieli", "cat f1", "e"]

# command = ["mk f1", "w f1 10 hellojieli", "cat f1", "e"]
# for i in command:
#     print(i)
import sys 
import string
import random
import time
import re

def generate_random_string(length):
    printable = string.ascii_letters + string.digits + string.punctuation
    random_string = ''.join(random.choices(string.ascii_letters, k=length))
    return random_string

def generate_random_length(n):
    return random.randint(1, n)


command = ["i", "w", "d"]
argc = [5, 4, 4]

filename_perfix = "file"
filename = [""]

Argc = len(sys.argv)
file_num = 1

if Argc > 1:
    file_num = int(sys.argv[1])

for i in range(file_num):
    filename.append(filename_perfix + str(i+1))
    print("mk " + filename[i+1])
    
file_content = [""]

for i in range(file_num):
    file_content.append("")

def Write(index, length, content):
    file_content[index] = ""
    written = min(length, len(content))
    file_content[index] = content[:written]

def Insert(index, pos, l, content):
    if pos > len(file_content[index]):
        return
    tmp = file_content[index]
    file_content[index] = tmp[:pos] + content + tmp[pos:]
    
def Delete(index, pos, l):
    if pos > len(file_content[index]):
        return
    tmp = file_content[index]
    file_content[index] = tmp[:pos]
    if pos + l > len(tmp):
        return
    file_content[index] += tmp[pos+l:]

def List():
    print("ls")
    for i in range(file_num):
        print("cat" + " " + filename[i+1])

def Check():
    for i in range(file_num):
        if len(file_content[i]) == 0:
            content = generate_random_string(1000)
            Write(i, 1000, content)
            print("w" + " " + filename[i+1] + " " + str(1000) + " " + content)

def Generate_Output(cmd, index, pos, l, content):
    if cmd == "i":
        return "i" + " " + filename[index] + " " + str(pos) + " " + str(l) + " " + content
    elif cmd == "w":
        return "w" + " " + filename[index] + " " + str(l) + " " + content
    elif cmd == "d":
        return "d" + " " + filename[index] + " " + str(pos) + " " + str(l)

# First write something to the files
for i in range(file_num):
    # length = generate_random_length(1000)
    length = 1000
    content = generate_random_string(length)
    Write(i, length, content)
    cmd_line = str(Generate_Output("w", i + 1, 0, length, content))
    print(cmd_line)
    
List()

test_num = 3
for i in range(1, test_num):
    for j in range(file_num):
        # length = generate_random_length(1000)
        length = 1000
        content = generate_random_string(length)
        # print(content)
        if len(file_content[j]) != 0:
            pos = generate_random_length(len(file_content[i]))
        # pos = random.randint(0, len(file_content[j]))
            l = generate_random_length(len(file_content[j]))
        else:
            pos = 0
            l = 0
        # cmd = random.choices(command, k=1)
        if i % 3 != 0:
            cmd = "i"
        else:
            cmd = "d"
        # cmd = "w"
        # cmd_line = Generate_Output(cmd, i, pos, l, content)
        cmd_line= ""
        cmd_line = str(Generate_Output(cmd, j + 1, pos, l, content))
        # cmd_line += "\n"
        print(cmd_line)
    List()
        # time.sleep(secs=1)
        # Check()
List()
print("e")

with open('check.txt', 'w') as file:
    file.write(str(file_num))
    file.write('\n')
    for i in range(file_num):
        file.write(file_content[i])
        file.write("\n")

# pattern = b'w file\d \d+ ([\x20-\x7E]+)'

# # try:
# #     fs = open('fs.log', 'rb')
# # except: 
# #     print("No such file or directory: 'fs.log'")
# #     exit(1)

# with open('fs.log', 'rb') as file:
#     for line in file:
#         line = line.strip()
#         print(line)
#         match = re.search(pattern, line)
#         if match:
#             groups = match.groups()
#             print(groups[0])
#         else:
#             print("No match")
        