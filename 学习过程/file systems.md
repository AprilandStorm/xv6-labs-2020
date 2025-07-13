# 文件系统
- 突出的特性：1. user friendly names/pathnames; 2. share files between users/processes; 3. persistence/durability
- inode（索引节点）是文件的“元信息”存储结构，记录了文件的属性、数据块地址等，但不包含文件名。
把文件放进抽屉里，抽屉外贴了个标签（文件名），但真正描述文件内容、体重、出生日期的身份证就放在另一个地方 —— 这张“身份证”就是 inode。

## Q&A
- 问：对于read/write的接口，是不是提供了同步/异步的选项？\
答：你可以认为一个磁盘的驱动与console的驱动是基本一样的。驱动向设备发送一个命令表明开始读或者写，过了一会当设备完成了操作，会产生一个中断表明完成了相应的命令。但是因为磁盘本身比console复杂的多，所以磁盘的驱动也会比我们之前看过的console的驱动复杂的多。不过驱动中的代码结构还是类似的，也有bottom部分和top部分，中断和读写控制寄存器（注，详见lec09）。
- boot block是不是包含了操作系统启动的代码？\
答：完全正确，它里面通常包含了足够启动操作系统的代码。之后再从文件系统中加载操作系统的更多内容。
- 所以XV6是存储在虚拟磁盘上？\
答：在QEMU中，我们实际上走了捷径。QEMU中有个标志位-kernel，它指向了内核的镜像文件，QEMU会将这个镜像的内容加载到了物理内存的0x80000000。所以当我们使用QEMU时，我们不需要考虑boot sector。
- 所以当你运行QEMU时，你就是将程序通过命令行传入，然后直接就运行传入的程序，然后就不需要从虚拟磁盘上读取数据了？\
答：完全正确。
- XV6中文件最大的长度是多少呢？\
答：（256+12）*1024 = 268KB
- 可以做一些什么来让文件系统支持大得多的文件呢？\
答：可以用类似page table的方式，构建一个双重indirect block number指向一个block，这个block中再包含了256个indirect block number，每一个又指向了包含256个block number的block。
- 为什么每个block存储256个block编号？\
答：因为每个编号是4个字节。1024/4 = 256。这又带出了一个问题，如果block编号只是4个字节，磁盘最大能有多大？是的，2的32次方（注，4TB）。
- 假设我们需要读取文件的第8000个字节，那么你该读取哪个block呢？从inode的数据结构中该如何计算呢？\
答：对于8000，我们首先除以1024，也就是block的大小，得到大概是7。这意味着第7个block就包含了第8000个字节。所以直接在inode的direct block number中，就包含了第8000个字节的block。为了找到这个字节在第7个block的哪个位置，我们需要用8000对1024求余数，我猜结果是是832。所以为了读取文件的第8000个字节，文件系统查看inode，先用8000除以1024得到block number，然后再用8000对1024求余读取block中对应的字节。
- 有没有一些元数据表明当前的inode是目录而不是一个文件？\
答：有的，实际上是在inode中。inode中的type字段表明这是一个目录还是一个文件。如果你对一个类型是文件的inode进行查找，文件系统会返回错误。
- write 33代表了什么？我们正在创建文件，所以我们期望文件系统干什么呢？\
答：看起来给我们分配的inode位于block 33。之所以有两个write 33，第一个是为了标记inode将要被使用。在XV6中，我记得是使用inode中的type字段来标识inode是否空闲，这个字段同时也会用来表示inode是一个文件还是一个目录。所以这里将inode的type从空闲改成了文件，并写入磁盘表示这个inode已经被使用了。第二个write 33就是实际的写入inode的内容。inode的内容会包含linkcount为1以及其他内容。
- write 46是向第一个data block写数据，那么这个data block属于谁呢？\
答：block 46是根目录的第一个block。为什么它需要被写入数据呢？\
答：向根目录增加了一个新的entry，其中包含了文件名x，以及我们刚刚分配的inode编号。
- 接下来的write 32又是什么意思呢？\
答：block 32保存的仍然是inode，那么inode中的什么发生了变化使得需要将更新后的inode写入磁盘？是的，根目录的大小变了，因为我们刚刚添加了16个字节的entry来代表文件x的信息。
- block 595看起来在磁盘中很靠后了，是因为前面的block已经被系统内核占用了吗？\
答：我们可以看前面makefs指令，makefs存了很多文件在磁盘镜像中，这些都发生在创建文件x之前，所以磁盘中很大一部分已经被这些文件填满了。
- 第二阶段最后的write 33是否会将block 595与文件x的inode关联起来？\
答：会的。这里的write 33会发生几件事情：首先inode的size字段会更新；第一个direct block number会更新。这两个信息都会通过write 33一次更新到磁盘上的inode中。
- 既然sleep lock是基于spinlock实现的，为什么对于block cache，我们使用的是sleep lock而不是spinlock？\
答：对于spinlock有很多限制，其中之一是加锁时中断必须要关闭。所以如果使用spinlock的话，当我们对block cache做操作的时候需要持有锁，那么我们就永远也不能从磁盘收到数据。或许另一个CPU核可以收到中断并读到磁盘数据，但是如果我们只有一个CPU核的话，我们就永远也读不到数据了。出于同样的原因，也不能在持有spinlock的时候进入sleep状态（注，详见13.1）。所以这里我们使用sleep lock。sleep lock的优势就是，我们可以在持有锁的时候不关闭中断。我们可以在磁盘操作的过程中持有锁，我们也可以长时间持有锁。当我们在等待sleep lock的时候，我们并没有让CPU一直空转，我们通过sleep将CPU出让出去了。
- 我有个关于brelease函数的问题，看起来它先释放了block cache的锁，然后再对引用计数refcnt减一，为什么可以这样呢？\
答：这是个好问题。如果我们释放了sleep lock，这时另一个进程正在等待锁，那么refcnt必然大于1，而b->refcnt --只是表明当前执行brelease的进程不再关心block cache。如果还有其他进程正在等待锁，那么refcnt必然不等于0，我们也必然不会执行if(b->refcnt == 0)中的代码。

