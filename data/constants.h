/* Permission level for TAG service operations */
#define USER_PERM 0
#define ANY_PERM 1

/* Number of entries in the TAG services table (TST) */
#define TAG_SERVICE_NUM 256

/* number of levels for every TAG service */
#define LEVELS_NUM 32

/* Maximum message length allowed */
#define MAX_MSG_SIZE 4096

/* Commands allowed in tag_get syscall */
#define CMD_CREATE 0
#define CMD_OPEN 1

/* Number of entries in syscall table */
#define NR_ENTRIES 256

#define NO_MAP -1

#define NR_NEW_SYSCALLS 4
