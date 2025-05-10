//user/sleep.c
#include"kernel/types.h"//定义了一些基本类型
#include"kernel/stat.h"//包含文件属性信息
#include"user/user.h"//包含xv6用户空间的系统调用接口

int main(int argc, char **argv){//第一个参数表示命令行参数的个数；第二个参数表示一个指向参数字符串数组的指针
    if(argc < 2){//如果用户没有输入正确参数，打印提醒
        printf("usage: sleep <ticks>\n");
    }
    sleep(atoi(argv[1]));//将argv[1]字符串转换为整数，然后调用sleep系统调用，它让进程挂起argv[1]个时钟周期，然后程序会被调用恢复执行
    exit(0);
}