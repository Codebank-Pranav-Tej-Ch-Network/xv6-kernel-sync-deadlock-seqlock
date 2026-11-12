#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// ============================================================================
// Task 1: Counting Semaphores & Shared VM Page-Fault Buffer
// Cache-line aligned (64B) to prevent MESI false sharing / cache-line bouncing
// ============================================================================
struct sem {
  int value;
  struct spinlock lock;
  int active;
} __attribute__((aligned(64)));

static struct sem sems[NSEM];
static struct spinlock semtable_lock;
static int pgflt_sem_id = -1;
static uint pgflt_shared_buf[16];
static uint pgflt_write_idx = 0;

void
seminit(void)
{
  int i;
  initlock(&semtable_lock, "semtable");
  for(i = 0; i < NSEM; i++){
    initlock(&sems[i].lock, "sem");
    sems[i].value = 0;
    sems[i].active = 0;
  }
}

int
sem_create_k(int init_val)
{
  int i;
  if(init_val < 0)
    return -1;

  acquire(&semtable_lock);
  for(i = 0; i < NSEM; i++){
    if(!sems[i].active){
      acquire(&sems[i].lock);
      sems[i].value = init_val;
      sems[i].active = 1;
      release(&sems[i].lock);
      if(pgflt_sem_id < 0)
        pgflt_sem_id = i;
      release(&semtable_lock);
      return i;
    }
  }
  release(&semtable_lock);
  return -1;
}

int
sem_wait_k(int sem_id)
{
  if(sem_id < 0 || sem_id >= NSEM)
    return -1;

  acquire(&sems[sem_id].lock);
  if(!sems[sem_id].active){
    release(&sems[sem_id].lock);
    return -1;
  }
  while(sems[sem_id].value <= 0){
    if(myproc() && myproc()->killed){
      release(&sems[sem_id].lock);
      return -1;
    }
    sleep(&sems[sem_id], &sems[sem_id].lock);
  }
  sems[sem_id].value--;
  release(&sems[sem_id].lock);
  return 0;
}

int
sem_post_k(int sem_id)
{
  if(sem_id < 0 || sem_id >= NSEM)
    return -1;

  acquire(&sems[sem_id].lock);
  if(!sems[sem_id].active){
    release(&sems[sem_id].lock);
    return -1;
  }
  sems[sem_id].value++;
  wakeup(&sems[sem_id]);
  release(&sems[sem_id].lock);
  return 0;
}

void
pgflt_sem_notify(uint fault_va, int pid)
{
  int sid;
  acquire(&semtable_lock);
  sid = pgflt_sem_id;
  pgflt_shared_buf[pgflt_write_idx % 16] = fault_va;
  pgflt_write_idx++;
  release(&semtable_lock);

  if(sid >= 0 && sid < NSEM && sems[sid].active){
    cprintf("[TRAP T_PGFLT] PID %d faulted at VA 0x%x -> posting to sem[%d]\n",
            pid, fault_va, sid);
    sem_post_k(sid);
  }
}

int
sys_sem_create(void)
{
  int init_val;
  if(argint(0, &init_val) < 0)
    return -1;
  return sem_create_k(init_val);
}

int
sys_sem_wait(void)
{
  int sem_id;
  if(argint(0, &sem_id) < 0)
    return -1;
  return sem_wait_k(sem_id);
}

int
sys_sem_post(void)
{
  int sem_id;
  if(argint(0, &sem_id) < 0)
    return -1;
  return sem_post_k(sem_id);
}

int
sys_icache_stat(void)
{
  int *hits, *misses, *seq, *fallbacks;
  int sim_mode = 0;

  if(argptr(0, (void*)&hits, sizeof(int)) < 0 ||
     argptr(1, (void*)&misses, sizeof(int)) < 0 ||
     argptr(2, (void*)&seq, sizeof(int)) < 0 ||
     argptr(3, (void*)&fallbacks, sizeof(int)) < 0)
    return -1;

  // Optional 5th arg or initial *fallbacks < 0 triggers writer contention test
  if(*fallbacks < 0)
    sim_mode = 1;

  return icache_get_telemetry(hits, misses, seq, fallbacks, sim_mode);
}
