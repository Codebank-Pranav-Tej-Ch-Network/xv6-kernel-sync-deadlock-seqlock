// Mutual exclusion lock.
struct spinlock {
  uint locked;       // Is the lock held?

  // For debugging:
  char *name;        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
  uint pcs[10];      // The call stack (an array of program counters)
                     // that locked the lock.
};

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


