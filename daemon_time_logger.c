/*
* Hi Dr. Lamb!
*
 * Compile with:
 * gcc -o daemon_time_logger daemon_time_logger.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <syslog.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>

// global flags that are set by the signal handler
volatile sig_atomic_t terminate = 0;
volatile sig_atomic_t reload = 0;

void handle_signal(int sig) {
    if (sig == SIGTERM) {
        syslog(LOG_INFO, "Received SIGTERM, now exiting.");
        closelog();
        exit(0);
    } else if (sig == SIGHUP) {
        syslog(LOG_INFO, "Received SIGHUP (now reload signal).");
    }
}

int main() {
    pid_t pid;

    // First fork for the daemon
    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    //session leader
    if (setsid() < 0) {
        exit(EXIT_FAILURE);
    }

    //setup signal handlers using the sigaction
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    //second fork
    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    // Set umask
    umask(0);

    // chdir to root
    if (chdir("/") < 0) {
        exit(EXIT_FAILURE);
    }

    // close stdin, stdout, stderr and redirect to /dev/null
    int fd = open("/dev/null", O_RDWR);
    if (fd != -1) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > 2) close(fd);
    } else {
        exit(EXIT_FAILURE);  // Fail if /dev/null is not able to be opened
    }

    // use syslog for logging 
    openlog("daemon_time_logger", LOG_PID | LOG_CONS, LOG_DAEMON);
    syslog(LOG_INFO, "Daemon started...");

    // main loop
    while (!terminate) {
        if (reload) {
            syslog(LOG_INFO, "Received SIGHUP... reload.");
            reload = 0;
        }

        // get the time! (Finally!)
        time_t now = time(NULL);
        struct tm tm_info;
        char timestr[64];

        if (localtime_r(&now, &tm_info) != NULL) {
            if (strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", &tm_info) > 0) {
                syslog(LOG_INFO, "Current time: %s", timestr);
            } else {
                syslog(LOG_ERR, "strftime() failed to format.");
            }
        } else {
            syslog(LOG_ERR, "localtime_r() failed: %s", strerror(errno));
        }

        sleep(1);
    }

    syslog(LOG_INFO, "Daemon received SIGTERM... shutting down.");
    closelog();
    return 0;
}