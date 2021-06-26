#ifndef CONSTANTS_H
#define CONSTANTS_H

#define MODNAME "TBDE"

/* Permission level for TAG service operations */
#define ONLY_OWNER 0
#define ANY 1

/* Number of entries in the TAG services table (TST) */
#define TAG_SERVICES_NUM 256

/* Available levels for each tag service */
#define LEVELS 32

/* Commands allowed in tag_get syscall */
#define CMD_CREATE 0
#define CMD_OPEN 1

/* Command allowed in tag_ctl syscall */
#define AWAKE_ALL 0
#define REMOVE 1

/* Number of entries in syscall table */
#define NR_ENTRIES 256

/* Maximum size of a message sent/received by a thread */
#define MAX_MSG_SIZE 4096

/* Useful in vtpmo */
#define NO_MAP -1

/* Number of syscalls needed in the subsystem */
#define NR_NEW_SYSCALLS 4

#define EOPEN -2

#define DEVNAME "TBDE"

#define MAJOR_NM 237

#endif