## 存储设备 disk
- SSD通常是0.1到1毫秒的访问时间，而HDD通常是在10毫秒量级完成读写一个disk block。
- sector通常是磁盘驱动可以读写的最小单元，它过去通常是512字节。
- block通常是操作系统或者文件系统视角的数据。在XV6中它是1024字节.所以XV6中一个block对应两个sector。通常来说一个block对应了一个或者多个sector。
- 这些存储设备连接到了电脑总线之上，总线也连接了CPU和内存。

## inode
- 这是一个64字节的数据结构。
- 通常来说它有一个type字段，表明inode是文件还是目录。
- nlink字段，也就是link计数器，用来跟踪究竟有多少文件名指向了当前的inode。
- size字段，表明了文件数据有多少个字节。
- XV6的inode中总共有12个block编号。这些被称为direct block number。这12个block编号指向了构成文件的前12个block。举个例子，如果文件只有2个字节，那么只会有一个block编号0，它包含的数字是磁盘上文件前2个字节的block的位置。
- 之后还有一个indirect block number，它对应了磁盘上一个block，这个block包含了256个block number，这256个block number包含了文件的数据.
- inode中的信息完全足够用来实现read/write系统调用，至少可以找到哪个disk block需要用来执行read/write系统调用。
- 从路径名我们知道，应该从root inode开始查找。通常root inode会有固定的inode编号，在XV6中，这个编号是1。
- 从前一节我们可以知道，inode从block 32开始，如果是inode1，那么必然在block 32中的64到128字节的位置。所以文件系统可以直接读到root inode的内容。
- 接下来就是扫描root inode包含的所有block，以找到“y”。该怎么找到root inode所有对应的block呢？根据前一节的内容就是读取所有的direct block number和indirect block number
- 如果找到了，那么目录y也会有一个inode编号，假设是251，我们可以继续从inode 251查找，先读取inode 251的内容，之后再扫描inode所有对应的block，找到“x”并得到文件x对应的inode编号，最后将其作为路径名查找的结果返回。

## inode代码展示
- sysfile.c中包含了所有与文件系统相关的函数，分配inode发生在sys_open函数中，这个函数会负责创建文件
- 在sys_open函数中，会调用create函数。create函数中首先会解析路径名并找到最后一个目录，之后会查看文件是否存在，如果存在的话会返回错误。之后就会调用ialloc（inode allocate），这个函数会为文件x分配inode

## sleeplock
- 首先在内存中，对于一个block只能有一份缓存。这是block cache必须维护的特性。
- 这里使用了与之前的spinlock略微不同的sleep lock。与spinlock不同的是，可以在I/O操作的过程中持有sleep lock。
- 它采用了LRU作为cache替换策略。
- 它有两层锁。第一层锁用来保护buffer cache的内部数据；第二层锁也就是sleep lock用来保护单个block的cache。
- 我们花了一些时间来看block cache的实现，这对于性能来说是至关重要的，因为读写磁盘是代价较高的操作，可能要消耗数百毫秒，而block cache确保了如果我们最近从磁盘读取了一个block，那么我们将不会再从磁盘读取相同的block
- xv6 提供一个全局 buffer cache（大小固定，NBUF=30），所有磁盘读写都先访问这块 cache，结构是一个双向循环链表，头节点为 bcache.head。
