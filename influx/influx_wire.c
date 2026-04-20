//
// Created by andrew on 1/28/26.
//

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <netinet/in.h>
#include <unistd.h>
#include <math.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include "influx_wire.h"
#include "generic.h"

struct influx *influx_new(const char *ip, const __u16 port, const char *username, const char *password, const char *database)
{
    struct influx *db = NULL;
    if((db = calloc(1, sizeof(struct influx))) == NULL) {
        DEBUG_PRINT(printf("Failed allocating for database structure\n"));
        return NULL;
    }
    if(!inet_pton(AF_INET, ip, &db->host_ip)) {
        DEBUG_PRINT(printf("Failed to get address from supplied IP %s\n", ip));
        goto cleanup_error;
    }
    if(htons(port) != 0) {
        db->host_port = port;
    } else {
        DEBUG_PRINT(printf("Invalid port specified\n"));
        goto cleanup_error;
    }
    snprintf(db->hostname, 256, "%s", ip);
    if(username)
        snprintf(db->username, 256, "%s", username);
    if(password)
        snprintf(db->password, 256, "%s", password);
    if(!database) {
        DEBUG_PRINT(printf("Database parameter cannot be NULL\n"));
        goto cleanup_error;
    }
    snprintf(db->database, 256, "%s", database);
    return db;

    cleanup_error:
    FREE_NULL(db);
    return db;
}

struct influx_measurement *influx_add_measurement(struct influx *db) {
    if (db->measurement == NULL && db->n_measurement == 0) {
        db->measurement = calloc(1, sizeof(struct influx_measurement));
        if (!db->measurement)
            return NULL;
        db->n_measurement++;
        return db->measurement;
    }
    struct influx_measurement *new = reallocarray(db->measurement, db->n_measurement+1,
        sizeof(struct influx_measurement));
    if (!new)
        return NULL;
    db->measurement = new;
    memset(&db->measurement[db->n_measurement], 0, sizeof(struct influx_measurement));
    db->n_measurement++;
    return &db->measurement[db->n_measurement-1];
};

int influx_set_tags(struct influx_measurement *measurement, char *tags)
{
    if(strlen(tags) == 0 || strlen(tags) > MAX_TAG_LIMIT) {
        DEBUG_PRINT(printf("Tag lag exceeded maximum limit\n"));
        return -1;
    }
    snprintf(measurement->influx_tags, MAX_TAG_LIMIT, "%s", tags);
    memcpy(measurement->influx_tags, tags, strlen(tags));
    return 0;
}

void influx_start_measurement(struct influx_measurement *measurement, char *section)
{
    memset(measurement->buffer, 0, BUFFER_SIZE);
    snprintf(measurement->buffer, BUFFER_SIZE, "%s,%s ", section, measurement->influx_tags);
}

void influx_end_measurement(struct influx_measurement *measurement, __u64 ts)
{
    if(measurement->buffer[strlen(measurement->buffer)-1] == ',') {
        measurement->buffer[strlen(measurement->buffer)-1] = 0;
    }
    snprintf(&measurement->buffer[strlen(measurement->buffer)], INFLUX_REMAINING, " %llu", ts);
    snprintf(&measurement->buffer[strlen(measurement->buffer)], INFLUX_REMAINING, "    \n");
}

void influx_add_long(struct influx_measurement *measurement, char *name, const long long value)
{
    snprintf(&measurement->buffer[strlen(measurement->buffer)], INFLUX_REMAINING, "%s=%lldi,", name, value);
}

void influx_add_double(struct influx_measurement *measurement, char *name, double value)
{
    if(isnan(value) || isinf(value))
        value = -40;
    snprintf(&measurement->buffer[strlen(measurement->buffer)], INFLUX_REMAINING, "%s=%.3f,", name, value);
}

void influx_add_string(struct influx_measurement *measurement, const char *name, char *value)
{
    for(int i = 0; i < strlen(value); i++)
        if(value[i] == '\n' || iscntrl(value[i]))
            value[i] = ' ';
    snprintf(&measurement->buffer[strlen(measurement->buffer)], INFLUX_REMAINING, "%s=\"%s\",", name, value);
}

void influx_add_boolean(struct influx_measurement *measurement, char *name, __u64 value)
{
    if (value)
        snprintf(&measurement->buffer[strlen(measurement->buffer)], INFLUX_REMAINING, "%s=true,", name);
    else
        snprintf(&measurement->buffer[strlen(measurement->buffer)], INFLUX_REMAINING, "%s=false,", name);
}

