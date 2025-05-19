# 一些课堂上的问与答
1. 学生问：PPN是如何合并成最终的物理内存地址？\
教授答： 在最高级的page directory中的PPN，包含了下一级page directory的物理内存地址，以此类推。在最低级page directory，我们还是可以得到44比特的PPN，这里包含了我们实际上想要翻译的物理page地址，
然后再加上虚拟内存地址12bit offset，就得到了56bit物理内存地址。（可以把PPN理解为指针）
2. 教授问：为什么PPN存在这些page directory中？为什么不是一个虚拟内存地址？\
学生答：因为我们需要在物理内存中查找下一个page directory的地址
3. 教授说：是的，我们泵让我们的地址翻译依赖于另一个翻译，否则我们可能陷入递归的无线循环中。所以page directory必须存物理地址。那SATP呢？它存的是物理地址还是虚拟地址？\
学生回答：还是物理地址，因为最高级的page directory还是存在物理内存中，对吧\
教授说：是的，必须是物理地址，因为我们要用它来完成地址翻译，而不是对它进行地址翻译。所以SATP需要知道最高一级的page directory的物理地址是什么
4. 学生问：这里有层次化的3个page table，每个page table都是用虚拟地址的9个比特来索引，所以是由虚拟地址
5. 中的3个9比特来分别索引3个page table吗？\
教授答：是的，最高的9比特用来索引最高一级的page directory，以此类推
6. 学生问：最高一级的page table会用虚拟内存地址中的27bit index的最高9bit来完成索引，如果索引为空，MMU会自动创建一个page table吗？\
教授答：不会的，MMU会告诉操作系统或者处理器，抱歉我不能翻译这个地址，最终会变成一个page fault。如果一个地址不能被翻译，那就不翻译。
7. 学生问：怎么计算page table的物理地址，是不是这样，我们从最高级的page table得到44bit的PPN，然后再加上虚拟地址中的12bit offset，就得到了完整的56比特page table物理地址？\
教授回答：不会加上虚拟地址中的offset，这里只是使用了12bit的0。所以我们用44bit的PPN，再加上12bit的0，这样就得到了下一级page directory的56bit物理地址。这里要求每个page directory都与物理page对齐（也就是page directory的起始地址就是某个page的起始地址，所有低12bit都为0）.     
8. 学生提问：3级的page table是由操作系统实现还是由硬件自己实现的？\
教授回答：这是由硬件实现的，所有3级page table的查找都发生在硬件中。MMU是硬件的一部分而不是操作系统的一部分。在XV6中，有一个函数也实现了page table的查找，因为时不时的xv6也需要完成硬件的工作，所以XV6有这个叫做walk的函数，他在软件中实现了MMU硬件相同的功能。
9. 学生提问：之前提到，硬件会完成3级page table的查找，那为什么我们要在XV6中有一个walk函数来完成同样的工作？\
教授回答： 这里有几个原因，首先XV6中的walk函数设置了最初的page table，它需要对三级page table进行编程所以它首先要能模拟3级page table。另一个原因或许你们已经在syscall实验中遇到了，在XV6中，内核有它自己的page table,用户进程也有自己的page table，用户进程指向sys_info结构体的指针存在于用户空间的page table，但是内核需要将这个指针翻译成一个自己可以读写的物理地址。如果你查看copy_in，copy_out，你可以发现内核会通过用户进程的page table，将用户的虚拟地址翻译得到物理地址，这样内核可以读写相应的物理内存地址     
10. 学生问：对于不同的进程会有不同的kernel stack吗？\
教授答：是的，每个用户进程都有一个对应的kernel stack
11. 学生问：用户程序的虚拟内存会映射到未使用的物理地址空间吗？\
教授回答：在kernel page table中，有一段Free Memory，它对应了物理内存中的一段地址。XV6使用这段free memory来存放用户进程的page table，text，data。如果我们运行了非常多的用户进程，某个时间点我们会耗尽这段内存，这个时候fork或者exec会返回错误。
12. 学生问：这就意味着，用户进程的虚拟地址会比内核的虚拟地址空间小的多，是吗？\
教授回答：本质上来说，两边的虚拟地址空间大小是一样的。但用户进程的虚拟地址空间使用率会更低。
13. 学生问：如果多个进程都将内存映射到了同一个物理位置，这里会优化合并到同一个地址吗？\
教授回答：XV6不会做这样的事情，但是page table实验中有一部分就是在做这个事情。真正的操作系统会做这样的工作
14. 学生问：每个进程都会有自己的3级树状page table，通过这个page table将虚拟地址翻译成物理地址。所以看起来当我们将内核虚拟地址翻译成物理地址时，我们并不需要kernel的page table，因为进程会使用自己的树状page table并完成地址翻译\
教授回答：当kernel创建了一个进程，针对这个进程的page table也会从free memory中分配出来。内核会为用户进程的page table分配几个page，并填入PTE。在某个时间点，当内核运行了这个进程，内核会将进程的根page table的地址加载到SATP中。从那个时间点开始，处理器会使用内核为那个进程构建的虚拟地址空间。
15. 学生提问：所以内核为进程放弃了一些自己的内存，但是进程的虚拟地址空间理论上与内核的虚拟地址空间一样大，虽然实际中肯定不会这样大\
教授回答：是的，用户进程的虚拟地址空间分布，与内核地址空间一样，他也是从0到MAXVA。它有由内核设置好的，专属于进程的page table来完成地址翻译。
16. 学生提问：但是我们不能将所有的MAXVA地址都使用吧？\
教授回答：是的，否则我们会耗尽内存。大多数的进程使用的内存都远远小于虚拟地址空间
17. 教授答：walk这个函数会返回page table的PTE，而内核可以读写PTE。这个函数的作用是返回某一个PTE的指针。这是个虚拟地址，它指向了这个PTE。之后内核可以通过向这个地址写数据来操纵这条PTE执行的物理page。当page table被加载到SATP寄存器，这里的更改就会生效。
```
//walk
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
   if(va >= MAXVA){
       panic("walk");
   }
   for(inr level = 2; level > 0; level--){
       pte_t* pte = &pagetable[PX(level,va)];
       if(* pte & PTE_V){
           pagetable = (pagetable_t)PTE2PA(* pte);
       }
       else{
           if(!alloc || (pagetable = (pde_t*)kalloc()) == 0){
                 return 0;
           }
           memset(pagetable, 0, PGSIZE);
           * pte = PA2PTE(PAGETABLE) | PTE_V;
       }
   }
   return &pagetable[PX(0, va)];
}
```
从代码看，这个函数从level2走到level1然后到level0，如果参数alloc不为0，且某一个level的page table不存在，这个函数就会创建一个临时的page table，将内容初始化为0，并继续运行。所以最后总是返回的是最低一级的page directory的PTE\
如果参数alloc没有设置，那么在第一个PTE对应的下一级page table不存在时就会返回\
19. 学生问：对于walk函数，在写完SATP寄存器之后，内核还能直接访问物理地址吗？在代码里面看起来像是通过page table将虚拟地址翻译成了物理地址，但是这个时候SATP已经被设置了，得到的物理地址不会被认为是虚拟地址吗？\
教授回答：来看一下kvminithart函数，这里的kernel_page_table是一个物理地址，并写入到SATP寄存器中。从那以后，我们代码运行在一个我们构建出来的地址空间中。在之前的kvminit函数中，kvmmap会对每个地址或者每个page调用walk函数\
20. 学生问：在SATP寄存器设置完之后，walk是不是还是按照相同的方式工作？\
教授答：是的。它还能工作的原因是，内核设置了虚拟地址等于物理地址的映射关系，因为很多地方能工作的原因都是因为内核设置的地址映射关系是相同的\
21. 学生问：每一个进程的SATP寄存器存在哪？\
教授答：每个CPU核只有一个SATP寄存器，但是在每个proc结构体，如果你查看proc.h，里面有一个指向page table的指针，这对应了进程的根page table物理内存地址\
22. 学生问：为什么通过三级page table会比一个超大的page table更好呢？\
教授答：3级page table中，大量的PTE都是可以不存储。比如，对于最高级的page table里面，如果有一个PTE为空，那么就完全不用创建它对应的中间级和最底层page table，以及里面的PTE。所以，这就像是在整个虚拟地址空间中的一大段地址完全不需要有映射一样

# 实验3
- 需要切换到本次实现的分支：pgtbl
## 知识点
实现开始前，最好阅读(XV6手册)[https://pdos.csail.mit.edu/6.S081/2020/xv6/book-riscv-rev1.pdf]的第三章，以及阅读源码`kernel/vm.c`。
