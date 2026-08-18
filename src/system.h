#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdbool.h>

// Drop privileges to a non-root user
int system_drop_privileges(const char *username);

// Daemonize the process
int system_daemonize(void);

#endif // SYSTEM_H
