# Assignment 4 (CS309L)

- **Name:** Ch Venkata Sai Pranav Tej
- **Roll Number:** CS24B057
- **Course:** CS309L Operating Systems Lab — Assignment 4

---

## Part A: Source Code Modifications

### Task 1: Counting Semaphores & the Page Fault Trap

#### 1. `param.h`
```c
#define NBUF         (MAXOPBLOCKS*3)  // size of disk block cache
#define FSSIZE       1000  // size of file system in blocks
#define NSEM         32  // maximum number of kernel counting semaphores
```

#### 2. `syscall.h`
```c
#define SYS_mkdir  20
#define SYS_close  21
#define SYS_sem_create 22
#define SYS_sem_wait   23
#define SYS_sem_post   24
#define SYS_flock      25
#define SYS_icache_stat 26
```

#### 3. `syscall.c`
```c
extern int sys_sem_create(void);
extern int sys_sem_wait(void);
extern int sys_sem_post(void);
extern int sys_flock(void);
extern int sys_icache_stat(void);

static int (*syscalls[])(void) = {
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
[SYS_wait]    sys_wait,
[SYS_pipe]    sys_pipe,
[SYS_read]    sys_read,
[SYS_kill]    sys_kill,
[SYS_exec]    sys_exec,
[SYS_fstat]   sys_fstat,
[SYS_chdir]   sys_chdir,
[SYS_dup]     sys_dup,
[SYS_getpid]  sys_getpid,
[SYS_sbrk]    sys_sbrk,
[SYS_sleep]   sys_sleep,
[SYS_uptime]  sys_uptime,
[SYS_open]    sys_open,
[SYS_write]   sys_write,
[SYS_mknod]   sys_mknod,
[SYS_unlink]  sys_unlink,
[SYS_link]    sys_link,
[SYS_mkdir]   sys_mkdir,
[SYS_close]   sys_close,
[SYS_sem_create] sys_sem_create,
[SYS_sem_wait]   sys_sem_wait,
[SYS_sem_post]   sys_sem_post,
[SYS_flock]      sys_flock,
[SYS_icache_stat] sys_icache_stat,
};
```

#### 4. `usys.S`
```asm
SYSCALL(sem_create)
SYSCALL(sem_wait)
SYSCALL(sem_post)
SYSCALL(flock)
SYSCALL(icache_stat)
```

#### 5. `user.h`
```c
int sem_create(int init_val);
int sem_wait(int sem_id);
int sem_post(int sem_id);
int flock(int fd, int op);
int icache_stat(int *hits, int *misses, int *seq, int *fallbacks);
```

#### 6. `defs.h`
```c
// fs.c
int             icache_get_telemetry(int *hits, int *misses, int *seq, int *fallbacks, int simulate_contention);

// proc.c
int             wfg_check_and_resolve(struct proc *requester, struct inode *target_ip, int policy);
int             wfg_check_lock_order(struct proc *requester, struct inode *target_ip);

// sysproc.c (Counting Semaphores)
void            seminit(void);
int             sem_create_k(int init_val);
int             sem_wait_k(int sem_id);
int             sem_post_k(int sem_id);
void            pgflt_sem_notify(uint fault_va, int pid);
```

#### 7. `sysproc.c`
```c
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
```

#### 8. `trap.c`
```c
  case T_PGFLT:
    {
      uint fault_va = rcr2();
      uint page_base = PGROUNDDOWN(fault_va);
      if(myproc() != 0 && (tf->cs & 3) == DPL_USER && fault_va < KERNBASE){
        // Resolve demand page fault by mapping physical frame(s) up to fault page
        uint oldsz = myproc()->sz;
        uint newsz = page_base + PGSIZE;
        if(newsz > oldsz && allocuvm(myproc()->pgdir, oldsz, newsz) != 0){
          myproc()->sz = newsz;
          switchuvm(myproc());
          pgflt_sem_notify(fault_va, myproc()->pid);
          break;
        }
      }
      pgflt_sem_notify(fault_va, myproc() ? myproc()->pid : 0);
      if(myproc())
        myproc()->killed = 1;
      break;
    }
```

---

### Task 2: Filelocks (`flock`)

#### 1. `fcntl.h`
```c
// Advisory file lock (flock) operation flags & deadlock policy flags
#define LOCK_SH   0x001   // Shared file lock
#define LOCK_EX   0x002   // Exclusive file lock
#define LOCK_NB   0x004   // Non-blocking request
#define LOCK_UN   0x008   // Unlock file
#define LOCK_PREEMPT 0x010 // Option C: Preempt youngest process in WFG cycle
#define LOCK_ORDER   0x020 // Option B: Enforce strict inode inum lock ordering

#define EDEADLK   35      // Resource deadlock would occur
#define EWOULDBLOCK 11    // Operation would block
#define EINVAL    22      // Invalid argument / lock ordering violation
```

