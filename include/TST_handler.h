#ifndef TST_HANDLER_H
#define TST_HANDLER_H

#include "constants.h"
#include "messages_list.h"

/* This spinlock allows to handle concurrent accesses to the TST */
static spinlock_t tst_spinlock;

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
   wq_t head; /* First element in the level queueing */
   spinlock_t queue_lock; /* Spinlock for threads queueing */
   spinlock_t msg_lock; /* Spinlock for messages accesses */
   list_t msg_list; /* List of messages sent on this level */
} level_t;

/* This structure represents a TAG service, a TST entry. */
typedef struct _tag_service {
   int key; /* The service identifier */
   int perm; /* The access permission for the service */
   pid_t owner; /* The service creator process */
   int priv; /* Is this service istantiated with IPC_PRIVATE? */
   int open; /* Is the service open? */
   level_t levels[LEVELS];
   spinlock_t awake_all_lock; /* Spinlock for threads awakening through tag_ctl syscall */
   rwlock_t send_lock; /* Read-write lock for message sendings */
   rwlock_t receive_lock; /* Read-write lock for message receivings */
   spinlock_t level_activation_lock[LEVELS]; /* Spinlock for level initialization */
} tag_t;

/* ----------- Management functions ----------- */

char *status_builder(void);

int create_service(int key, int permission);

int open_service(int key, int permission);

int send_message(int key, int level, char *buffer, size_t size);

char *receive_message(int key, int level, char* buffer, size_t size);

int awake_all_threads(int key);

int remove_tag(int key);

int TST_alloc(void);

void TST_dealloc(void);

#endif
