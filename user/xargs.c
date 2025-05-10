//xargs.c : 从标准输入读入数据，将每一行当作参数，传给xargs的程序名后面的参数，后面的参数作为命令运行传入的参数

#include"kernel/types.h"
#include"kernel/stat.h"
#include"user/user.h"
#include"kernel/fs.h"

//运行指定的程序，接收参数
void run(char* program, char** args){
    //创建子进程，在子进程中执行指定程序
    if(fork() == 0){
        exec(program, args);
        exit(0);
    }
    return;
}

int main(int argc, char** argv){
    char buf[2048];//读入时使用的内存池
    char* p = buf, * last_p = buf;//当前参数结束、开始指针
    char* argsbuf[128];//全部参数列表，字符串指针数组，包含argv传入的参数和stdin读入的参数
    char** args = argsbuf;//指向argsbuf中第一个从stdin读入的参数
    
    //首先将xargs的参数复制到argsbuf中
    for(int i = 1; i < argc; i++){
        *args = argv[i];
        args++;
    }

    //记录当前参数位置
    char** pa = args;

    //从标准输入读取数据，存储在缓冲区buf中
    while(read(0, p, 1) != 0){
        if(*p == ' ' || *p == '\n'){
            //读入一个参数完成
            *p = '\0';//根据空格将字符串分割为多个参数
            *(pa++) = last_p;
            last_p = p + 1;    

            //每当遇到换行符时，表示一组参数读取完毕，调用run函数
            if(*p == '\n'){
                *pa = 0;
                run(argv[1], argsbuf);
                pa = args; //重置读入参数指针，准备读入下一行
            }
        }
        p++;
    }
    
    //如果最后一行不是空行，同样的逻辑再处理一次
    if(pa != args){
        //收尾最后一个参数
        *p = '\0';
        *(pa++) = last_p;
        //收尾最后一行
        *pa = 0;//参数列表末尾用null标识列表结束
        //执行最后一行指令
        run(argv[1], argsbuf);
    }
    //等待所有子进程结束
    while(wait(0) != -1){};
    exit(0);
}