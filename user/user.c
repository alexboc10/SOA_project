#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <syscall.h>
#include <sys/ipc.h>

#include "../include/constants.h"

#define TAG_GET 134
#define TAG_SEND 174
#define TAG_RECEIVE 182
#define TAG_CTL 183

/* All the tests return void in order to make then usable as thread jobs */

typedef struct _get_args{
        int key;
        int perm;
} get_args;

typedef struct _exchange_args{
        int tag;
        int level;
        char *buff;
        size_t size;
} exchange_args;

typedef struct _ctl_args{
        int tag;
} ctl_args;

void service_removing();
void threads_awakening();

void *create_service(void *params) {
   int ret;
   get_args *args = (get_args *) params;

   ret = syscall(TAG_GET, args->key, CMD_CREATE, args->perm);

   if (ret == -1) {
      printf("Thread %ld: service %d creation failed\n", syscall(SYS_gettid), args->key);
   } else {
      printf("Thread %ld: service %d creation succeed\n", syscall(SYS_gettid), args->key);
   }
}

void *open_service(void *params) {
   int ret;
   get_args *args = (get_args *) params;

   ret = syscall(TAG_GET, args->key, CMD_OPEN, args->perm);

   if (ret == -1) {
      printf("Thread %ld: service %d opening failed\n", syscall(SYS_gettid), args->key);
   } else {
      printf("Thread %ld: service %d opening succeed\n", syscall(SYS_gettid), args->key);
   }

}

void *receive_message(void *params) {
   int ret;
   exchange_args *args = (exchange_args *) params;

   ret = syscall(TAG_RECEIVE, args->tag, args->level, args->buff, args->size);

   if (ret == -1) {
      printf("Thread %ld: tag %d reveiving at level %d failed\n", syscall(SYS_gettid), args->tag, args->level);
   } else {
      printf("Thread %ld: tag %d receiving message '%s' at level %d succeed (%ld bytes received)\n", syscall(SYS_gettid), args->tag, args->buff, args->level, args->size);
   }

}

void *send_message(void *params) {
   int ret;
   exchange_args *args = (exchange_args *) params;

   ret = syscall(TAG_SEND, args->tag, args->level, args->buff, args->size);

   if (ret == -1) {
      printf("Thread %ld: tag %d sending at level %d failed\n", syscall(SYS_gettid), args->tag, args->level);
   } else {
      printf("Thread %ld: tag %d sending message '%s' at level %d succeed (%ld bytes sent)\n", syscall(SYS_gettid), args->tag, args->buff, args->level, args->size);
   }

}

void *awake_threads(void *params) {
   int ret;
   ctl_args *args = (ctl_args *) params;

   ret = syscall(TAG_CTL, args->tag, AWAKE_ALL);

   if (ret == -1) {
      printf("Thread %ld: threads awakening on tag %d failed\n", syscall(SYS_gettid), args->tag);
   } else {
      printf("Thread %ld: threads awakening on tag %d succeed\n", syscall(SYS_gettid), args->tag);
   }

}

void *remove_service(void *params) {
   int ret;
   ctl_args *args = (ctl_args *) params;

   ret = syscall(TAG_CTL, args->tag, REMOVE);

   if (ret == -1) {
      printf("Thread %ld: service with tag %d removal failed\n", syscall(SYS_gettid), args->tag);
   } else {
      printf("Thread %ld: service with tag %d removal  succeed\n", syscall(SYS_gettid), args->tag);
   }

}

/* Services with key 4, 98, 211 created One more service has been created with a key
   choosen by the system */
void services_creation() {
   pthread_t tid0, tid1, tid2, tid3, tid4, tid5;

   get_args args1 = {.key = 98, .perm = ANY};
   pthread_create(&tid0, NULL, create_service, (void *) &args1);

   /* This thread will have a failure: the service with key 98 alreay exists */
   pthread_create(&tid1, NULL, create_service, (void *) &args1);

   get_args args2 = {.key = 211, .perm = ANY};
   pthread_create(&tid2, NULL, create_service, (void *) &args2);

   get_args args3 = {.key = 4, .perm = ONLY_OWNER};
   pthread_create(&tid3, NULL, create_service, (void *) &args3);

   get_args args4 = {.key = 301, .perm = ANY};
   /* This thread will have a failure: the maximum key allowed is 256 */
   pthread_create(&tid4, NULL, create_service, (void *) &args4);

   get_args args5 = {.key = IPC_PRIVATE, .perm = ANY};
   pthread_create(&tid5, NULL, create_service, (void *) &args5);

   pthread_join(tid0, NULL);
   pthread_join(tid1, NULL);
   pthread_join(tid2, NULL);
   pthread_join(tid3, NULL);
   pthread_join(tid4, NULL);
   pthread_join(tid5, NULL);

}

