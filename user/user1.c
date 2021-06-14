#include <unistd.h>
#include <stdlib.h>
#include <syscall.h>
#include <stdio.h>
#include <pthread.h>

#include "../include/constants.h"

typedef struct _exchange_args{
        int tag;
        int level;
        char *buff;
        size_t size;
} exchange_args;

void *receive_msg(void *params) {
   int ret;

   exchange_args *args = (exchange_args *) params;

   ret = syscall(182, args->tag, args->level, args->buff, args->size);
   printf("result: %d\n", ret);
   printf("%s\n", args->buff);

   return NULL;
}

void *send_msg(void *params) {
   int ret;

   exchange_args *args = (exchange_args *) params;

   ret = syscall(174, args->tag, args->level, args->buff, args->size);
   printf("result: %d\n", ret);

   return NULL;
}

int main (int argc, char **argv) {
   int ret;
   char buff[64];
   pthread_t tid1, tid2;

   ret = syscall(134, 4, CMD_CREATE, ANY);
   printf("key: %d\n", ret);

   ret = syscall(134, ret, CMD_OPEN, ANY);
   printf("tag: %d\n", ret);

   exchange_args args1 = {.tag = 3, .level = 2, .buff = buff, .size = 64};
   pthread_create(&tid1, NULL, receive_msg, (void *) &args1);

   sleep(2);

   exchange_args args2 = {.tag = 3, .level = 2, .buff = "Ciao, thread!", .size = sizeof("Ciao, thread!")};
   pthread_create(&tid2, NULL, send_msg, (void *) &args2);

   pthread_join(tid1, NULL);
   pthread_join(tid2, NULL);

   return 0;
}

