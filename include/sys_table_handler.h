#ifndef SYS_TABLE_HANDLER_H
#define SYS_TABLE_HANDLER_H

/* ---------- Syscalls management ---------- */
int install_syscalls(void);

int uninstall_syscalls(void);

void get_syscalls_addresses(void);

#endif
