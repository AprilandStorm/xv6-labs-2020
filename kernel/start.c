#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

void main();
void timerinit();

// entry.S needs one stack per CPU.
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

// scratch area for timer interrupt, one per CPU.
uint64 mscratch0[NCPU * 32];

// assembly code in kernelvec.S for machine-mode timer interrupt.
extern void timervec();

// entry.S jumps here in machine mode on stack0.
//系统启动代码，从machine模式切换到supervisor模式的初始化操作
void
start()
{
  // set M Previous Privilege mode to Supervisor, for mret.
  unsigned long x = r_mstatus();//读取mstatus(machine status)寄存器
  x &= ~MSTATUS_MPP_MASK;//将特权级（MPP）的位取消MACHINE模式
  x |= MSTATUS_MPP_S;//将特权级切换为S模式
  w_mstatus(x);//写入mstatus寄存器

  // set M Exception Program Counter to main, for mret.
  // requires gcc -mcmodel=medany
  w_mepc((uint64)main);

  // disable paging for now.
  w_satp(0);//禁用分页机制，satp是S模式的地址转换和保护寄存器。使用物理地址访问内存

  // delegate all interrupts and exceptions to supervisor mode.
  w_medeleg(0xffff);//异常委托寄存器， 0xffff表示所有16个异常/中断委托出去
  w_mideleg(0xffff);//中断委托寄存器
  w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);//SIE接收External,软件和定时器中断

  // ask for clock interrupts.
  timerinit();//初始化定时器

  // keep each CPU's hartid in its tp register, for cpuid().
  int id = r_mhartid();//读取当前CPU的hart ID
  w_tp(id);//将ID写入tp(线程指针)寄存器，为cpuid()提供hart ID

  // switch to supervisor mode and jump to main().
  asm volatile("mret");//跳转到main
}

// set up to receive timer interrupts in machine mode,
// which arrive at timervec in kernelvec.S,
// which turns them into software interrupts for
// devintr() in trap.c.
void
timerinit()
{
  // each CPU has a separate source of timer interrupts.
  int id = r_mhartid();

  // ask the CLINT for a timer interrupt.
  int interval = 1000000; // cycles; about 1/10th second in qemu.
  *(uint64*)CLINT_MTIMECMP(id) = *(uint64*)CLINT_MTIME + interval;

  // prepare information in scratch[] for timervec.
  // scratch[0..3] : space for timervec to save registers.
  // scratch[4] : address of CLINT MTIMECMP register.
  // scratch[5] : desired interval (in cycles) between timer interrupts.
  uint64 *scratch = &mscratch0[32 * id];
  scratch[4] = CLINT_MTIMECMP(id);
  scratch[5] = interval;
  w_mscratch((uint64)scratch);

  // set the machine-mode trap handler.
  w_mtvec((uint64)timervec);

  // enable machine-mode interrupts.
  w_mstatus(r_mstatus() | MSTATUS_MIE);

  // enable machine-mode timer interrupts.
  w_mie(r_mie() | MIE_MTIE);
}