#### 2. `file.h`
```c
// in-memory copy of an inode
struct inode {
  uint dev;           // Device number
  uint inum;          // Inode number
  int ref;            // Reference count
  struct sleeplock lock; // protects everything below here
  int valid;          // inode has been read from disk?
  uint locked_by_pid; // Task 2: PID holding advisory flock (0 if unlocked)
  int lock_type;      // Task 2: Advisory lock type (LOCK_SH, LOCK_EX, etc.)

  short type;         // copy of disk inode
  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT+1];
};
```

#### 3. `file.c`
```c
  if(ff.type == FD_PIPE)
    pipeclose(ff.pipe, ff.writable);
  else if(ff.type == FD_INODE){
    // Task 2: Clean up advisory file lock if held by closing process before iput
    ilock(ff.ip);
    if(myproc() && ff.ip->locked_by_pid == (uint)myproc()->pid){
      acquire(&ff.ip->lock.lk);
      ff.ip->locked_by_pid = 0;
      ff.ip->lock_type = 0;
      wakeup(&ff.ip->locked_by_pid);
      release(&ff.ip->lock.lk);
    }
    iunlock(ff.ip);

    begin_op();
    iput(ff.ip);
    end_op();
  }
```

#### 4. `sysfile.c`
```c
// Task 2 & Task 3: UNIX-style advisory file locking (flock) with WFG cycle detection
int
sys_flock(void)
{
  int fd, op;
  struct file *f;
  struct inode *ip;
  struct proc *curproc = myproc();

  if(argfd(0, &fd, &f) < 0 || argint(1, &op) < 0)
    return -1;
  if(f->type != FD_INODE || (ip = f->ip) == 0)
    return -1;

  ilock(ip);

  // Unlock operation
  if(op & LOCK_UN){
    if(ip->locked_by_pid == (uint)curproc->pid){
      acquire(&ip->lock.lk);
      ip->locked_by_pid = 0;
      ip->lock_type = 0;
      wakeup(&ip->locked_by_pid);
      release(&ip->lock.lk);
    }
    iunlock(ip);
    return 0;
  }

  // Acquire advisory lock (blocking or non-blocking, with WFG deadlock check)
  while(ip->locked_by_pid != 0 && ip->locked_by_pid != (uint)curproc->pid){
    if(op & LOCK_NB){
      iunlock(ip);
      return -EWOULDBLOCK;
    }

    // Algo Option B: Strict Lock Ordering check by inode inum
    if((op & LOCK_ORDER) && wfg_check_lock_order(curproc, ip) < 0){
      iunlock(ip);
      return -EDEADLK;
    }

    // Task 3: Inject WFG cycle-check trigger before sleep()
    int wfg_res = wfg_check_and_resolve(curproc, ip, op);
    if(wfg_res < 0){
      iunlock(ip);
      return wfg_res; // Returns -EDEADLK
    }

    // Release inode sleeplock before sleeping on advisory lock channel
    iunlock(ip);
    acquire(&ip->lock.lk);
    if(ip->locked_by_pid != 0 && ip->locked_by_pid != (uint)curproc->pid){
      sleep(&ip->locked_by_pid, &ip->lock.lk);
    }
    release(&ip->lock.lk);

    curproc->waiting_on_inode = 0;
    if(curproc->killed)
      return -1;

    ilock(ip);
  }

  acquire(&ip->lock.lk);
  ip->locked_by_pid = curproc->pid;
  ip->lock_type = op & (LOCK_SH | LOCK_EX);
  release(&ip->lock.lk);

  iunlock(ip);
  return 0;
}
```

---

### Task 3: Deadlock Simulation & Creative Resolution

#### 1. `proc.h`
```c
struct proc {
  uint sz;                     // Size of process memory (bytes)
  pde_t* pgdir;                // Page table
  char *kstack;                // Bottom of kernel stack for this process
  enum procstate state;        // Process state
  int pid;                     // Process ID
  struct proc *parent;         // Parent process
  struct trapframe *tf;        // Trap frame for current syscall
  struct context *context;     // swtch() here to run process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)
  struct inode *waiting_on_inode; // Task 3: Directed edge in Wait-For Graph (WFG)
  uint creation_time;          // Task 3 (Option C): Tick timestamp for youngest-proc heuristic
};
```

