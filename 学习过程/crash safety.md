# Crash safety
- 这里的Crash safety并不是一个通用的解决方案，而是只关注一个特定的问题的解决方案，也就是crash或者电力故障可能会导致在磁盘上的文件系统处于不一致或者不正确状态的问题。
- 这个问题可能出现的场景可能是这样，当你在运行make指令时，make与文件系统会有频繁的交互，并读写文件，但是在make执行的过程中断电了，可能是你的笔记本电脑没电了，也可能就是停电了，之后电力恢复之后，你重启电脑并运行ls指令，你会期望你的文件系统仍然在一个好的可用的状态。
- 这里我们关心的crash或者故障包括了：在文件系统操作过程中的电力故障；在文件系统操作过程中的内核panic。
- 如果我们在多个步骤的错误位置crash或者电力故障了，存储在磁盘上的文件系统可能会是一种不一致的状态，之后可能会发生一些坏的事情。
- 解决方法，也就是logging.之所以它很流行，是因为它是一个很好用的方法。

## Q&A
- 因为这里的接口只有read/write，但是如果我们做append操作，就不再安全了，对吧？\
答：某种程度来说，append是文件系统层面的操作，在这个层面，我们可以使用上面介绍的logging机制确保其原子性（注，append也可以拆解成底层的read/write）。
- 当正在commit log的时候crash了会发生什么？比如说你想执行多个写操作，但是只commit了一半。\
答：在上面的第2步，执行commit操作时，你只会在记录了所有的write操作之后，才会执行commit操作。所以在执行commit时，所有的write操作必然都在log中。而commit操作本身也有个有趣的问题，它究竟会发生什么？如我在前面指出的，commit操作本身只会写一个block。文件系统通常可以这么假设，单个block或者单个sector的write是原子操作（注，有关block和sector的区别详见14.3）。这里的意思是，如果你执行写操作，要么整个sector都会被写入，要么sector完全不会被修改。所以sector本身永远也不会被部分写入，并且commit的目标sector总是包含了有效的数据。而commit操作本身只是写log的header，如果它成功了只是在commit header中写入log的长度，例如5，这样我们就知道log的长度为5。这时crash并重启，我们就知道需要重新install 5个block的log。如果commit header没能成功写入磁盘，那这里的数值会是0。我们会认为这一次事务并没有发生过。这里本质上是write ahead rule，它表示logging系统在所有的写操作都记录在log中之前，不能install log。
- 这是不是意味着，bwrite不能直接使用？\
答：是的，可以这么认为，文件系统中的所有bwrite都需要被log_write替换。
- group commit有必要吗？不能当一个文件系统操作结束的时候就commit掉，然后再commit其他的操作吗？\
答：如果这样的话你需要非常非常小心。因为有一点我没有说得很清楚，我们需要保证write系统调用的顺序。如果一个read看到了一个write，再执行了一次write，那么第二个write必须要发生在第一个write之后。在log中的顺序，本身就反应了write系统调用的顺序，你不能改变log中write系统调用的执行顺序，因为这可能会导致对用户程序可见的奇怪的行为。所以必须以transaction发生的顺序commit它们，而一次性提交所有的操作总是比较安全的，这可以保证文件系统处于一个好的状态。
- 前面说到cache size至少要跟log size一样大，如果它们一样大的话，并且log pin了30个block，其他操作就不能再进行了，因为buffer中没有额外的空间了。
答：如果buffer cache中没有空间了，XV6会直接panic。这并不理想，实际上有点恐怖。所以我们在挑选buffer cache size的时候希望用一个不太可能导致这里问题的数字。这里为什么不能直接返回错误，而是要panic？因为很多文件系统操作都是多个步骤的操作，假设我们执行了两个write操作，但是第三个write操作找不到可用的cache空间，那么第三个操作无法完成，我们不能就直接返回错误，因为我们可能已经更新了一个目录的某个部分，为了保证文件系统的正确性，我们需要撤回之前的更新。所以如果log pin了30个block，并且buffer cache没有额外的空间了，会直接panic。当然这种情况不太会发生，只有一些极端情况才会发生。