int influx_connect_socket(struct influx *db) {
    struct timespec start, end;
    db->serv_addr.sin_family = AF_INET;
    db->serv_addr.sin_addr.s_addr = db->host_ip;
    db->serv_addr.sin_port = htons(db->host_port);
    db->socket = socket(AF_INET, SOCK_STREAM, 0);
    if (db->socket < 0) {
        DEBUG_PRINT(printf("Failed to create influx database socket\n"));
        return -1;
    }
    fcntl(db->socket, F_SETFL, O_NONBLOCK);
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (;;) {
        if (connect(db->socket, (struct sockaddr *)&db->serv_addr, sizeof(db->serv_addr)) == -1) {
            if (errno != EINPROGRESS) {
                DEBUG_PRINT(printf("Failed to connect to influx server [%s:%d] [%s]",
                    db->hostname, db->host_port, strerror(errno)));
                close(db->socket);
                return -1;
            }
            usleep(200000);
            clock_gettime(CLOCK_MONOTONIC, &end);
            if (end.tv_sec > start.tv_sec+5) {
                DEBUG_PRINT(printf("Timed out connecting to socket\n"));
                return -1;
            }
        }
        return 0;
    }
}

void inline __attribute__((__always_inline__)) influx_zero_descriptors(fd_set *fds_read,
    fd_set *fds_write, fd_set *fds_except) {
    FD_ZERO(fds_read);
    FD_ZERO(fds_write);
    FD_ZERO(fds_except);
}

int influx_wait_socket(struct influx *db, fd_set *read, fd_set *write, fd_set *expect, const int timeout) {
    struct timeval timer = { .tv_sec = timeout, .tv_usec = 0};
    if (select(db->socket+1, read, write, expect, &timer) == 1) {
        int so_error;
        socklen_t len = sizeof(so_error);
        getsockopt(db->socket, SOL_SOCKET, SO_ERROR, &so_error, &len);
        if (so_error != 0) {
            DEBUG_PRINT(printf("[%s] Socket timeout\n", db->hostname));
            return -1;
        }
    }
    return 0;
}

int influx_send_buffer(struct influx *db) {
    if (strlen(db->buffer) < 0 || strlen(db->buffer) > BUFFER_SIZE)
        return -1;
    const size_t total = strlen(db->buffer);
    size_t sent = 0;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    fd_set fds_read, fds_write, fds_except;
    while (sent < total) {
        influx_zero_descriptors(&fds_read, &fds_write, &fds_except);
        FD_SET(db->socket, &fds_write);
        if (influx_wait_socket(db, NULL, &fds_write, NULL, 2))
            return -1;
        const ssize_t ret = write(db->socket, &db->buffer[sent], total-sent);
        if (ret < 0) {
            return -1;
        }
        sent+=ret;
        clock_gettime(CLOCK_MONOTONIC, &end);
        if (end.tv_sec > start.tv_sec +5)
            return -1;
    }
    return 0;
}

int influx_send_query(struct influx *db, const int action) {
    fflush(stdout);
    fd_set fds_read, fds_write, fds_except;
    influx_zero_descriptors(&fds_read, &fds_write, &fds_except);
    char *buffer = calloc(BUFFER_SIZE*3, 1);
    if (!buffer)
        printf("Failed allocating for temprorary buffer\n");
    if (action == INFLUX_WRITE) {
        snprintf(buffer, BUFFER_SIZE,
            "POST /write?db=%s&u=%s&p=%s&epoch=ms HTTP/1.1\r\nHOST: %s:%u\r\nContent-Length: %ld\r\n\r\n",
            db->database, db->username, db->password, db->hostname, db->host_port, strlen(db->buffer));
    } else if (action == INFLUX_QUERY) {
        snprintf(buffer, BUFFER_SIZE,
            "POST /query?db=%s&u=%s&p=%s&epoch=ms&q=%s HTTP1/1\r\nHOST: %s:%u\r\nContent-Length %ld\r\n\r\n",
            db->database, db->username, db->password, db->encoded, db->hostname, db->host_port, strlen(db->encoded));
    } else {
        FREE_NULL(buffer);
        return -1;
    }
    FD_SET(db->socket, &fds_write);
    if (influx_wait_socket(db, &fds_read, &fds_write, &fds_except, 2)) {
        DEBUG_PRINT(printf("Timed out sending to socket\n"));
        FREE_NULL(buffer);
        return -1;
    }
    if (write(db->socket, buffer, strlen(buffer)) != strlen(buffer)) {
        DEBUG_PRINT(printf("Failed posting to socket with errno %d [%s]\n", errno, strerror(errno)));
        FREE_NULL(buffer);
        return -1;
    }
    if (action == INFLUX_WRITE) {
        if (influx_send_buffer(db) != 0) {
            DEBUG_PRINT(printf("Failed writing to database\n"));
            FREE_NULL(buffer);
            return -1;
        }
    }
    FREE_NULL(buffer);
    return 0;
}

