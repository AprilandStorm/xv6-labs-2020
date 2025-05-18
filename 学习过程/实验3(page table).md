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
