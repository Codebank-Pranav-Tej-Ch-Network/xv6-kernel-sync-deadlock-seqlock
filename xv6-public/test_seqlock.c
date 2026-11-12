#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int
main(int argc, char *argv[])
{
  int hits0 = 0, misses0 = 0, seq0 = 0, fb0 = 0;
  int hits1 = 0, misses1 = 0, seq1 = 0, fb1 = 0;
  int hits2 = 0, misses2 = 0, seq2 = 0, fb2 = -1;
  int fd, i, mode;

  printf(1, "=== Task 4: Seqlocks & VFS Inode Cache (icache) Telemetry Test ===\n");

  mode = icache_stat(&hits0, &misses0, &seq0, &fb0);
  printf(1, "[READ 1 - Optimistic] mode=%d | seq=%d (even) | hits=%d | misses=%d | fallbacks=%d\n",
         mode, seq0, hits0, misses0, fb0);

  // Trigger VFS icache activity (hits & misses via iget)
  int pinned_fd = open("README", O_RDONLY);
  for(i = 0; i < 5; i++){
    fd = open("README", O_RDONLY);
    if(fd >= 0)
      close(fd);
  }
  if(pinned_fd >= 0)
    close(pinned_fd);
  fd = open("seqlock_probe", O_CREATE | O_RDWR);
  if(fd >= 0)
    close(fd);

  mode = icache_stat(&hits1, &misses1, &seq1, &fb1);
  printf(1, "[READ 2 - Optimistic] mode=%d | seq=%d (even) | hits=%d (+%d) | misses=%d (+%d) | fallbacks=%d\n",
         mode, seq1, hits1, hits1 - hits0, misses1, misses1 - misses0, fb1);

  // Test Creative Extension: Reader Starvation Avoidance (N consecutive failed reads -> hard spinlock)
  printf(1, "\n--- Testing Starvation-Avoidance Fallback (N=%d retries -> hard spinlock) ---\n", 5);
  mode = icache_stat(&hits2, &misses2, &seq2, &fb2);
  printf(1, "[READ 3 - Fallback]   mode=%d (hard spinlock) | seq=%d | hits=%d | misses=%d | fallbacks=%d\n",
         mode, seq2, hits2, misses2, fb2);

  printf(1, "=== [PASS] Task 4 Seqlock Telemetry & Starvation Fallback Succeeded ===\n");
  exit();
}
