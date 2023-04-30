#include <stdio.h>
#include <time.h>

int main() {
    time_t now;
    struct tm *local_time;

    // 获取当前时间
    now = time(NULL);

    // 将时间格式化为本地时间
    local_time = localtime(&now);

    printf("当前时间是：%04d年%02d月%02d日 %02d:%02d:%02d\n",
           local_time->tm_year + 1900, local_time->tm_mon + 1, local_time->tm_mday,
           local_time->tm_hour, local_time->tm_min, local_time->tm_sec);

    return 0;
}
