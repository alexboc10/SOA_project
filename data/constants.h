/* Permission level for TAG service operations */
#define LIMITED 0
#define ANY 1

/* Number of entries in the TAG services table (TST) */
#define TAG_SERVICE_NUM 256

/* Number of levels for every TAG service */
#define LEVELS_NUM 32

/* Maximum message length allowed */
#define MAX_MSG_SIZE 4096

/* Commands allowed in tag_get syscall */
#define CMD_CREATE 0
#define CMD_OPEN 1

/* Number of entries in syscall table */
#define NR_ENTRIES 256

#define NO_MAP -1

/* Number of syscalls needed in the subsystem */
#define NR_NEW_SYSCALLS 4
