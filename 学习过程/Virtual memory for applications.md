# Q&A
- 当我们允许用户针对Page Fault来运行handler代码时，这不会引入安全漏洞吗？\
答：当我们执行upcall的时候，upcall会走到设置了handler的用户空间进程中，所以handler与设置了它的应用程序运行在相同的context，相同的Page Table中。所以handler唯一能做的事情就是影响那个应用程序，并不能影响其他的应用程序，因为它不能访问其他应用程序的Page Table，或者切换到其他应用程序的Page Table。所以这里还好。当然，如果handler没有返回，或者做了一些坏事，最终内核还是会杀掉进程。所以唯一可能出错的地方就是进程伤害了自己，但是它不能伤害任何其他进程。
- 在分配物理内存Page时，我们需要让操作系统映射到地址空间的特定地址，否则的话可能会映射到任意地址，是吧？\
答：操作系统会告知是哪个地址，并且这里可能是任意的地址。
- 能再讲一下为什么一个物理内存Page就可以工作吗？我觉得这像是lazy allocation，但是区别又是什么呢？\
答：当我们刚刚完成设置时，我们一个内存Page都没有，setup_sqrt_region分配了一个地址段，但是又立即通过munmap将与这个地址段关联的内存释放了。所以在启动的最开始对于表单并没有一个物理内存Page与之关联。\
之后，当我们得到了一个Page Fault，这意味着整个表单对应的地址中至少有一个Page没有被映射，虽然实际上我们一个Page都没有映射。现在我们得到了一个Page Fault，我们只需要映射一个Page，在这个Page中，我们会存入i，i+1。。。的平方根（注，因为一个Page4096字节，一个double8个字节，所以一个Page可以保存512个表单项）。因为这是第一个Page Fault，之前并没有映射了内存Page，所以不需要做任何事情。\
之后，程序继续运行并且查找了表单中的更多项，如果查找一个没有位于已分配Page上的表单项时，会得到另一个Page Fault。这时，在handle_sigsegv会分配第二个内存Page，并为这个Page计算平方根的值。之后会munmap记录在last_page_base中的内存。\
当然，在实际中我们永远也不会这么做，在实际中至少会保留一些内存Page，这里只是以一种极端的方式展示，你可以只通过内存中的一个Page来表示一个巨大的表单。所以在handle_sigsegv中，会释放上一次映射的内存Page。之后程序继续运行，所以在任何一个时间，只有一个物理内存Page被使用了。很明显，你们在实际中不会这么做，这里更多的是展示前面提到特性的能力。
- 问：刚刚说到在Handler里面会扫描一个Page中的所有对象，但是对象怎么跟内存Page对应起来呢？\
答：在最开始的时候，to空间是没有任何对象的。当需要forward的时候，我刚刚描述的是拷贝一个对象，但是实际上拷贝的是一个内存Page中的N个对象，这样它们可以填满整个Page。所以现在我们在to空间中，有N个对象位于一个Page中，并且它们都没有被扫描。之后某个时间，Page Fault Handler会被调用，GC会遍历这个内存Page上的N个对象，并检查它们的指针。对于这些指针，GC会将对应的对象拷贝到to空间的unscanned区域中。之后，当应用程序使用了这些未被扫描的对象，它会再次得到Page Fault，进而再扫描这些对象，以此类推。
- 在完成了GC之后，会切换from和to空间吗？\
答：最开始我们使用的是from空间，当用完了的时候，你会将对象拷贝到to空间，一旦完成了扫描，from空间也被完全清空了，你可以切换两个空间的名字。现在会使用to空间来完成内存分配。直到它也满了，你会再次切换。
- GC和应用程序是不是有不同的Page Table？\
答：不，它们拥有相同的Page Table。它们只是将物理内存映射到了地址空间的两个位置，也就是Page Table的两个位置。在一个位置，PTE被标记成invalid，在另一个位置，PTE被标记成可读写的。
- 问：GC什么时候会停止，什么时候又会再开始？我认为GC可以一直运行，如果它是并发的。\
答：是的，基于虚拟内存的解决方案一个酷的地方在于，GC可以一直运行。它可以在没有unscanned对象时停止。
- 但是你需要遍历所有在from空间的对象，你怎么知道已经遍历了所有的对象呢？\
答：你会从根节点开始扫描整个对象的图，然后拷贝到to空间。在某个时间点，你不再添加新的对象了，因为所有的对象已经被拷贝过了。当你不再添加新的对象，你的unscanned区域就不再增长，如果它不再增长，那么你就遍历了所有的对象

