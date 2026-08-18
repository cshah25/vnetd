#include "system.h"
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>

int system_drop_privileges(const char *username) {
    struct passwd *pw = getpwnam(username);
    if (!pw) {
        return -1;
    }

    if (setgroups(0, NULL) < 0) return -1;
    if (setgid(pw->pw_gid) < 0) return -1;
    if (setuid(pw->pw_uid) < 0) return -1;

    return 0;
}

int system_daemonize(void) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) exit(0);

    if (setsid() < 0) return -1;

    pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) exit(0);

    return 0;
}
