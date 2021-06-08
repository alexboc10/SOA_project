#include "constants.h"
#include "rcu_list.h"

/* This spinlock allows to handle concurrent accesses to the TST */
spinlock_t tst_spinlock;

/* An element of the wait queue list. It allows to handle the sleeping thread 
   during the service usage */
typedef struct _wq_elem {
   struct task_struct *task;
   int pid;
   int awake;
   int already_hit;
   struct _wq_elem *next;
} wq_t;

/* A level can be seen as a specific pipe belonging to a tag service.
   Every thread can send and receive messages in the context of a level */
typedef struct _level {
   int active; /* Is the level initialized? */
   int waiting_threads; /* Number of threads waiting for a message */
   wq_t first_elem; /* First element in the level queueing */
   spinlock_t queue_spinlock;
   spinlock_t msg_spinlock;
   list_t msg_list; /* List of messages sent on this level */
} level_t;

/* This structure represents a TAG service, a TST entry. */
typedef struct _tag_service {
   int key; /* The service identifier */
   int perm; /* The access permission for the service */
   pid_t owner; /* The service creator thread */
   int priv; /* Is this service istantiated with IPC_PRIVATE? */
   level_t levels[LEVELS];
   spinlock_t level_activation_spinlock[LEVELS]; /* Spinlock for level initialization */
} tag_t;
