//
// Created by andrew on 3/27/26.
//

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <libssh2.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include "nlring.h"
#include "ring_ssh.h"

int ring_host_connect(struct thread *t) {
    fd_set fds;
    struct timeval timeout = { .tv_sec = 5, .tv_usec = 0 };
    memset(&t->serv_socket, 0, sizeof(struct sockaddr_in));
    if (inet_pton(AF_INET, t->host.ipv4_addr, &t->serv_socket.sin_addr) != 1) {
        printf("Failed to resolve %s to a valid IPv4 address\n", t->host.ipv4_addr);
        return -1;
    }
    t->socket = socket(AF_INET, SOCK_STREAM, 0);
    if (t->socket == LIBSSH2_INVALID_SOCKET) {
        t->error = true;
        snprintf(t->error_msg, 2048, "Failed to create socket\n");
        return -1;
    }
    t->serv_socket.sin_family = AF_INET;
    t->serv_socket.sin_port = htons(22);
    fcntl(t->socket, F_SETFL, O_NONBLOCK);
    const int ret = connect(t->socket, (struct sockaddr*)&t->serv_socket, sizeof(struct sockaddr_in));
    if (ret != 0 && errno != EINPROGRESS) {
        printf("Failed connection... [ret: %d] [errno: %d] [%s]", ret, errno, strerror(errno));
        close(t->socket);
        return -1;
    }
    FD_ZERO(&fds);
    FD_SET(t->socket, &fds);
    if (select(t->socket+1, NULL, &fds, NULL, &timeout) == 1) {
        int so_error;
        socklen_t len_err = sizeof(so_error);
        getsockopt(t->socket, SOL_SOCKET, SO_ERROR, &so_error, &len_err);
        if (so_error != 0) {
            printf("Socket error: %d [%s]\n", so_error, strerror(so_error));
            return -1;
        }
        return 0;
    }
    return -1;
}

void ring_ssh_cleanup(struct thread *t) {
    if (t->session) {
        libssh2_session_free(t->session);
        t->session = NULL;
    }
    if (t->socket != LIBSSH2_INVALID_SOCKET) {
        shutdown(t->socket, 2);
        close(t->socket);
    }
}

int ssh_wait_socket(const libssh2_socket_t socket_fd, LIBSSH2_SESSION *session) {
    struct timeval timeout;
    fd_set fd;
    fd_set *write_fd = NULL;
    fd_set *read_fd = NULL;
    timeout.tv_sec = 0;
    timeout.tv_usec = 300000;
    const int dir = libssh2_session_block_directions(session);
    if (dir & LIBSSH2_SESSION_BLOCK_INBOUND)
        read_fd = &fd;
    if (dir & LIBSSH2_SESSION_BLOCK_OUTBOUND)
        write_fd = &fd;
    const int rc = select((int)(socket_fd +1), read_fd, write_fd, NULL, &timeout);
    return rc;
}

int ring_init_ssh_session(struct thread *t) {
    int retval = 0;
    t->session = libssh2_session_init();
    if (!t->session)
        return -1;
    libssh2_session_set_blocking(t->session, 0);
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    while ((retval = libssh2_session_handshake(t->session, t->socket)) == LIBSSH2_ERROR_EAGAIN) {
        usleep(200000);
        clock_gettime(CLOCK_MONOTONIC, &end_time);
        if (end_time.tv_sec > start_time.tv_sec+10)
            return -1;
    }
    if (retval) {
        ring_ssh_cleanup(t);
        return -1;
    }
    t->known_hosts = libssh2_knownhost_init(t->session);
    if (!t->known_hosts) {
        ring_ssh_cleanup(t);
        return -1;
    }
    libssh2_knownhost_readfile(t->known_hosts, KNOWN_HOSTS_FILE, LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    size_t len;
    int type;
    const char *fingerprint = libssh2_session_hostkey(t->session, &len, &type);
    if (!fingerprint) {
        ring_ssh_cleanup(t);
        return -1;
    }
    struct libssh2_knownhost *host;
    const int check = libssh2_knownhost_checkp(t->known_hosts, t->host.ipv4_addr, 22, fingerprint,
        len, KNOWN_HOST_MASK, &host);
    if (check == LIBSSH2_KNOWNHOST_CHECK_NOTFOUND) {
        libssh2_knownhost_add(t->known_hosts, t->host.ipv4_addr, NULL, fingerprint, len,
            KNOWN_HOST_MASK, NULL);
        libssh2_knownhost_writefile(t->known_hosts, KNOWN_HOSTS_FILE, LIBSSH2_KNOWNHOST_FILE_OPENSSH);
    }
    libssh2_knownhost_free(t->known_hosts);
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    while ((retval = libssh2_userauth_publickey_fromfile(t->session, t->username,
        t->pubkey, t->private_key, NULL)) == LIBSSH2_ERROR_EAGAIN) {
        clock_gettime(CLOCK_MONOTONIC, &end_time);
        if (end_time.tv_sec > start_time.tv_sec+5) {
            ring_ssh_cleanup(t);
            return -1;
        }
        usleep(200000);
    }
    do {
        t->channel = libssh2_channel_open_session(t->session);
        if (t->channel || libssh2_session_last_error(t->session, NULL, NULL, 0) != LIBSSH2_ERROR_EAGAIN)
            break;
        ssh_wait_socket(t->socket, t->session);
    } while (1);
    if (!t->channel)
        ring_ssh_cleanup(t);
    return 0;
}

int ring_execute_command(struct thread *t, const char *command) {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    int retval;
    while ((retval = libssh2_channel_exec(t->channel, command)) == LIBSSH2_ERROR_EAGAIN) {
        ssh_wait_socket(t->socket, t->session);
        clock_gettime(CLOCK_MONOTONIC, &end);
        if (end.tv_sec > start.tv_sec+10) {
            return -1;
        }
    }
    memset(t->buffer, 0, BUFFER_SIZE);
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (;;) {
        ssize_t read_data = 0;
        do {
            char buf[16384] = {0};
            read_data = libssh2_channel_read(t->channel, buf, 16384);
            if (read_data > 0)
                snprintf(&t->buffer[strlen(t->buffer)], BUFFER_SIZE-strlen(t->buffer), "%s", buf);
        } while (read_data > 0);
        if (read_data == LIBSSH2_ERROR_EAGAIN) {
            ssh_wait_socket(t->socket, t->session);
            clock_gettime(CLOCK_MONOTONIC, &end);
            if (end.tv_sec > start.tv_sec+15) {
                ring_ssh_cleanup(t);
                return -1;
            }
        } else
            break;
    }
    clock_gettime(CLOCK_MONOTONIC, &start);
    while ((retval = libssh2_channel_close(t->channel)) == LIBSSH2_ERROR_EAGAIN) {
        ssh_wait_socket(t->socket, t->session);
        clock_gettime(CLOCK_MONOTONIC, &end);
        if (end.tv_sec > start.tv_sec +10) {
            ring_ssh_cleanup(t);
            return -1;
        }
    }
    char *exit_signal = NULL;
    if (retval == 0) {
        libssh2_channel_get_exit_status(t->channel);
        libssh2_channel_get_exit_signal(t->channel, &exit_signal,
            NULL, NULL, NULL, NULL, NULL);
    }
    free(exit_signal);
    libssh2_channel_free(t->channel);
    t->channel = NULL;
    ring_ssh_cleanup(t);
    return 0;
}
