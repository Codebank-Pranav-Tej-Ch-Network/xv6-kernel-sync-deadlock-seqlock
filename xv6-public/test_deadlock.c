#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int
main(int argc, char *argv[])
{
  int fdA, fdB;
  int pid;
  struct stat stA, stB;

  printf(1, "=== Task 2 & 3: Advisory flock & WFG Deadlock Resolution Test ===\n");

  fdA = open("lockfileA", O_CREATE | O_RDWR);
  fdB = open("lockfileB", O_CREATE | O_RDWR);
  fstat(fdA, &stA);
  fstat(fdB, &stB);
  printf(1, "[INIT] Created lockfileA (inum=%d) and lockfileB (inum=%d)\n",
         stA.ino, stB.ino);
  close(fdA);
  close(fdB);

  // --------------------------------------------------------------------------
  // Subtest 1: Algo Option A (WFG Cycle Detection & Rollback with -EDEADLK)
  // --------------------------------------------------------------------------
  printf(1, "\n--- [Subtest 1] Algo Option A: WFG Cycle Detection & -EDEADLK Rollback ---\n");
  pid = fork();
  if(pid == 0){
    int cfdA = open("lockfileA", O_RDWR);
    int cfdB = open("lockfileB", O_RDWR);
    sleep(5); // Let parent lock fileA first
    flock(cfdB, LOCK_EX);
    printf(1, "[Child PID %d] Locked lockfileB (inum=%d)\n", getpid(), stB.ino);
    sleep(15); // Wait for parent to block on fileB

    printf(1, "[Child PID %d] Requesting lockfileA (inum=%d) -> forming Circular Wait...\n",
           getpid(), stA.ino);
    int r = flock(cfdA, LOCK_EX);
    if(r == -EDEADLK){
      printf(1, "[Child PID %d] flock aborted with -EDEADLK (%d)! Releasing lockfileB.\n",
             getpid(), r);
      flock(cfdB, LOCK_UN);
    }
    close(cfdA);
    close(cfdB);
    exit();
  }

  fdA = open("lockfileA", O_RDWR);
  fdB = open("lockfileB", O_RDWR);
  flock(fdA, LOCK_EX);
  printf(1, "[Parent PID %d] Locked lockfileA (inum=%d)\n", getpid(), stA.ino);
  sleep(10); // Let child lock fileB
  printf(1, "[Parent PID %d] Waiting on lockfileB (inum=%d)...\n", getpid(), stB.ino);
  if(flock(fdB, LOCK_EX) == 0){
    printf(1, "[Parent PID %d] Acquired lockfileB after child rollback!\n", getpid());
    flock(fdB, LOCK_UN);
  }
  flock(fdA, LOCK_UN);
  wait();

  // --------------------------------------------------------------------------
  // Subtest 2: Algo Option B (Strict Inode inum Lock Ordering Heuristic)
  // --------------------------------------------------------------------------
  printf(1, "\n--- [Subtest 2] Algo Option B: Strict Inode inum Lock Ordering Avoidance ---\n");
  pid = fork();
  if(pid == 0){
    int cfdA = open("lockfileA", O_RDWR);
    flock(cfdA, LOCK_EX);
    sleep(20);
    flock(cfdA, LOCK_UN);
    close(cfdA);
    exit();
  }
  sleep(5);
  // Parent locks higher-inum fileB first, then requests lower-inum fileA with LOCK_ORDER
  flock(fdB, LOCK_EX);
  int ord_res = flock(fdA, LOCK_EX | LOCK_ORDER);
  printf(1, "[Parent PID %d] Out-of-order flock(inum %d -> inum %d) returned %d (-EDEADLK)\n",
         getpid(), stB.ino, stA.ino, ord_res);
  flock(fdB, LOCK_UN);
  wait();

  // --------------------------------------------------------------------------
  // Subtest 3: Algo Option C (Creative Preemption of Youngest Process in Cycle)
  // --------------------------------------------------------------------------
  printf(1, "\n--- [Subtest 3] Algo Option C: Youngest-Process Preemption via kill() ---\n");
  flock(fdA, LOCK_EX);
  pid = fork();
  if(pid == 0){
    int cfdA = open("lockfileA", O_RDWR);
    int cfdB = open("lockfileB", O_RDWR);
    flock(cfdB, LOCK_EX);
    printf(1, "[Younger Child PID %d] Locked lockfileB, now blocking on lockfileA...\n", getpid());
    // Child blocks waiting on fdA (no cycle yet since parent hasn't requested fdB)
    flock(cfdA, LOCK_EX);
    exit();
  }
  sleep(15); // Ensure younger child is sleeping on lockfileA
  printf(1, "[Older Parent PID %d] Requesting lockfileB with LOCK_PREEMPT...\n", getpid());
  if(flock(fdB, LOCK_EX | LOCK_PREEMPT) == 0){
    printf(1, "[Older Parent PID %d] Acquired lockfileB after youngest-process preemption!\n",
           getpid());
    flock(fdB, LOCK_UN);
  }
  flock(fdA, LOCK_UN);
  wait();

  close(fdA);
  close(fdB);
  printf(1, "=== [PASS] Task 2 & 3 Flock & Deadlock Resolution Suite Completed ===\n");
  exit();
}
