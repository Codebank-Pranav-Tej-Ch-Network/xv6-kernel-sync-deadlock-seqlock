#define O_RDONLY  0x000
#define O_WRONLY  0x001
#define O_RDWR    0x002
#define O_CREATE  0x200

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
