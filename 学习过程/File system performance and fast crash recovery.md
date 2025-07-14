# File system performance and fast crash recovery
##  Why logging
- 这节课讲的是Linux中的广泛使用的ext3文件系统所使用的logging系统，同时我们也会讨论在高性能文件系统中添加log需要面对的一些问题
- 为什么要学习logging
  1. 可以认为logging是一种魔法，它可以用在任何一个已知的存储系统的故障恢复流程中，它在很多地方都与你想存储的任何数据相关。所以你们可以在大量的存储场景中看到log，比如说数据库，文件系统，甚至一些需要在crash之后恢复的定制化的系统中。
  2. 因为log是一种可以用来表示所有在crash之前发生事情的数据结构，如果能够理解log，那么就可以更容易的从crash中恢复过来。
  3. 当你尝试构建高性能logging系统时，log本身也有大量有意思的地方。
- 今天的论文是有关向ext2增加journal，并得到ext3文件系统（注，所以可以认为ext3文件系统就是ext2加上了logging系统）。

## XV6 File system logging回顾
- 可以认为磁盘分为了两个部分：
  1. 首先是文件系统目录的树结构，以root目录为根节点，往下可能有其他的目录，我们可以认为目录结构就是一个树状的数据结构。
  2. 除了文件系统之外，XV6在磁盘最开始的位置还有一段log。header block会记录之后的每一个log block应该属于文件系统中哪个block，假设第一个log block属于block 17，第二个属于block 29。
- Frans和Robert在这里可能有些概念不统一，对于Frans来说，目录内容应该也属于文件内容，目录是一种特殊的文件，详见14.3；而对于Robert来说，目录内容是metadata。
- 在内核中存在block cache，最初write请求会被发到block cache。block cache就是磁盘中block在内存中的拷贝，所以最初对于文件block或者inode的更新走到了block cache。
-  在write系统调用的最后，这些更新都被拷贝到了log中，之后我们会更新header block的计数来表明当前的transaction已经结束了。
-  在文件系统的代码中，任何修改了文件系统的系统调用函数中，某个位置会有begin_op，表明马上就要进行一系列对于文件系统的更新了，不过在完成所有的更新之前，不要执行任何一个更新。
-  在begin_op之后是一系列的read/write操作。
-  最后是end_op，用来告诉文件系统现在已经完成了所有write操作。所以在begin_op和end_op之间，所有的write block操作只会走到block cache中。当系统调用走到了end_op函数，文件系统会将修改过的block cache拷贝到log中。
-  在拷贝完成之后，文件系统会将修改过的block数量，通过一个磁盘写操作写入到log的header block，这次写入被称为commit point。
-  在commit point之前，如果发生了crash，在重启时，整个transaction的所有写磁盘操作最后都不会应用。在commit point之后，即使立即发生了crash，重启时恢复软件会发现在log header中记录的修改过的block数量不为0，接下来就会将log header中记录的所有block，从log区域写入到文件系统区域。
-  这里实际上使得系统调用中位于begin_op和end_op之间的所有写操作在面对crash时具备原子性。也就是说，要么文件系统在crash之前更新了log的header block，这样所有的写操作都能生效；要么crash发生在文件系统更新log的header block之前，这样没有一个写操作能生效。
-  重要的点：
  1. 包括XV6在内的所有logging系统，都需要遵守write ahead rule。这里的意思是，任何时候如果一堆写操作需要具备原子性，系统需要先将所有的写操作记录在log中，之后才能将这些写操作应用到文件系统的实际位置。write ahead rule是logging能实现故障恢复的基础。write ahead rule使得一系列的更新在面对crash时具备了原子性。
  2. XV6对于不同的系统调用复用的是同一段log空间，但是直到log中所有的写操作被更新到文件系统之前，我们都不能释放或者重用log。我将这个规则称为freeing rule，它表明我们不能覆盖或者重用log空间，直到保存了transaction所有更新的这段log，都已经反应在了文件系统中。
  3. Journaling the Linux ext2fs Filesystem,freeing rule的意思就是，在从log中删除一个transaction之前，我们必须将所有log中的所有block都写到文件系统中。
- XV6的logging有什么问题？为什么Linux不使用与XV6完全一样的logging方案？这里的回答简单来说就是XV6的logging太慢了。
- 在任何一个文件系统调用的commit过程中，不仅是占据了大量的时间，而且其他系统调用也不能对文件系统有任何的更新。所以这里的系统调用实际上是一次一个的发生，而每个系统调用需要许多个写磁盘的操作。
- 这里每个系统调用需要等待它包含的所有写磁盘结束，对应的技术术语被称为synchronize。XV6的系统调用对于写磁盘操作来说是同步的（synchronized），所以它非常非常的慢。
- 在XV6的logging方案中，每个block都被写了两次。第一次写入到了log，第二次才写入到实际的位置。虽然这么做有它的原因，但是ext3可以一定程度上修复这个问题。

## ext3 file system log format
- ext3是针对之前一种的文件系统（ext2）logging方案的修改，所以ext3就是在几乎不改变之前的ext2文件系统的前提下，在其上增加一层logging系统
- ext3的数据结构与XV6是类似的。在内存中，存在block cache，这是一种write-back cache（注，区别于write-through cache，指的是cache稍后才会同步到真正的后端）
- ext3还维护了一些transaction信息。它可以维护多个在不同阶段的transaction的信息。每个transaction的信息包含有：
  1. 一个序列号
  2. 一系列该transaction修改的block编号。这些block编号指向的是在cache中的block，因为任何修改最初都是在cache中完成。
  3. 以及一系列的handle，handle对应了系统调用，并且这些系统调用是transaction的一部分，会读写cache中的block
- 在磁盘上，与XV6一样：
  1. 会有一个文件系统树，包含了inode，目录，文件等等
  2. 会有bitmap block来表明每个data block是被分配的还是空闲的
  3. 在磁盘的一个指定区域，会保存log
- 主要的区别在于ext3可以同时跟踪多个在不同执行阶段的transaction
- ext3的log:
  1. 在log的最开始，是super block。这是log的super block，而不是文件系统的super block。log的super block包含了log中第一个有效的transaction的起始位置和序列号
