#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void) {
    int n;
    if (argint(0, &n) < 0)
        return -1;
    exit(n);
    return 0;  // not reached
}

uint64
sys_getpid(void) {
    return myproc()->pid;
}

uint64
sys_fork(void) {
    return fork();
}

uint64
sys_wait(void) {
    uint64 p;
    if (argaddr(0, &p) < 0)
        return -1;
    return wait(p);
}

uint64
sys_sbrk(void) {
    int addr;
    int n;

    if (argint(0, &n) < 0)
        return -1;
    addr = myproc()->sz;
    if (growproc(n) < 0)
        return -1;
    return addr;
}

uint64
sys_sleep(void) {
    int n;
    uint ticks0;

    if (argint(0, &n) < 0)
        return -1;
    acquire(&tickslock);
    ticks0 = ticks;
    while (ticks - ticks0 < n) {
        if (myproc()->killed) {
            release(&tickslock);
            return -1;
        }
        sleep(&ticks, &tickslock);
    }
    release(&tickslock);
    backtrace();
    return 0;
}

uint64
sys_kill(void) {
    int pid;

    if (argint(0, &pid) < 0)
        return -1;
    return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void) {
    uint xticks;

    acquire(&tickslock);
    xticks = ticks;
    release(&tickslock);
    return xticks;
}

uint64
sys_sigalarm(void) {
    int alarm_interval;
    uint64 handler_ptr;

    if (argint(0, &alarm_interval) < 0 || argaddr(1, &handler_ptr) < 0)
        return -1;

    myproc()->alarm_interval = alarm_interval;
    myproc()->alarm_handler = handler_ptr;
    myproc()->ticks_passed = 0;

    return 0;
}

uint64
sys_sigreturn(void) {
    // TODO
    // recover trapframe from trapframe_backup before return back to user space
    // printf("size: %d %d\n", sizeof(struct trapframe), sizeof(myproc()->trapframe_backup));
    memmove(myproc()->trapframe, myproc()->trapframe_backup, sizeof(struct trapframe));
    myproc()->ticks_passed = 0;

    return 0;
}
