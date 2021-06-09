#ifndef MESSAGES_LIST_H
#define MESSAGES_LIST_H

/* This is the structure representing a message. A message is an entity
   that can be sent and received by threads. They are stored in a RCU
   list to preserve the correctness of writings e readings in an efficient
   way */
typedef struct _msg {
        struct _msg *next;
        char *msg;
        size_t size;
        int reading_threads;
} msg_t;

/* This structure allows an efficient management of the messages.
   It is based on the typical mechanism with epochs */
typedef struct _rcu_msg_list {
	int epoch;
	long standing[2];
        msg_t *head;
	spinlock_t write_spinlock;
} list_t;

/* --------- Messages management ---------- */
void rcu_messages_list_init(list_t *list);

msg_t *rcu_messages_list_insert(list_t *list, msg_t msg);

int rcu_messages_list_remove(list_t *list, msg_t *msg);

#endif
