//find.c : find all the files in a directory tree with a specific name

#include"kernel/types.h"
#include"kernel/stat.h"
#include"user/user.h"
#include"kernel/fs.h"

//递归查找函数，查找路径为path的目录下是否有目标文件target
void find(char* path, char* target){
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;
    //打开目录
    if((fd = open(path, 0)) < 0){
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }
    //获取目录的状态信息
    if(fstat(fd, &st) < 0){//将打开文件的信息放入st的指针中
        fprintf(2, "find : cannot stat %s\n",path);
        close(fd);
        return;
    }
    //文件、目录分别处理
    switch(st.type){
        //如果是文件，检查文件名是否与目标文件名匹配
        case T_FILE:
        if(strcmp(path+strlen(path) - strlen(target), target) == 0){//用于 检查 path 路径的最后部分是否与 target 目标文件名匹配；比较两个字符串是否相等
            printf("%s\n", path);//%s表示输出字符串
        }
        break;
        //如果是目录
        case T_DIR:
        //检查路径长度是否超出缓冲区大小
        if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){//目录字符长度加上"/"长度1加上最大文件名长度加上字符串结束符0的长度；如果该长度大于缓存字符长度，则溢出
            printf("find: path too long\n");
            break;
        }
        strcpy(buf, path);//将path复制给buf，使得buf包含当前目录路径
        p = buf + strlen(buf);//让指针指向buf末尾的'0'位置
        *p++ = '/';//在该指针指向的内存存入'/'，然后指针向后移动1位
        //读取目录项
        while (read(fd, &de, sizeof(de)) == sizeof(de)){
            if(de.inum == 0){
                continue;
            }
            memmove(p, de.name, DIRSIZ);//将 de.name 复制到 p 处，最多 DIRSIZ 个字符。
            p[DIRSIZ] = 0;//memmove 不会在字符串末尾自动添加 \0，所以需要手动处理。
            //获取目录项的状态信息
            if(stat(buf, &st) < 0){//将有关命名文件的信息放入st。
                printf("find: cannot stat %s\n", buf);
                continue;
            }
            //排除"."和".."目录
            if(strcmp(buf + strlen(buf) - 2, "/.") != 0 && strcmp(buf + strlen(buf) - 3, "/..") != 0){
                find(buf, target); //递归查找子目录
            }
        }  
        break;
    }
    close(fd); //关闭目录
}

int main(int argc, char** argv){
    //如果参数不足，直接退出程序
    if(argc < 3){
        exit(0);
    }
    //查找目录下的文件
    char target[512];
    target[0] = '/'; //为查找的文件名添加“/”在开头
    strcpy(target + 1, argv[2]);//将目标文件名存储在target中
    find(argv[1], target);//调用查找函数
    exit(0);
}