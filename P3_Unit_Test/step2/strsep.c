#include <stdio.h>
#include <string.h>

int main() {
    char str[] = "hello world how are you";
    char delimiters[] = " ";

    char *pstr = str;
    char *sub_str;

    while ((sub_str = strsep(&pstr, delimiters)) != NULL) {
        printf("Sub-string: %s\n", sub_str);
    }

    return 0;
}