## 示例
### log block
- bitmap block，它记录了哪个data block是空闲的
- 在这个位置，我们先写了block 33表明inode已被使用，之后出现了电力故障，然后计算机又重启了。这时，我们丢失了刚刚分配给文件x的inode。这个inode虽然被标记为已被分配，但是它并没有放到任何目录中，所以也就没有出现在任何目录中，因此我们也就没办法删除这个inode。所以在这个位置发生电力故障会导致我们丢失inode。
- 解决方法：这些操作必须作为一个原子操作出现在磁盘上

## File system logging
### logging
- 好的属性：
  1. 它可以确保文件系统的系统调用是原子性的。
  2. 它支持快速恢复（Fast Recovery）。这里的快速是相比另一个解决方案来说，在另一个解决方案中，你可能需要读取文件系统的所有block，读取inode，bitmap block，并检查文件系统是否还在一个正确的状态，再来修复。
  3. 它可以非常的高效，尽管我们在XV6中看到的实现不是很高效。
- logging的基本思想：
  1. 你将磁盘分割成两个部分，其中一个部分是log，另一个部分是文件系统，文件系统可能会比log大得多。
  2. 当需要更新文件系统时，我们并不是更新文件系统本身。假设我们在内存中缓存了bitmap block，也就是block 45。当需要更新bitmap时，我们并不是直接写block 45，而是将数据写入到log中，并记录这个更新应该写入到block 45。对于所有的写 block都会有相同的操作，例如更新inode，也会记录一条写block 33的log。
- 基本步骤：
  1. 所以基本上，任何一次写操作都是先写入到log，我们并不是直接写入到block所在的位置，而总是先将写操作写入到log中。
  2. commit op. 之后在某个时间，当文件系统的操作结束了，比如说我们前一节看到的4-5个写block操作都结束，并且都存在于log中，我们会commit文件系统的操作。这意味着我们需要在log的某个位置记录属于同一个文件系统的操作的个数，例如5。
  3. install log. 当我们在log中存储了所有写block的内容时，如果我们要真正执行这些操作，只需要将block从log分区移到文件系统分区。
  4. clean log。一旦完成了，就可以清除log。
- 这么做为什么是好的？在重启的时候，文件系统会查看log的commit记录值，如果是0的话，那么什么也不做。如果大于0的话，我们就知道log中存储的block需要被写入到文件系统中，很明显我们在crash的时候并不一定完成了install log，我们可能是在commit之后，clean log之前crash的。
- 考虑crash的几种可能情况：
  1. 在第1步和第2步之间crash会发生什么？在重启的时候什么也不会做，就像系统调用从没有发生过一样，也像crash是在文件系统调用之前发生的一样。这完全可以，并且也是可接受的。
  2. 在第2步和第3步之间crash会发生什么？在这个时间点，所有的log block都落盘了，因为有commit记录，所以完整的文件系统操作必然已经完成了。我们可以将log block写入到文件系统中相应的位置，这样也不会破坏文件系统。所以这种情况就像系统调用正好在crash之前就完成了。
  3. 在install（第3步）过程中和第4步之前这段时间crash会发生什么？在下次重启的时候，我们会redo log，我们或许会再次将log block中的数据再次拷贝到文件系统。这样也是没问题的，因为log中的数据是固定的，我们就算重复写了文件系统，每次写入的数据也是不变的。重复写入并没有任何坏处，因为我们写入的数据可能本来就在文件系统中，所以多次install log完全没问题。当然在这个时间点，我们不能执行任何文件系统的系统调用。我们应该在重启文件系统之前，在重启或者恢复的过程中完成这里的恢复操作。换句话说，install log是幂等操作（注，idempotence，表示执行多次和执行一次效果一样），你可以执行任意多次，最后的效果都是一样的。
- write ahead rule：在写入commit记录之前，你需要确保所有的写操作都在log中
- 在XV6中，我们会看到数据有两种状态，是在磁盘上还是在内存中。内存中的数据会在crash或者电力故障之后丢失。
- 当文件系统在运行时，在内存中也有header block的一份拷贝，拷贝中也包含了n和block编号的数组。

