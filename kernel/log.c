#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

// Simple logging that allows concurrent FS system calls.
//
// A log transaction contains the updates of multiple FS system
// calls. The logging system only commits when there are
// no FS system calls active. Thus there is never
// any reasoning required about whether a commit might
// write an uncommitted system call's updates to disk.
//
// A system call should call begin_op()/end_op() to mark
// its start and end. Usually begin_op() just increments
// the count of in-progress FS system calls and returns.
// But if it thinks the log is close to running out, it
// sleeps until the last outstanding end_op() commits.
//
// The log is a physical re-do log containing disk blocks.
// The on-disk log format:
//   header block, containing block #s for block A, B, C, ...
//   block A
//   block B
//   block C
//   ...
// Log appends are synchronous.

// Contents of the header block, used for both the on-disk header block
// and to keep track in memory of logged block# before commit.
struct logheader {
  int n;
  int block[LOGSIZE];
};

struct log {
  struct spinlock lock;
  int start;
  int size;
  int outstanding; // how many FS sys calls are executing.
  int committing;  // in commit(), please wait.
  int dev;
  struct logheader lh;
};
struct log log;

static void recover_from_log(void);
static void commit();

void
initlog(int dev, struct superblock *sb)
{
  if (sizeof(struct logheader) >= BSIZE)
    panic("initlog: too big logheader");

  initlock(&log.lock, "log");
  log.start = sb->logstart;
  log.size = sb->nlog;
  log.dev = dev;
  recover_from_log();
}

// Copy committed blocks from log to their home location
//负责将日志中的修改应用到实际文件系统的磁盘位置
static void
install_trans(int recovering)//参数 recovering：1 表示正在崩溃恢复（此时不减少块的引用计数）;0 表示正常事务提交（需要减少引用计数）
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *lbuf = bread(log.dev, log.start+tail+1); // read log block
    struct buf *dbuf = bread(log.dev, log.lh.block[tail]); // read dst
    memmove(dbuf->data, lbuf->data, BSIZE);  // copy block to dst
    bwrite(dbuf);  // write dst to disk
    if(recovering == 0)
      bunpin(dbuf);//减少目标块的引用计数（pin count）
    brelse(lbuf);
    brelse(dbuf);
  }
}

// Read the log header from disk into the in-memory log header
static void
read_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *lh = (struct logheader *) (buf->data);
  int i;
  log.lh.n = lh->n;
  for (i = 0; i < log.lh.n; i++) {
    log.lh.block[i] = lh->block[i];
  }
  brelse(buf);
}

// Write in-memory log header to disk.
// This is the true point at which the
// current transaction commits.
//将日志的元数据（即日志头 log header）写入磁盘
static void
write_head(void)
{
  struct buf *buf = bread(log.dev, log.start);//从磁盘读取一个块到缓存（buffer cache）
  struct logheader *hb = (struct logheader *) (buf->data);//获取日志头结构;
  int i;
  hb->n = log.lh.n;//写入日志块数;hb = header block
  for (i = 0; i < log.lh.n; i++) {
    hb->block[i] = log.lh.block[i];//log.lh.block[i]：日志中记录的第 i 个块的块号。将所有记录的块号复制到磁盘日志头的 block[] 数组中。目的：崩溃恢复时，系统知道哪些块需要从日志区域复制回实际位置
  }
  bwrite(buf);//将修改后的日志头缓存块（buf）写回磁盘;
  brelse(buf);
}

static void
recover_from_log(void)
{
  read_head();
  install_trans(1); // if committed, copy from log to disk
  log.lh.n = 0;
  write_head(); // clear the log
}

// called at the start of each FS system call.
void
begin_op(void)
{
  acquire(&log.lock);
  while(1){
    if(log.committing){
      sleep(&log, &log.lock);//如果日志正在提交（log.committing == 1），当前线程睡眠（sleep），等待提交完成。
    } else if(log.lh.n + (log.outstanding+1)*MAXOPBLOCKS > LOGSIZE){
      //log.lh.n：当前日志中已记录的块数。log.outstanding：未完成的事务数。MAXOPBLOCKS：单个事务允许的最大块数（预分配空间）。
      // this op might exhaust log space; wait for commit.
      sleep(&log, &log.lock);//睡眠等待，直到其他事务完成并释放日志空间（由 end_op() 唤醒）。
    } else {
      log.outstanding += 1;
      release(&log.lock);
      break;
    }
  }
}