#### 2. `proc.c`
```c
// ============================================================================
// Task 3: Wait-For Graph (WFG) Deadlock Detection, Avoidance & Resolution
// ============================================================================

// Helper: find active process in ptable by PID (caller must hold ptable.lock)
static struct proc*
wfg_find_proc_locked(uint pid)
{
  struct proc *p;
  if(pid == 0)
    return 0;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state != UNUSED && p->state != ZOMBIE && (uint)p->pid == pid)
      return p;
  }
  return 0;
}

// Algo Option B (Creative Avoidance): Strict Lock Ordering Heuristic
// Enforces ascending acquisition order over inode->inum addresses.
int
wfg_check_lock_order(struct proc *requester, struct inode *target_ip)
{
  int fd;
  struct file *f;
  struct inode *held_ip;

  if(requester == 0 || target_ip == 0)
    return 0;

  for(fd = 0; fd < NOFILE; fd++){
    f = requester->ofile[fd];
    if(f && f->type == FD_INODE && f->ip){
      held_ip = f->ip;
      if(held_ip != target_ip &&
         held_ip->locked_by_pid == (uint)requester->pid &&
         held_ip->inum > target_ip->inum){
        cprintf("[WFG OPTION B: LOCK ORDERING VIOLATION] PID %d holds inum %d "
                "and requested lower inum %d -> avoiding potential deadlock (-EDEADLK)\n",
                requester->pid, held_ip->inum, target_ip->inum);
        return -EDEADLK;
      }
    }
  }
  return 0;
}

// Algo Option A (DFS/BFS Cycle Detection & Rollback) +
// Algo Option C (Creative Resolution: Youngest-Process Preemption via kill)
// Note: Acquires ptable.lock before traversing process states.
int
wfg_check_and_resolve(struct proc *requester, struct inode *target_ip, int policy)
{
  struct proc *curr_proc;
  struct inode *curr_ip;
  struct proc *youngest_proc;
  int depth = 0;

  if(requester == 0 || target_ip == 0)
    return 0;

  acquire(&ptable.lock);

  requester->waiting_on_inode = target_ip;
  curr_ip = target_ip;
  youngest_proc = requester;

  // Traverse directed Wait-For Graph: Proc -> waiting_on_inode -> locked_by_pid
  while(curr_ip != 0 && depth < NPROC){
    uint holder_pid = curr_ip->locked_by_pid;
    if(holder_pid == 0)
      break;

    curr_proc = wfg_find_proc_locked(holder_pid);
    if(curr_proc == 0)
      break;

    // Track youngest process in the chain (by creation_time, tie-breaking by higher PID)
    if(curr_proc->creation_time > youngest_proc->creation_time ||
       (curr_proc->creation_time == youngest_proc->creation_time &&
        curr_proc->pid > youngest_proc->pid)){
      youngest_proc = curr_proc;
    }

    // Cycle detected if edge routes back to requester
    if(curr_proc == requester){
      cprintf("[WFG CYCLE DETECTED] PID %d --(waits inum %d)--> PID %d "
              "--(waits inum %d)--> PID %d\n",
              requester->pid, target_ip->inum,
              target_ip->locked_by_pid,
              curr_ip->inum, requester->pid);

      if(policy & LOCK_PREEMPT){
        // Algo Option C: Preemptively send kill signal to youngest process in cycle
        cprintf("[WFG OPTION C: PREEMPTION] Youngest process in cycle is PID %d "
                "(creation_time=%d) -> sending preemptive kill signal\n",
                youngest_proc->pid, youngest_proc->creation_time);
        youngest_proc->killed = 1;
        if(youngest_proc->state == SLEEPING)
          youngest_proc->state = RUNNABLE;

        if(youngest_proc == requester){
          requester->waiting_on_inode = 0;
          release(&ptable.lock);
          return -EDEADLK;
        }
        release(&ptable.lock);
        return 1;
      }

      // Algo Option A: Detection & Rollback (-EDEADLK)
      cprintf("[WFG OPTION A: ROLLBACK] Aborting flock for PID %d with -EDEADLK (-%d)\n",
              requester->pid, EDEADLK);
      requester->waiting_on_inode = 0;
      release(&ptable.lock);
      return -EDEADLK;
    }

    curr_ip = curr_proc->waiting_on_inode;
    depth++;
  }

  release(&ptable.lock);
  return 0;
}
```

---

### Task 4: Seqlocks & the Inode Cache

