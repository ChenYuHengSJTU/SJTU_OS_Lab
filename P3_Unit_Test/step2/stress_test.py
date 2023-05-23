import random
import string
# generate str
def RandomStr(n):
    str = ""
    for i in range(n):
        str += random.choice(string.ascii_letters + string.digits)
    return str

# generate int
# 包括【a,b】
def RandomInt(a, b):
    return random.randint(a, b)

MAX_ENTRY = 8 + 32 * 4

class Entry:
    def __init__(self, name, isDir, size, parent, child):
        self.name = name
        self.isDir = isDir
        self.size = size
        self.parent = parent
        self.child = child

    def __str__(self):
        return self.name + " " + str(self.isDir) + " " + str(self.size) + " " + str(self.parent) + " " +str(self.child)


class FileTestCase:
    def __init__(self, file, reference):
        self.file = file
        self.reference = reference

    def __str__(self):
        return self.file + "\033[36mSHOULD BE\033[0m] " + self.command

dir_perfix = "dir"
file_perfix = "file"
root = "/"
cur_dir = Entry(root, True, 0, [], [])

command = ["mkdir", "rmdir", "cd", "ls", "mk", "rm", "cat", "e", "i", "w", "d"]
file_command = ["mk", "rm", "cat", "i", "w", "d"]
dir_command = ["mkdir", "rmdir", "cd", "ls"]

def ParseOutput(str):
    print(str)    

def GetEntry():
    return random.choice(cur_dir.child)

FILE_NUM = 100
CASE_NUM = 1000
fileContent = [""] * FILE_NUM
fileExist = [False] * FILE_NUM
Reference = [""] * CASE_NUM

cnt = 0
# generate test case
def GenerateInput(flag):
    # test file
    Str = ""
    if flag is True:
        cmd = random.choice(file_command)
        Str.append(cmd)
        fd = RandomInt(0, FILE_NUM - 1)
        if cmd == "mk":
            if fileExist[fd] is True:
                return
            fileExist[fd] = True
            Reference[cnt] = ""
            cnt = cnt + 1
            Str.append(" " + file_perfix + str(fd))
            # print(str)
        elif cmd == "rm":
            if fileExist[fd] is False:
                return
            fileExist[fd] = False
            Reference[cnt] = ""
            cnt = cnt + 1
            Str.append(" " + file_perfix + str(fd))
        