#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <sys/time.h>

#define NBUCKET 5
#define NKEYS 100000

pthread_mutex_t locks[NBUCKET];

struct entry {
  int key;
  int value;
  struct entry *next;
};
struct entry *table[NBUCKET];
int keys[NKEYS];
int nthread = 1;

double
now()
{
 struct timeval tv;
 gettimeofday(&tv, 0);
 return tv.tv_sec + tv.tv_usec / 1000000.0;
}

static void 
insert(int key, int value, struct entry **p, struct entry *n)
{
  struct entry *e = malloc(sizeof(struct entry));
  e->key = key;
  e->value = value;
  e->next = n;
  *p = e;
}

static 
void put(int key, int value)//将（key,value)插入哈希表中
{
  int i = key % NBUCKET;
  
  pthread_mutex_lock(&locks[i]);//获取锁

  // is the key already present?
  struct entry *e = 0;
  for (e = table[i]; e != 0; e = e->next) {
    if (e->key == key)
      break;
  }
  if(e){
    // update the existing key.
    e->value = value;
  } else {
    // the new is new.
    insert(key, value, &table[i], table[i]);
  }

  pthread_mutex_unlock(&locks[i]);//释放锁
}

static struct entry*
get(int key)
{
  int i = key % NBUCKET;
  
  pthread_mutex_lock(&locks[i]);

  struct entry *e = 0;
  for (e = table[i]; e != 0; e = e->next) {
    if (e->key == key) break;
  }
  
  pthread_mutex_unlock(&locks[i]);//必须在return之前解锁，否则函数返回了，锁没有释放，程序进入死锁状态
  return e;
}

static void *
put_thread(void *xa)
{
  int n = (int) (long) xa; // thread number
  int b = NKEYS/nthread;

  for (int i = 0; i < b; i++) {
    put(keys[b*n + i], n);
  }

  return NULL;
}

static void *
get_thread(void *xa)
{
  int n = (int) (long) xa; // thread number
  int missing = 0;

  for (int i = 0; i < NKEYS; i++) {
    struct entry *e = get(keys[i]);
    if (e == 0) missing++;
  }
  printf("%d: %d keys missing\n", n, missing);
  return NULL;
}

int
main(int argc, char *argv[])//初始化多个互斥锁；根据用户指定的线程数nthread启动多个线程；分配key[]数组，每个线程负责一部分key;测量打印put操作所用时间和速率以及get所用时间和速率
{
  pthread_t *tha;
  void *value;
  double t1, t0;
  
  for(int i = 0; i < NBUCKET; i++){//初始化互斥锁
    pthread_mutex_init(&locks[i], NULL);
  }

  if (argc < 2) {
    fprintf(stderr, "Usage: %s nthreads\n", argv[0]);
    exit(-1);
  }
  nthread = atoi(argv[1]);//从命令行获取线程数量
  tha = malloc(sizeof(pthread_t) * nthread);//动态申请一个 pthread_t 数组 tha[] 用于保存线程 ID
  srandom(0);//设置伪随机种子
  assert(NKEYS % nthread == 0);//要求 NKEYS 能整除 nthread，确保每线程任务均匀
  for (int i = 0; i < NKEYS; i++) {//生成 NKEYS 个 key，保存在 keys[] 中
    keys[i] = random();
  }

  //
  // first the puts
  //
  t0 = now();
  for(int i = 0; i < nthread; i++) {
    assert(pthread_create(&tha[i], NULL, put_thread, (void *) (long) i) == 0);//创建线程
  }
  for(int i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);//等待线程结束
  }
  t1 = now();

  printf("%d puts, %.3f seconds, %.0f puts/second\n",
         NKEYS, t1 - t0, NKEYS / (t1 - t0));

  //
  // now the gets
  //
  t0 = now();
  for(int i = 0; i < nthread; i++) {
    assert(pthread_create(&tha[i], NULL, get_thread, (void *) (long) i) == 0);
  }
  for(int i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);
  }
  t1 = now();

  printf("%d gets, %.3f seconds, %.0f gets/second\n",
         NKEYS*nthread, t1 - t0, (NKEYS*nthread) / (t1 - t0));
}
