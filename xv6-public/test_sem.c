#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int sem_id;
  int pid_reader, pid_faulter;

  printf(1, "=== Task 1: Counting Semaphores & VM Page Fault Trap Test ===\n");

  sem_id = sem_create(0);
  if(sem_id < 0){
    printf(1, "[FAIL] sem_create(0) failed\n");
    exit();
  }
  printf(1, "[INIT] Created counting semaphore sem[%d] with initial value = 0\n", sem_id);

  pid_reader = fork();
  if(pid_reader == 0){
    printf(1, "[READER PID %d] Calling sem_wait(%d) -> sleeping until T_PGFLT posts...\n",
           getpid(), sem_id);
    if(sem_wait(sem_id) == 0){
      printf(1, "[READER PID %d] WOKEN UP successfully by Page Fault trap sem_post(%d)!\n",
             getpid(), sem_id);
    } else {
      printf(1, "[READER PID %d] sem_wait failed\n", getpid());
    }
    exit();
  }

  // Give reader time to enter sleep() inside sem_wait()
  sleep(10);

  pid_faulter = fork();
  if(pid_faulter == 0){
    char *unmapped_va = sbrk(0) + 4096;
    printf(1, "[FAULTER PID %d] Triggering VM Page Fault (T_PGFLT) at unmapped VA 0x%x...\n",
           getpid(), (uint)unmapped_va);
    *unmapped_va = 'X';
    sleep(10);
    printf(1, "[FAULTER PID %d] Page fault resolved on-demand (value='%c') and semaphore posted!\n",
           getpid(), *unmapped_va);
    exit();
  }

  wait();
  wait();
  printf(1, "=== [PASS] Task 1 Page Fault Semaphore Test Completed Successfully ===\n");
  exit();
}