#### 1. `spinlock.h`
```c
// ============================================================================
// Task 4: Seqlock with Memory Barriers & Reader Starvation-Avoidance Fallback
// Aligned to 64-byte hardware cache line to prevent SMP cache-line bouncing
// ============================================================================
#define SEQLOCK_MAX_RETRIES 5

typedef struct {
  uint sequence;
  struct spinlock lock;
} __attribute__((aligned(64))) seqlock_t;

#define seqlock_init(sl, lkname) \
  do { \
    (sl)->sequence = 0; \
    initlock(&(sl)->lock, (lkname)); \
  } while(0)

// Writer: acquires spinlock, increments sequence to Odd, issues memory barrier
#define seqlock_write_begin(sl) \
  do { \
    acquire(&(sl)->lock); \
    (sl)->sequence++; \
    __sync_synchronize(); \
  } while(0)

// Writer: issues memory barrier, increments sequence to Even, releases spinlock
#define seqlock_write_end(sl) \
  do { \
    __sync_synchronize(); \
    (sl)->sequence++; \
    release(&(sl)->lock); \
  } while(0)

// Reader: fetches sequence and issues full memory barrier
static inline uint
seqlock_read_begin(seqlock_t *sl)
{
  uint seq = *(volatile uint *)&sl->sequence;
  __sync_synchronize();
  return seq;
}

// Reader: issues memory barrier and returns 1 if sequence was Odd or changed
static inline int
seqlock_read_retry(seqlock_t *sl, uint start_seq)
{
  __sync_synchronize();
  return (start_seq & 1) || (*(volatile uint *)&sl->sequence != start_seq);
}
```

#### 2. `fs.c`
```c
struct {
  struct spinlock lock;
  struct inode inode[NINODE];
  // Task 4: Embedded VFS icache telemetry protected by seqlock_t
  seqlock_t seqlock;
  uint hits;
  uint misses;
  uint fallback_reads;
} icache;

static struct inode*
iget(uint dev, uint inum)
{
  struct inode *ip, *empty;

  acquire(&icache.lock);

  // Is the inode already cached?
  empty = 0;
  for(ip = &icache.inode[0]; ip < &icache.inode[NINODE]; ip++){
    if(ip->ref > 0 && ip->dev == dev && ip->inum == inum){
      ip->ref++;
      // Task 4: Writer-side seqlock update on icache hit
      seqlock_write_begin(&icache.seqlock);
      icache.hits++;
      seqlock_write_end(&icache.seqlock);
      release(&icache.lock);
      return ip;
    }
    if(empty == 0 && ip->ref == 0)    // Remember empty slot.
      empty = ip;
  }

  // Recycle an inode cache entry.
  if(empty == 0)
    panic("iget: no inodes");

  ip = empty;
  ip->dev = dev;
  ip->inum = inum;
  ip->ref = 1;
  ip->valid = 0;
  ip->locked_by_pid = 0;
  ip->lock_type = 0;
  // Task 4: Writer-side seqlock update on icache miss
  seqlock_write_begin(&icache.seqlock);
  icache.misses++;
  seqlock_write_end(&icache.seqlock);
  release(&icache.lock);

  return ip;
}

// Task 4: Seqlock optimistic lock-free reader with N-retry Starvation-Avoidance Fallback
int
icache_get_telemetry(int *hits, int *misses, int *seq, int *fallbacks, int simulate_contention)
{
  uint start_seq;
  uint h = 0, m = 0;
  int retries = 0;

  if(simulate_contention){
    icache.seqlock.sequence++; // Force odd sequence during optimistic retries
    __sync_synchronize();
  }

  while(retries < SEQLOCK_MAX_RETRIES){
    start_seq = seqlock_read_begin(&icache.seqlock);
    if((start_seq & 1) == 0){
      h = *(volatile uint *)&icache.hits;
      m = *(volatile uint *)&icache.misses;
      if(!seqlock_read_retry(&icache.seqlock, start_seq)){
        *hits = (int)h;
        *misses = (int)m;
        *seq = (int)start_seq;
        *fallbacks = (int)icache.fallback_reads;
        return 0; // Optimistic lock-free read succeeded!
      }
    }
    retries++;
  }

  if(simulate_contention){
    __sync_synchronize();
    icache.seqlock.sequence++; // Restore even sequence before hard spinlock handoff
  }

  // Starvation Avoidance Fallback: failed N consecutive optimistic reads -> acquire hard spinlock
  acquire(&icache.seqlock.lock);
  icache.fallback_reads++;
  h = icache.hits;
  m = icache.misses;
  start_seq = icache.seqlock.sequence;
  *hits = (int)h;
  *misses = (int)m;
  *seq = (int)start_seq;
  *fallbacks = (int)icache.fallback_reads;
  cprintf("[SEQLOCK FALLBACK] Reader failed %d consecutive optimistic reads -> "
          "acquired hard spinlock (seq=%d, fallbacks=%d)\n",
          retries, start_seq, icache.fallback_reads);
  release(&icache.seqlock.lock);
  return 1; // Indicates fallback spinlock path was taken
}
```

---

## Part B: Execution Screenshots

### i. Page Fault Semaphore Posting and Waking a Reader (`test_sem`)

SCREENSHOT_1_PLACEHOLDER

### ii. WFG Deadlock Cycle Detection, Avoidance, and Preemption (`test_deadlock`)

SCREENSHOT_2_PLACEHOLDER

### iii. Seqlock Telemetry Read and Starvation-Avoidance Fallback (`test_seqlock`)

SCREENSHOT_3_PLACEHOLDER
