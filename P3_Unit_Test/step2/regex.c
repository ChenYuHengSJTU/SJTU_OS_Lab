#include <regex.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(){
    char str[1024] = "";

    char* regex[]={
        "(f)\n",
        "(mk) (\\w+)\n",
        "(rm) (\\w+)\n",
        "(cd) ([a-zA-Z./0-9]+)\n",
        "(w) (\\w+) (\\d+)\n"
    };    

    regex_t reg;
    // int ret;
    regmatch_t match[4];


    if(regcomp(&reg, regex[4], REG_EXTENDED) != 0){
        printf("regcomp error\n");
        exit(1);
    }

    printf("subs: %d\n", reg.re_nsub);

    while(1){
        fgets(str, 1024, stdin);
        if(str[0] == '\n') break;
        if(regexec(&reg, str, 3, match, 0) == 0){
            printf("match\n");
        }
        else{
            printf("not match\n");
            char err[1024];
            regerror(regexec(&reg, str, 3, match, 0), &reg, err, 1024);
            printf("%s\n", err);
            continue;
        }
        for (int i = 0; i <= 2 && match[i].rm_so != -1; i++) {
            printf("match[%d]: %.*s\n", i, match[i].rm_eo - match[i].rm_so, str + match[i].rm_so);
            printf("so: %s\n", str + match[i].rm_so);
            printf("eo: %s\n", str + match[i].rm_eo);
        }
        memset(str, 0, sizeof(str));
    }
}

// #include <regex.h>
// #include <stdio.h>

// int main() {
//     // 待匹配的字符串和正则表达式
//     char *input = "Hello, world! You are awesome.";
//     char *pattern = "(\\w+), (\\w+)! You are (\\w+)\\.";

//     // 编译正则表达式
//     regex_t re;
//     if (regcomp(&re, pattern, REG_EXTENDED) != 0) {
//         printf("error: failed to compile regex\n");
//         return 1;
//     }

//     // 执行匹配
//     regmatch_t match[4];
//     if (regexec(&re, input, 4, match, 0) != 0) {
//         printf("no match\n");
//         return 0;
//     }

//     // 输出完整匹配结果和各个子表达式的匹配结果
//     for (int i = 0; i <= 3 && match[i].rm_so != -1; i++) {
//         printf("match[%d]: %.*s\n", i, match[i].rm_eo - match[i].rm_so, input + match[i].rm_so);
//     }

//     // 释放正则表达式对象
//     regfree(&re);

//     return 0;
// }
