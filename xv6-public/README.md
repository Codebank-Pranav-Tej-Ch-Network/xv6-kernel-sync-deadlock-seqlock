# xv6-public (Extended with Semaphores, WFG Deadlock Engine & Seqlocks)

This directory contains the modified **MIT xv6 (x86)** kernel source code and user-space verification suites for **CS309L Operating Systems Laboratory — Assignment 4** by **Ch Venkata Sai Pranav Tej (`CS24B057`)**.

For full project documentation, execution screenshots, and architectural diagrams, see:
- [**Root Repository README.md**](../README.md)
- [**End-to-End Technical Architecture (`docs/TECHNICAL_ARCHITECTURE.md`)**](../docs/TECHNICAL_ARCHITECTURE.md)

---

## Modified & Added Source Files

| File | Role |
| :--- | :--- |
| `param.h` | Defines `NSEM (32)` and `SEM_PGFLT_ID (0)` |
| `fcntl.h` | Defines `LOCK_SH`, `LOCK_EX`, `LOCK_NB`, `LOCK_UN`, `LOCK_PREEMPT`, `LOCK_ORDER`, `EDEADLK` |
| `spinlock.h` | Implements 64-byte aligned `seqlock_t` and `__sync_synchronize()` barrier primitives |
| `file.h` / `file.c` | Adds `locked_by_pid` and `lock_type` to `struct inode`; auto-releases locks in `fileclose()` |
| `proc.h` / `proc.c` | Adds `waiting_on_inode` and `creation_time` to `struct proc`; implements 3-policy WFG deadlock engine |
| `sysproc.c` | Implements counting semaphore table `sems[32]` and `sem_create`, `sem_wait`, `sem_post`, `icache_stat` |
| `trap.c` | Implements `T_PGFLT` (`Trap 14`) demand-paging handler and `pgflt_sem_notify()` |
| `sysfile.c` | Implements `sys_flock()` advisory file locking with WFG cycle detection |
| `fs.c` | Instruments `icache` and `iget()` with `seqlock_t` telemetry and $N=5$ retry spinlock fallback |
| `test_sem.c` | User-space test suite for Task 1 (Counting Semaphores & Page Fault Trap) |
| `test_deadlock.c` | User-space test suite for Tasks 2 & 3 (`flock` & 3 WFG Deadlock Policies) |
| `test_seqlock.c` | User-space test suite for Task 4 (`seqlock_t` Telemetry & Starvation Fallback) |

---

## Building & Running

```bash
make clean
make fs.img xv6.img
make qemu-nox
```

Inside the xv6 shell (`$`):
```sh
$ test_sem
$ test_deadlock
$ test_seqlock
```

---

## Original MIT xv6 Credits

xv6 is a re-implementation of Dennis Ritchie's and Ken Thompson's Unix Version 6 (v6), developed by Frans Kaashoek, Robert Morris, and Russ Cox at MIT PDOS (`https://github.com/mit-pdos/xv6-public`).
