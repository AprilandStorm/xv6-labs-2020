# 文件系统
- 突出的特性：1. user friendly names/pathnames; 2. share files between users/processes; 3. persistence/durability
- inode（索引节点）是文件的“元信息”存储结构，记录了文件的属性、数据块地址等，但不包含文件名。
把文件放进抽屉里，抽屉外贴了个标签（文件名），但真正描述文件内容、体重、出生日期的身份证就放在另一个地方 —— 这张“身份证”就是 inode。
##
## 存储设备 disk
- SSD通常是0.1到1毫秒的访问时间，而HDD通常是在10毫秒量级完成读写一个disk block。
- sector通常是磁盘驱动可以读写的最小单元，它过去通常是512字节。
- block通常是操作系统或者文件系统视角的数据。在XV6中它是1024字节.所以XV6中一个block对应两个sector。通常来说一个block对应了一个或者多个sector。
- 这些存储设备连接到了电脑总线之上，总线也连接了CPU和内存。
