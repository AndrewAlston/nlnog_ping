//
// Created by aalston on 4/20/26.
//
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include "nlring.h"

struct ring_control rc = {0};

void sig_execute(int signal, siginfo_t *siginfo, void *control) {
    struct ring_control *controller = siginfo->si_value.sival_ptr;
    controller->stop = true;
}

void sig_handler(int signal, siginfo_t *siginfo, void *control) {
    printf("Got control-c, quitting\n");
    struct sigaction action;
    action.sa_sigaction = sig_execute;
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    union sigval val;
    val.sival_ptr = &rc;
    sigqueue(getpid(), SIGINT, val);
}
int main() {
    struct sigaction action;
    action.sa_sigaction = sig_handler;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);

    struct ring_conf *conf = read_nlnog_config("config/sysconfig.json");
    if (!conf) {
        printf("Failed reading configuration\n");
        return EXIT_FAILURE;
    }
    if (conf->bind_core) {
        rc.bind_core = conf->bind_core;
        rc.core = conf->core;
    }
    rc.config = conf;
    printf("Creating run thread\n");
    pthread_create(&rc.control_thread, NULL, nlnog_run, &rc);
    pthread_join(rc.control_thread, NULL);
    ring_free_conf(&conf);
    return EXIT_SUCCESS;
}