void services_opening() {
   pthread_t tid0, tid1, tid2, tid3;

   get_args args1 = {.key = 98, .perm = ANY};
   pthread_create(&tid0, NULL, open_service, (void *) &args1);

   get_args args2 = {.key = 211, .perm = ANY};
   pthread_create(&tid1, NULL, open_service, (void *) &args2);

   get_args args3 = {.key = 4, .perm = ANY};
   pthread_create(&tid2, NULL, open_service, (void *) &args3);

   get_args args4 = {.key = 57, .perm = ANY};
   /* This thread will have a failure: service with key 57 does not exist */
   pthread_create(&tid3, NULL, open_service, (void *) &args4);

   pthread_join(tid0, NULL);
   pthread_join(tid1, NULL);
   pthread_join(tid2, NULL);
   pthread_join(tid3, NULL);

}

void message_exchange() {
   pthread_t tid0, tid1, tid2, tid3, tid4, tid5;

   /* Buffers used for message receiving */
   char *buff1 = malloc(32);
   char *buff2 = malloc(32);
   char *buff3 = malloc(32);
   char *buff4 = malloc(32);

   /* Now the service identifier is the tag, meaning key-1 */

   exchange_args args1 = {.tag = 97, .level = 6, .buff = buff1, .size = 32};
   pthread_create(&tid0, NULL, receive_message, (void *) &args1);

   exchange_args args2 = {.tag = 117, .level = 12, .buff = buff2, .size = 32};
   /* This thread will have a failure: the tag does not match any existing service */
   pthread_create(&tid1, NULL, receive_message, (void *) &args2);

   /* The next 2 thread wait for a message from the same level of the same service */
   exchange_args args3 = {.tag = 3, .level = 2, .buff = buff3, .size = 32};
   pthread_create(&tid2, NULL, receive_message, (void *) &args3);

   exchange_args args4 = {.tag = 3, .level = 2, .buff = buff4, .size = 32};
   pthread_create(&tid3, NULL, receive_message, (void *) &args4);

   sleep(5);

   exchange_args args5 = {.tag = 3, .level = 2, .buff = "First writer", .size = strlen("First writer")};
   pthread_create(&tid4, NULL, send_message, (void *) &args5);

   /* This writing is discarded: no waiting threads */
   exchange_args args6 = {.tag = 75, .level = 30, .buff = "Second writer", .size = strlen("Second writer")};
   pthread_create(&tid5, NULL, send_message, (void *) &args6);

   sleep(2);

   printf("Removal started\n");
   service_removing();
   printf("Removal done\n");

   printf("Awakening started\n");
   threads_awakening();
   printf("Awakening done\n");

   pthread_join(tid0, NULL);
   pthread_join(tid1, NULL);
   pthread_join(tid2, NULL);
   pthread_join(tid3, NULL);
   pthread_join(tid4, NULL);
   pthread_join(tid5, NULL);
}

void service_removing() {
   pthread_t tid0, tid1;

   ctl_args args1 = {.tag = 210};
   pthread_create(&tid0, NULL, remove_service, (void *) &args1);

   ctl_args args2 = {.tag = 97};
   /* Some threads are waiting for a message. Removal not possible */
   pthread_create(&tid1, NULL, remove_service, (void *) &args2);

   pthread_join(tid0, NULL);
   pthread_join(tid1, NULL);

}

void threads_awakening() {
   pthread_t tid0, tid1;

   ctl_args args1 = {.tag = 97};
   pthread_create(&tid0, NULL, awake_threads, (void *) &args1);

   ctl_args args2 = {.tag = 3};
   /* No waiting threads on this service */
   pthread_create(&tid1, NULL, awake_threads, (void *) &args2);

   pthread_join(tid0, NULL);
   pthread_join(tid1, NULL);
}

void main(int argc, char **argv) {

   /* ------ Tests ------ */
   services_creation();
   printf("Creation done\n");
   services_opening();
   printf("Opening done\n");
   message_exchange();
   printf("Exchanging done\n");
   //service_removing();
   //printf("Removal done\n");
   //threads_awakening();
   //printf("Awakening done\n");
   /***********************/

}
