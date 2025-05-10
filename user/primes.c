// primes.c : 筛选素数

#include"kernel/types.h"
#include"kernel/stat.h"
#include"user/user.h"

//筛选指数的函数，接收一个管道作为参数
void sieve(int pleft[2]){
    //从左邻居读取整数
    int p;
    read(pleft[0], &p, sizeof(p));
    if(p == -1){
        exit(0);//如果读取到-1，表示结束，退出进程
    }
    printf("prime %d\n", p);

    //创建一个新的管道
    int pright[2];
    pipe(pright);

    if(fork() == 0){//右邻居,子进程，下一个stage
        close(pright[1]);//右邻居用不到这个管道的写端，关闭
        close(pleft[0]);//父进程的读取端用不到，关闭
        sieve(pright);//递归调用筛选函数
    }else{//左邻居，父进程，当前阶段
        close(pright[0]);//父进程不需要管道的读端，关闭
        //从左邻居接收数据
        int buf;
        while(read(pleft[0], &buf, sizeof(buf)) && buf != -1){
            if(buf % p != 0){//筛掉能被该进程筛掉的数字
                write(pright[1], &buf, sizeof(buf));//将剩余的数写进右端
            }
        }
        //接收左端来的-1，传递给右端-1
        buf = -1;
        write(pright[1], &buf, sizeof(buf));
        wait(0);
        exit(0);
    }
}

int main(int argc, char** argv){
    //创建初始管道
    int input_pipe[2];
    pipe(input_pipe);

    if(fork() == 0){//子进程
        close(input_pipe[1]);//子进程用不到管道的写端，关闭
        sieve(input_pipe);//调用筛选函数
        exit(0);
    }else{//父进程
        close(input_pipe[0]);//父进程用不到管道的读端，关闭
        int i;
        for(i = 2; i <= 35; i++){
            write(input_pipe[1], &i, sizeof(i));//向管道写入2~35
        }
        //写入结束标志
        i = -1;
        write(input_pipe[1], &i, sizeof(i));
    }
    wait(0);

    exit(0);
}