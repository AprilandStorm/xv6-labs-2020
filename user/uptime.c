//uptime.c : 使用uptime系统调用以滴答为单位打印正常运行时间

#include"kernel/types.h"
#include"kernel/stat.h"
#include"user/user.h"

int main(){
    printf("%d\n", uptime());
    exit(0);
}