## log_write函数
- 我们不应该在所有的写操作完成之前写入commit record。这意味着文件系统操作必须表明事务的开始和结束。
- 在XV6中，以创建文件的sys_open为例（在sysfile.c文件中）每个文件系统操作，都有begin_op和end_op分别表示事务的开始和结束。
- 并且事务中的所有写block操作具备原子性，这意味着这些写block操作要么全写入，要么全不写入。
- 在end_op中会实现commit操作。
- 在begin_op和end_op之间，磁盘上或者内存中的数据结构会更新。但是在end_op之前，并不会有实际的改变（注，也就是不会写入到实际的block中）。
- 在end_op时，我们会将数据写入到log中，之后再写入commit record或者log header
- log_write是由文件系统的logging实现的方法。任何一个文件系统调用的begin_op和end_op之间的写操作总是会走到log_write。log_write函数位于log.c文件，
- 任何文件系统调用，如果需要更新block或者说更新block cache中的block，都会将block编号加在这个内存数据中（注，也就是log header在内存中的cache），除非编号已经存在。

## end_op函数
- 如果我们在这个位置也就是在write_head之前crash了，那么最终的表现就像是transaction从来没有发生过。
- 将write_head称为commit point。
- write_head()
  1. 那crash发生在bwrite之后会发生什么呢？
  2. 这时header会写入到磁盘中，当重启恢复相应的文件系统操作会被恢复。在恢复过程的某个时间点，恢复程序可以读到log header并发现比如说有5个log还没有install，恢复程序可以将这5个log拷贝到实际的位置。所以这里的bwrite就是实际的commit point。
- 但 bread(log.dev, log.lh.block[tail]) 会优先返回 缓存中的脏块（如果存在），而非直接读磁盘
- 在commit函数中，install结束之后，会将log header中的n设置为0，再将log header写回到磁盘中。将n设置为0的效果就是清除log。

## File system recovering
- 当系统crash并重启了，在XV6启动过程中做的一件事情就是调用initlog函数。
- 如果我们在install_trans函数中又crash了，也不会有问题，因为之后再重启时，XV6会再次调用initlog函数，再调用recover_from_log来重新install log。如果我们在commit之前crash了多次，在最终成功commit时，log可能会install多次。

## Log写磁盘流程
- bwrite函数是block cache中实际写磁盘的函数，所以我们将会看到实际写磁盘的记录。
- 首先是前3行的bwrite 3，4，5。因为block 3是第一个log data block，所以前3行是在log中记录了3个写操作。这3个写操作都保存在log中，并且会写入到磁盘中的log部分。
- 第4行的bwrite 2。因为block 2是log的起始位置，也就是log header，所以这条是commit记录。
- 第5，6，7行的bwrite 33，46，32。这里实际就是将前3行的log data写入到实际的文件系统的block位置，这里实际是install log。
- 第8行的bwrite 2，是清除log（注，也就是将log header中的n设置为0）。到此为止，完成了实际上的写block 33，46，32这一系列的操作。第一部分是log write，第二部分是install log，每一部分后面还跟着一个更新commit记录（注，也就是commit log和clean log）。
- 这里所有的操作都在end_op中，只需要区分每一次end_op的调用就可以找到begin_op

## File system challenges
- 第一个是cache eviction。缓存驱逐
- 第二个挑战是，文件系统操作必须适配log的大小。在XV6中，总共有30个log block；因为block在落盘之前需要在cache中pin住，所以buffer cache的尺寸也要大于log的尺寸。
- 最后一个要讨论的挑战是并发文件系统调用。必须要保证多个并发transaction加在一起也适配log的大小。XV6通过限制并发文件系统操作的个数来实现这一点。在begin_op中，我们会检查当前有多少个文件系统操作正在进行。如果有太多正在进行的文件系统操作，我们会通过sleep停止当前文件系统操作的运行，并等待所有其他所有的文件系统操作都执行完并commit之后再唤醒。这里的其他所有文件系统操作都会一起commit。有的时候这被称为group commit