// called at the end of each FS system call.
// commits if this was the last outstanding operation.
//标记当前事务结束，并根据条件触发日志提交。
void
end_op(void)
{
  int do_commit = 0;//标志位，初始为 0，表示是否需要提交日志（1 表示需要提交）。

  acquire(&log.lock);
  log.outstanding -= 1;//当前事务结束时，递减计数器。若计数器归零，表示所有事务已完成，可以提交日志。
  if(log.committing)//log.committing：标志位，表示日志是否正在提交到磁盘。
    panic("log.committing");
  if(log.outstanding == 0){//判断是否需要提交日志
    do_commit = 1;
    log.committing = 1;
  } else {//如果仍有未完成事务（log.outstanding > 0），则唤醒（wakeup(&log)）可能阻塞在 begin_op() 的线程（等待日志空间）
    // begin_op() may be waiting for log space,
    // and decrementing log.outstanding has decreased
    // the amount of reserved space.
    wakeup(&log);
  }
  release(&log.lock);

  if(do_commit){// 满足条件下提交日志
    // call commit w/o holding locks, since not allowed
    // to sleep with locks.
    commit();
    acquire(&log.lock);
    log.committing = 0;//提交完成后，更新日志状态;重置 log.committing = 0，允许新事务提交。
    wakeup(&log);//唤醒（wakeup(&log)）可能阻塞的线程（如等待提交完成的其他事务）。
    release(&log.lock);
  }
}

// Copy modified blocks from cache to log.
static void
write_log(void)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *to = bread(log.dev, log.start+tail+1); // log block;bread：从磁盘读取一个块到缓存（buffer cache）。log.start+tail+1：目标日志块的磁盘位置。log.start：日志区域的起始块号。+1：跳过日志头块（log header block，通常存储元数据如 log.lh.n）。
    struct buf *from = bread(log.dev, log.lh.block[tail]); // cache block
    memmove(to->data, from->data, BSIZE);
    bwrite(to);  // write the log;将缓存中的日志块（to）写入磁盘。
    brelse(from);
    brelse(to);
  }
}

static void
commit()
{
  if (log.lh.n > 0) {
    write_log();     // Write modified blocks from cache to log;这基本上就是将所有存在于内存中的log header中的block编号对应的block，从block cache写入到磁盘上的log区域中（注，也就是将变化先从内存拷贝到log中）
    write_head();    // Write header to disk -- the real commit;write_head会将内存中的log header写入到磁盘中
    install_trans(0); // Now install writes to home locations
    log.lh.n = 0;
    write_head();    // Erase the transaction from the log
  }
}

// Caller has modified b->data and is done with the buffer.
// Record the block number and pin in the cache by increasing refcnt.
// commit()/write_log() will do the disk write.
//
// log_write() replaces bwrite(); a typical use is:
//   bp = bread(...)
//   modify bp->data[]
//   log_write(bp)
//   brelse(bp)
void
log_write(struct buf *b)
{
  int i;

  if (log.lh.n >= LOGSIZE || log.lh.n >= log.size - 1)//log.lh.n：当前日志中已记录的块数。如果当前日志已满（log.lh.n >= LOGSIZE）或剩余空间不足（log.lh.n >= log.size - 1）
    panic("too big a transaction");
  if (log.outstanding < 1)//log.outstanding：当前未完成的事务数量。如果 log_write 被调用时没有活跃的事务（log.outstanding < 1），说明代码逻辑错误（例如在事务外写日志），触发 panic。
    panic("log_write outside of trans");

  acquire(&log.lock);//获取日志锁
  for (i = 0; i < log.lh.n; i++) {//遍历日志中已记录的块号，如果当前缓冲区 b 的块号 b->blockno 已存在，则退出循环（避免重复记录)
    if (log.lh.block[i] == b->blockno)   // log absorbtion; log.lh.block[]：数组，保存日志中已记录的块号。
      break;
  }
  log.lh.block[i] = b->blockno;//将当前缓冲区的块号 b->blockno 写入日志的块号数组 log.lh.block[]。
  if (i == log.lh.n) {  // Add new block to log?表示当前块号是新的（未在日志中记录过），需要扩展日志。
    bpin(b);//增加缓冲区 b 的引用计数（pin），防止被释放或重用，确保数据在日志提交前不会被覆盖。
    log.lh.n++;
  }
  release(&log.lock);
}