# 应用程序使用虚拟内存所需要的特性
- 今天的话题是用户应用程序使用的虚拟内存，它主要是受这篇1991年的[论文](https://pdos.csail.mit.edu/6.828/2020/readings/appel-li.pdf)的启发。
- 今天论文中的核心观点是，用户应用程序也应该从灵活的虚拟内存中获得收益，也就是说用户应用程序也可以使用虚拟内存。
- 这里说的虚拟内存是指：User Mode或者应用程序想要使用与内核相同的机制，来产生Page Fault并响应Page Fault
- 也就是说User Mode需要能够修改PTE的Protection位（注，Protection位是PTE中表明对当前Page的保护，对应了4.3中的Writeable和Readable位）或者Privileged level。
- XV6不支持任何一个以下的特性。尽管在XV6的内核中包含了所有的可用的虚拟内存的机制，但是并没有以系统调用的形式将它们暴露给用户空间。
- 论文的观点是，任何一个好的操作系统都应该以系统调用的形式提供以上特性，以供应用程序使用。
## 需要的特性是什么？
- 首先，你需要trap来使得发生在内核中的Page Fault可以传播到用户空间，然后在用户空间的handler可以处理相应的Page Fault，之后再以正常的方式返回到内核并恢复指令的执行。
- 第二个特性是Prot1，它会降低了一个内存Page的accessability。accessability的意思是指内存Page的读写权限。内存Page的accessability有不同的降低方式，例如，将一个可以读写的Page变成只读的，或者将一个只读的Page变成完全没有权限。
- 管理多个Page的ProtN。ProtN基本上等效于调用N次Prot1，那为什么还需要有ProtN？因为单次ProtN的损耗比Prot1大不了多少，使用ProtN可以将成本分摊到N个Page，使得操作单个Page的性能损耗更少。
- 下一个特性是Unprot，它增加了内存Page的accessability，例如将本来只读的Page变成可读可写的。
- 查看内存Page是否是Dirty
- map2。map2使得一个应用程序可以将一个特定的内存地址空间映射两次，并且这两次映射拥有不同的accessability（注，也就是一段物理内存对应两份虚拟内存，并且两份虚拟内存有不同的accessability）

# 支持应用程序使用虚拟内存的系统调用
## mmap系统调用
- 第一个或许也是最重要的一个，是一个叫做mmap的系统调用。
- 它接收某个对象，并将其映射到调用者的地址空间中。
- mmap系统调用有许多令人眼花缭乱的参数：
  1. 第一个参数是一个你想映射到的特定地址，如果传入null表示不指定特定地址，这样的话内核会选择一个地址来完成映射，并从系统调用返回。
  2. 想要映射的地址段长度len
  3. 是Protection bit，例如读写R|W
  4. flags，MAP_PRIVATE是其中一个值，在mmap文件的场景下，MAP_PRIVATE表明更新文件不会写入磁盘，只会更新在内存中的拷贝，详见[man page](https://man7.org/linux/man-pages/man2/mmap.2.html
)）
 5. 第五个参数是传入的对象，在上面的例子中就是文件描述符。
 6. 第六个参数是offset。
- 通过上面的系统调用，可以将文件描述符指向的文件内容，从起始位置加上offset的地方开始，映射到特定的内存地址（如果指定了的话），并且连续映射len长度。
- 这使得你可以实现Memory Mapped File，你可以将文件的内容带到内存地址空间，进而只需要方便的通过普通的指针操作，而不用调用read/write系统调用，就可以从磁盘读写文件内容。
- mmap还可以用作他途。除了可以映射文件之外，还可以用来映射匿名的内存（Anonymous Memory）。这是sbrk（注，详见8.2）的替代方案，你可以向内核申请物理内存，然后映射到特定的虚拟内存地址。

## mprotect系统调用
- 当你将某个对象映射到了虚拟内存地址空间，你可以修改对应虚拟内存的权限，这样你就可以以特定的权限保护对象的一部分，或者整个对象。
- 举个例子，通过上图对于mprotect的调用将权限设置成只读（R），这时，对于addr到addr+len这段地址，load指令还能执行，但是store指令将会变成Page Fault。类似的，如果你想要将一段地址空间变成完成不可访问的，那么可以在mprotect中的权限参数传入None，那么任何对于addr到addr+len这段地址的访问，都会生成Page Fault。

## munmap系统调用
- 它使得你可以移除一个地址或者一段内存地址的映射关系。

## sigaction系统调用
- 本质上是用来处理signal。
- 它使得应用程序可以设置好一旦特定的signal发生了，就调用特定的函数。
- 在Page Fault的场景下，生成的signal是segfault。

## sigalarm
- 译注：sigalarm 是traps lab 中实现的一个系统调用，不属于标准Unix 接口
- 在sigalarm中可以设置每隔一段时间就调用handle

# 虚拟内存系统如何支持用户应用程序
## 有关实现，有两个方面较为有趣。
- 第一个是虚拟内存系统为了支持这里的特性，具体会发生什么？
  1. 在现代的Unix系统中，地址空间是由硬件Page Table来体现的，在Page Table中包含了地址翻译。
  2. 地址空间还包含了一些操作系统的数据结构，这些数据结构与任何硬件设计都无关，它们被称为Virtual Memory Areas（VMAs）
  3. VMA会记录一些有关连续虚拟内存地址段的信息。
  4. 在一个地址空间中，可能包含了多个section，每一个section都由一个连续的地址段构成，对于每个section，都有一个VMA对象。
  5. 连续地址段中的所有Page都有相同的权限，并且都对应同一个对象VMA（例如一个进程的代码是一个section，数据另一个sec是tion，它们对应不同的VMA，VMA还可以表示属于进程的映射关系，例如下面提到的Memory Mapped File）。
- User level trap是如何实现的？举个例子，如果是segfault，并且应用程序设置了一个handler来处理它，那么
  1. segfault事件会被传播到用户空间
  2. 并且通过一个到用户空间的upcall在用户空间运行handler
  3. 在handler中或许会调用mprotect来修改PTE的权限
  4. 之后handler返回到内核代码
  5. 最后，内核再恢复之前被中断的进程。

# 构建大的缓存表
- 什么是缓存表？它是用来记录一些运算结果的表单。
- 可以将一个费时的函数运算转变成快速的表单查找，所以这里一个酷的技巧就是预先将费时的运算结果保存下来。如果
存，这里可以使用论文提到的虚拟内存特性来解决这个挑战。
  1. 首先，你需要分配一个大的虚拟地址段，但是并不分配任何物理内存到这个虚拟地址段。
  2. 但是现在表单中并没有内容，表单只是一段内存地址。如果你现在查找表单的i槽位，会导致Page Fault。在发生Page Fault时，先针对对应的虚拟内存地址分配物理内存Page，之后计算f(i)，并将结果存储于tb[i]，也就是表单的第i个槽位，最后再恢复程序的运行。
  3. 即使接下来你要查找表单的i+1槽位，因为一个内存Page可能可以包含多个表单项，这时也不用通过Page Fault来分配物理内存Page。
  4. 不过如果你一直这么做的话，因为表单足够大，你最终还是会消耗掉所有的物理内存。所以Page Fault Handler需要在消耗完所有的内存时，回收一些已经使用过的物理内存Page。需要修改已经被回收了的物理内存对应的PTE的权限，这样在将来使用对应地址段时，就可以获得Page Fault。所以你需要使用Prot1或者ProtN来减少这些Page的accessbility。
## 程序示例  
- main()\
setup_sqrt_region(){它会从地址空间分配地址段，但是又不实际分配物理Page}->test_sqrt_region()
- test_sqrt_region()\
  1. 在test_sqrt_region中，会以随机数来遍历表单，并通过实际运算对应的平方根值，来检查表单中相应位置值是不是保存了正确的平方根值。
  2. 在test_sqrt_region运行的过程中，会产生Page Fault
- setup_sqrt_region()
  1. 通过将handle_sigsegv函数注册到了SIGSEGV事件。这样当segfault或者Page Fault发生时，内核会调用handle_sigsegv函数
- handle_sigsegv()\
handle_sigsegv函数与你们之前看过很多很多次的trap代码非常相似
  1. 它首先会获取触发Page Fault的地址
  2. 之后调用mmap对这个虚拟内存地址分配一个物理内存Page（注，这里是mmap映射匿名内存）。这里的虚拟内存地址就是我们想要在表单中用来保存数据的地址。
  3. 然后我们为这个Page中所有的表单项都计算对应的平方根值，之后就完事了。

# Baker's Real-Time Copying Garbage Collector
- Garbage Collector, GC是指编程语言替程序员完成内存释放，这样程序员就不用像在C语言中一样调用free来释放内存.
- 对于拥有GC的编程语言，程序员只需要调用类似malloc的函数来申请内存，但是又不需要担心释放内存的过程。
- GC会决定内存是否还在使用，如果内存并没有被使用，那么GC会释放内存。
- 论文中讨论了一种特定的GC，这是一种copying GC。什么是copying GC？
  1. opying GC的基本思想是将仍然在使用的对象拷贝到to空间去，具体的流程是 从根节点开始拷贝。每一个应用程序都会在一系列的寄存器或者位于stack上的变量中保存所有对象的根节点指针，通常来说会存在多个根节点，但是为了说明的简单，我们假设只有一个根节点。拷贝的流程会从根节点开始向下跟踪，所以最开始将根节点拷贝到了to空间，但是现在根节点中的指针还是指向着之前的对象。
  2. 之后，GC会扫描根节点对象。
  3. 接下来GC会将根节点对象中指针指向的对象也拷贝到to空间，很明显这些也是还在使用中的对象。当一个对象被拷贝到to空间时，根节点中的指针会被更新到指向拷贝到了to空间的对象。
  4. 还会存储一些额外的信息来记住相应的对象已经保存在了to空间，这里会在from空间保留一个forwarding指针。这里将对象从from空间拷贝到to空间的过程称为forward。
  5. 现在与根节点相关的对象都从from空间移到了to空间，并且所有的指针都被正确的更新了，所以现在我们就完成了GC，from空间的所有对象都可以被丢弃，并且from空间现在变成了空闲区域。
- 论文中讨论的是一种更为复杂的GC算法，它被称为Baker算法，这是一种很老的算法。它的一个优势是它是实时的，这意味着它是一种incremental GC（注，incremental GC是指GC并不是一次做完，而是分批分步骤完成）。这里的基本思想是:
  1. GC的过程没有必要停止程序的运行并将所有的对象都从from空间拷贝到to空间，然后再恢复程序的运行。
  2. GC开始之后，唯一必要的事情，就是将根节点拷贝到to空间。所以现在根节点被拷贝了，但是根节点内的指针还是指向位于from空间的对象。根节点只是被拷贝了并没有被扫描，其中的指针还没有被更新。
  3. 我们不应该跟踪from空间的指针（注，换言之GC时的指针跟踪都应该只在同一个空间中完成）。

# 使用虚拟内存特性的GC
- 如果拥有了前面提到的虚拟内存特性，你可以使用虚拟内存来减少指针检查的损耗，并且以几乎零成本的代价来并行运行GC。
- 这里的基本思想是将heap内存中from和to空间，再做一次划分，每一个部分包含scanned，unscanned两个区域。
- 在开始GC时，我们将根节点对象拷贝到to空间，但是根节点中的指针还是指向了位于from空间的对象。现在unscanned区域包括了所有的对象（注，现在只有根节点），我们会将unscanned区域的权限设置为None。这意味着，当开始GC之后，应用程序第一次使用根节点，它会得到Page Fault，因为这部分内存的权限为None。
- 在Page Fault Handler中，GC需要扫描位于内存Page中所有的对象，然后将这些对象所指向的其他对象从from空间forward到to空间。
- 之后，应用程序就可以访问特定的对象，因为我们将对象中的指针转换成了可以安全暴露给应用程序的指针（注，因为这些指针现在指向了位于to空间的对象），所以应用程序可以访问这些指针。
- 这种方案的好处是，它仍然是递增的GC，因为每次只需要做一小部分GC的工作。除此之外，它还有额外的优势：现在不需要对指针做额外的检查了（注，也就是不需要查看指针是不是指向from空间，如果是的话，将其forward到to空间）。
- 论文中提到使用虚拟内存的另一个好处是，它简化了GC的并发。GC现在可以遍历未被扫描的内存Page，并且一次扫描一个Page，同时可以确保应用程序不能访问这个内存Page，因为对于应用程序来说，未被扫描的内存Page权限为None。
- 对于应用程序来说，unscanned区域中的Page权限为None。这就引出了另一个问题，GC怎么能访问这个区域的内存Page？因为对于应用程序来说，这些Page是inaccessible。这里的技巧是使用map2，将同一个物理内存映射两次，第一次是我们之前介绍的方式，也就是为应用程序进行映射，第二次专门为GC映射。在GC的视角中，我们仍然有from和to空间。在to空间的unscanned区域中，Page具有读写权限。

# 使用虚拟内存特性的GC代码展示
## 应用程序使用的API包括了new和readptr
```
struct elem* readptr(struct elem** ptr) ;
struct elem* new();
```
- readptr会检查指针是否位于from空间，如果是的话，那么它指向的对象需要被拷贝。
- 我有一个循环链表，并且有两个根节点，其中一个指向链表的头节点，另一个指向链表的尾节点。
## 应用程序线程的工作是循环1000次，每次创建list，再检查list。
```
void* app_thread(void* x){
  for(int i = 0; i < 1000; i++){
      make_clist();
      check_clist();
  }
}
```
- 所以它会产生大量的垃圾，因为每次make_clist完成之后，再次make_clist，上一个list就成为垃圾了。所以GC必然会有一些工作要做。
## make_clist的代码
```
//这段代码创建了一个值为 0 到 LISTSZ-1 的循环链表（新节点插入头部，值递增），并每次插入后调用检查函数 check_clist()。
void make_clist(void) {
    struct elem *e;//定义了一个指向 struct elem 类型的指针 e，用于创建新的链表节点。
    root_head = new();
    readptr(&root_head)->val = 0;
    readptr(&root_head)->next = readptr(&root_head);//设置该节点的 next 指针指向自己，构成一个单节点的循环链表。
    root_last = readptr(&root_head);//将 root_last 也指向这个唯一的节点
    for (int i = 1; i < LISTSZ; i++) {
        e = new();
        readptr(&e)->next = readptr(&root_head);
        readptr(&e)->val = i;
        root_head = readptr(&e);
        readptr(&root_last)->next = readptr(&root_head);
        check_clist(i+1);//每次插入一个新节点后，调用 check_clist(i+1) 来检查链表结构是否正确。
    }

```
- 每个指针都需要被readptr检查包围
- make_clist会构建一个LISTSZ大小的链表，分配新的元素，并将新元素加到链表的起始位置，之后更新链表尾指针指向链表新的起始位置，这样就能构成一个循环链表。
## new()
```
struct elem *new(void) {
    struct elem *n;

    pthread_mutex_lock(&lock); // 互斥锁，保护共享内存分配资源

    if (collecting && scanned < to_free_start) {
        scan(scanned);                    // 扫描对象
        if (scanned >= to_free_start) {
            end_collecting();             // 如果扫描完了，就结束垃圾回收
        }
    }

    if (to_free_start + sizeof(struct obj) >= to + SPACESZ) {
        flip();                           // 到达当前分配区末尾，切换空间
    }

    n = (struct elem *) alloc();          // 分配新的元素
    pthread_mutex_unlock(&lock);

    return n;
}

```
- 检查是否有足够的空间，如果有足够的空间，我们就将指针地址增加一些，以分配内存空间给新的对象，最后返回。如果没有足够的空间，我们需要调用flip，也就是运行GC.

## flip()
- 实现了垃圾回收（GC）中的空间翻转（flip()）函数，是标记-复制（mark-copy GC）算法的一部分，用于在“from-space”和“to-space”之间切换。
```
void flip() {
    char *tmp = to;//临时变量 tmp 保存当前 to 空间指针
    printf("flip spaces\n");
    assert(!collecting);//确保当前不处于垃圾回收中（如果是，则说明出现了逻辑错误）
    to = from;//to表示当前from空间
    to_free_start = from;//表示当前GC分配的起点
    from = tmp;//from表示原先的to空间
    collecting = 1;//设置 collecting = 1 表示进入 GC 状态
    scanned = to;//初始化扫描位置（下一步将扫描从 to 复制过去的对象）
#ifdef VM//如果开启了虚拟内存支持（#ifdef VM）
    if (mprotect(to, SPACESZ, PROT_NONE) < 0) {//将 to 空间设置为不可访问 (PROT_NONE)，让程序在错误访问时触发 page fault。
        fprintf(stderr, "Couldn't unmap to space: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
#endif
    // move root_head and root_last to to-space
    root_head = (struct elem *) forward((struct obj *)root_head);//把 root_head 从旧空间复制到新空间,forward() 函数会判断对象是否已经被复制，并返回新位置
    root_last = (struct elem *) forward((struct obj *)root_last);//把  root_last 从旧空间复制到新空间
    pthread_cond_broadcast(&cond);//完成翻转后，使用 pthread_cond_broadcast() 通知所有等待 GC 结束的线程
}
```
- flip首先会切换from和to指针，之后将这个应用程序的两个根节点从from空间forward到to空间。
- 之后GC将root_head和root_last移到to空间中，这样应用程序就不能访问这两个对象，任何时候应用程序需要访问这两个对象，都会导致一个Page Fault。在Page Fault handler中，GC可以将其他对象从from空间拷贝到to空间，然后再Unprot对应的Page。
- 

## forward函数
- 这个函数会forward指针o指向的对象，首先检查指针o是不是在from空间，如果是的话，并且之前没有被拷贝过，那么就将它拷贝到to空间。如果之前拷贝过，那么就可以用to空间的指针代替对象指针，并将其返回。

## setup_spaces()
- 创建一个共享内存对象，并将其两次映射到进程的地址空间中，分别供 mutator 和 collector 使用。
- 首先是设置内存，通过shm_open创建一个Share-memory object，shm_open是一个Linux/Uinx系统调用；Share-memory object表现的像是一个文件，但是它并不是一个文件，它位于内存，并没有磁盘文件与之对应，可以认为它是一个位于内存的文件系统。
- 之后我们裁剪这个Shared-memory object到from和to空间的大小
- 之后我们通过mmap先将其映射一次，以供mutator也就是实际的应用程序使用。
- 然后再映射一次，以供GC使用
- 这里shm_open，ftruncate，和两次mmap，等效于map2。

## handle_sigsegv()
- 在Page Fault hanlder中，GC会运行scan函数。但是scan函数是以GC对应的PTE来运行的，所以它能工作。而同时，应用程序或者mutator不能访问这些Page，如果访问了的话，这会产生Page Fault。一旦scan执行完成，handler中会将Page设置成对应用程序可访问的（注，也就是调用mprotect）
- 代码是先扫描，再增加内存的访问权限，这样应用程序就可以安全的访问这些内存Page。

# 总结
- 现在Linux的Page Table是5级的，这样可以处理非常大的地址
- 
