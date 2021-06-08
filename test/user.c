#include <unistd.h>
#include <stdio.h>
#include <sys/ipc.h>

int main(int argc, char **argv) {
   printf("%d\n", IPC_PRIVATE);
}
