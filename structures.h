#include "constants.h"

/**/
typedef struct wq_elem {
   struct task_struct *task;    
   int pid;
   int awake;
   int already_hit;
   struct wq_elem *next;
} wq_t;

/**/
typedef struct level {
   int active; /* Is the level initialized? */
   int waiting_threads; /* Number of threads waiting for a message */
   wq_t first_elem; /* First element in the level queueing */
   spinlock_t queue_lock;
   rcul_t msg_list;
} level_t;

/* This structure represents a TAG service, a TST entry. */
typedef struct tag_service {
   int key; /* The service identifier */
   int perm; /* The access permission for the service */
   pid_t owner; /* The service creator thread */
   int priv; /* Is this service istantiated with IPC_PRIVATE? */
   level_t levels[LEVELS];
   spinlock_t level_activation_spinlock[LEVELS]; /* Spinlock for level initialization */
} tag_t;