int influx_push(struct influx *db)
{
    int code = 0;
    fd_set fds_read, fds_write, fds_except;
    if (influx_connect_socket(db))
        return -1;
    influx_zero_descriptors(&fds_read, &fds_write, &fds_except);
    FD_SET(db->socket, &fds_write);
    if (influx_wait_socket(db, &fds_read, &fds_write, &fds_except, 1)) {
        FREE_NULL(db->measurement);
        db->n_measurement = 0;
        INFLUX_DB_FINISH(-1);
    }
    for (int i = 0; i < db->n_measurement; i++) {
        memset(db->buffer, 0, BUFFER_SIZE);
        if (db->measurement && &db->measurement[i]) {
            memcpy(db->buffer, db->measurement[i].buffer, strlen(db->measurement[i].buffer));
            if (influx_send_query(db, INFLUX_WRITE))
                INFLUX_DB_FINISH(-1);
            influx_zero_descriptors(&fds_read, &fds_write, &fds_except);
            FD_SET(db->socket, &fds_read);
            if (influx_wait_socket(db, &fds_read, NULL, NULL, 2)) {
                DEBUG_PRINT(printf("Timeout waiting for database return\n"));
                FREE_NULL(db->measurement);
                db->n_measurement = 0;
                INFLUX_DB_FINISH(-1);
            }
            size_t ret;
            if((ret = read(db->socket, db->result, RESULT_SIZE)) > 0) {
                sscanf(db->result, "HTTP/1.1 %d", &code);
                if(code != 204) {
                    printf("Failed post, got error return code %d\n", code);
                    printf("Attempted to post %s\n", db->buffer);
                    printf("Got return %s\n", db->result);
                    FREE_NULL(db->measurement);
                    db->n_measurement = 0;
                    INFLUX_DB_FINISH(-1);
                }
            } else {
                DEBUG_PRINT(printf("Error reading from socket [%ld] [%s]\n", ret, strerror(errno)));
                FREE_NULL(db->measurement);
                db->n_measurement = 0;
                INFLUX_DB_FINISH(-1);
            }
        }
    }
    printf("Completed push of %u measurements\n", db->n_measurement);
    FREE_NULL(db->measurement);
    db->n_measurement = 0;
    INFLUX_DB_FINISH(0);
}

struct url_encoder *url_init_encoder() {
    struct url_encoder *result = calloc(1, sizeof(struct url_encoder));
    if (!result) {
        return NULL;
    }
    for (int i = 0; i < 256; i++) {
        result->rfc3985[i] = isalnum( i) || i == '~' || i == '-' || i == '.' || i == '_' ? i : 0;
        result->html5[i] = isalnum( i) || i == '*' || i == '-' || i == '.' || i == '_' ? i : (i == ' ') ? '+' : 0;
    }
    return result;
}

int url_encode(const char *table, const unsigned char *s, char *enc, const char *end){
    for (; *s; s++){
        if (table[*s]) *enc = table[*s];
        else sprintf( enc, "%%%02X", *s);
        while (*++enc) {};
        if (enc > end) {
            return -1;
        }
    }
    return 0;
}

int influx_run_query(struct influx *db, const unsigned char *query) {
    struct url_encoder *encoder = url_init_encoder();
    if (!encoder) {
        DEBUG_PRINT(printf("Failed allocating for URL encoder\n"));
        return -1;
    }
    memset(db->encoded, 0, BUFFER_SIZE*3);
    url_encode(encoder->rfc3985, query, db->encoded, db->encoded+(BUFFER_SIZE*3));
    FREE_NULL(encoder);
    if (influx_connect_socket(db))
        return -1;
    if (influx_connect_socket(db))
        return -1;
    fd_set fds_read, fds_write, fds_except;
    influx_zero_descriptors(&fds_read, &fds_write, &fds_except);
    FD_SET(db->socket, &fds_write);
    if (influx_wait_socket(db, &fds_read, &fds_write, &fds_except, 1))
        INFLUX_DB_FINISH(-1);
    if (influx_send_query(db, INFLUX_QUERY))
        INFLUX_DB_FINISH(-1);
    influx_zero_descriptors(&fds_read, &fds_write, &fds_except);
    FD_SET(db->socket, &fds_read);
    if (influx_wait_socket(db, &fds_read, &fds_write, &fds_except, 2)) {
        DEBUG_PRINT(printf("Timed out waiting for database response\n"));
        INFLUX_DB_FINISH(-1);
    }
    int code;
    if (read(db->socket, db->result, RESULT_SIZE) > 0) {
        sscanf(db->result, "HTTP/1.1 %d", &code);
        if (code != 200) {
            printf("Failed query post, got error return code %d\n", code);
            INFLUX_DB_FINISH(-1);
        }
    }
    ssize_t ret;
    do {
        influx_zero_descriptors(&fds_read, &fds_write, &fds_except);
        FD_SET(db->socket, &fds_read);
        influx_wait_socket(db, &fds_read, &fds_write, &fds_except, 1);
        ret = read(db->socket, &db->result[strlen(db->result)], BUFFER_SIZE-strlen(db->result));
    } while (ret > 0);
    INFLUX_DB_FINISH(0);
}

