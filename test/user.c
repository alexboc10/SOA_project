#include <unistd.h>

int main(int argc, char **argv) {
   syscall(134, 1, 1, 1);
   syscall(174, 1, 1, NULL, 1);
   syscall(182, 1, 1, NULL, 1);
   syscall(183, 1, 1);
}
