#ifndef DEV_HANDLER_H
#define DEV_HANDLER_H

int register_dev(void);

void unregister_dev(void);

char *status_builder(void);

void free_status_buffer(void);

#endif
