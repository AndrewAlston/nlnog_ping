//
// Created by andrew on 3/26/26.
//

#include <json-c/json.h>
#include <curl/curl.h>
#include <fcntl.h>
#include <unistd.h>
#include "nlring.h"
#include "generic.h"
#include "influx_wire.h"
#include "ring_ssh.h"
#include "json_helper.h"

#ifndef NANOSECONDS_PER_SECOND
#define NANOSECONDS_PER_SECOND 1000000000L
#endif

size_t get_curl_data(const void *buffer, const size_t size, const size_t n_members, void *url_data) {
    struct url_data *d = url_data;
    char *ptr = NULL;
    const size_t orig_len = d->length;
    if (!d->data) {
        d->length = size*n_members;
        d->data = calloc(1, n_members);
        if (!d->data) {
            return -1;
        }
        ptr = d->data;
    }  else {
        d->length += (size*n_members);
        char *data = realloc(d->data, d->length);
        if (data) {
            d->data = data;
            memset(&d->data[orig_len], 0, d->length-orig_len);
        } else {
            free(d->data);
            return -1;
        }
        ptr = &d->data[strlen(d->data)];
    }
    memcpy(ptr, buffer, size*n_members);
    return size*n_members;
}

struct url_data *retrieve_node_data() {
    struct url_data *data = calloc(1, sizeof(struct url_data));
    if (!data)
        return NULL;
    int retry_count = 0;
    while (retry_count < 5) {
        void *handle = curl_easy_init();
        curl_easy_setopt(handle, CURLOPT_URL, "https://api.ring.nlnog.net/1.0/nodes/active/protocol/ipv4");
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, get_curl_data);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, data);
        const int err = curl_easy_perform(handle);
        if (!err) {
            curl_easy_cleanup(handle);
            return data;
        }
        curl_easy_cleanup(handle);
        handle = NULL;
        retry_count++;
    }
    return NULL;
}

int populate_node_check(const int node_count, json_object *obj, struct hosts *host) {
    char paths[5][256] = {0};
    struct json_data *data = calloc(1, sizeof(struct json_data));
    if (!data)
        return -1;
    snprintf(paths[0], 256, "/results/nodes/%d/active", node_count);
    snprintf(paths[1], 256, "/results/nodes/%d/alive_ipv4", node_count);
    snprintf(paths[2], 256, "/results/nodes/%d/countrycode", node_count);
    snprintf(paths[3], 256, "/results/nodes/%d/ipv4", node_count);
    snprintf(paths[4], 256, "/results/nodes/%d/hostname", node_count);
    int status = 0;
    if ((status = json_pointer_get(obj, paths[0], &data->active)) < 0)
        goto complete_check;
    if ((status = json_pointer_get(obj, paths[1], &data->alive_ipv4)) < 0)
        goto complete_check;
    if ((status = json_pointer_get(obj, paths[2], &data->countrycode)) < 0)
        goto complete_check;
    if ((status = json_pointer_get(obj, paths[3], &data->ipv4)) < 0)
        goto complete_check;
    if ((status = json_pointer_get(obj, paths[4], &data->hostname)) < 0)
        goto complete_check;
    if (!json_object_get_int(data->active) || !json_object_get_int(data->alive_ipv4)) {
        status = -1;
        goto complete_check;
    }
    snprintf(host->hostname, 1024, "%s", json_object_get_string(data->hostname));
    snprintf(host->ipv4_addr, INET_ADDRSTRLEN, "%s", json_object_get_string(data->ipv4));
    snprintf(host->country, 16, "%s", json_object_get_string(data->countrycode));
    host->active = true;
    complete_check:
    FREE_NULL(data);
    return status;
}

struct t_container *parse_node_data(const struct url_data *data) {
    struct hosts *hosts = calloc(MAX_NODES, sizeof(struct hosts));
    if (!hosts)
        return NULL;
    json_object *obj = json_tokener_parse(data->data);
    if (!obj)
        printf("Failed parsing json data\n");
    int active_nodes = 0;
    for (int i = 0; i < MAX_NODES; i++) {
        populate_node_check(i, obj, &hosts[i]);
        if (hosts[i].active)
            active_nodes++;
    }
    json_object_put(obj);
    struct t_container *tc = calloc(1, sizeof(struct t_container));
    if (active_nodes > 0) {
        if (!tc) {
            FREE_NULL(hosts);
            return NULL;
        }
        tc->threads = calloc(active_nodes, sizeof(struct thread));
        tc->num_threads = active_nodes;
        if (!tc->threads) {
            FREE_NULL(hosts);
            FREE_NULL(tc);
            return NULL;
        }
        int current_node = 0;
        for (int i = 0; i < MAX_NODES; i++) {
            if (hosts[i].active) {
                struct hosts *current = &tc->threads[current_node++].host;
                memcpy(current->hostname, hosts[i].hostname, 1024);
                memcpy(current->country, hosts[i].country, 16);
                memcpy(current->ipv4_addr, hosts[i].ipv4_addr, INET_ADDRSTRLEN);
            }
        }
        FREE_NULL(hosts);
        return tc;
    }
    FREE_NULL(hosts);
    FREE_NULL(tc->threads);
    FREE_NULL(tc);
    return NULL;
}

struct latency_measurements *allocate_latency_measurement(struct thread *t) {
    struct latency_measurements *measure = calloc(1, sizeof(struct latency_measurements));
    if (!measure)
        return NULL;
    snprintf(measure->min_str, 512, "min");
    snprintf(measure->avg_str, 512, "avg");
    snprintf(measure->max_str, 512, "max");
    snprintf(measure->dev_str, 512, "deviation");
    snprintf(measure->country_string, 16, "counter");
    snprintf(measure->hostname, 1024, "%s", t->host.hostname);
    snprintf(measure->country, 16, "%s", t->host.country);
    snprintf(measure->address, 1024, "%s", t->host.ipv4_addr);
    return measure;
}

int process_poll_data(struct thread *t) {
    char *ptr = NULL, *end = NULL;
    const char *data = NULL;
    struct latency_measurements *m = &t->measurement;
    if ((data = strstr(t->buffer, "rtt")) == NULL)
        return -1;
    if ((ptr = strstr(data, "= ")) == NULL)
        return -1;
    if (strlen(ptr) > 3)
        ptr+=2;
    else
        return -1;
    ptr[strlen(ptr)-1] = '\0';
    GET_RING_DOUBLE(m->min, ptr, end);
    GET_RING_DOUBLE(m->avg, ptr, end);
    GET_RING_DOUBLE(m->max, ptr, end);
    m->deviation = strtod(ptr, NULL);
    return 0;
}

struct t_container *ring_poll_thread_init(const struct ring_conf *config) {
    struct url_data *data = retrieve_node_data();
    if (!data)
        return NULL;
    if (!data->data) {
        FREE_NULL(data);
        return NULL;
    }
    struct t_container *ret = parse_node_data(data);
    if (!ret) {
        FREE_NULL(data->data);
        FREE_NULL(data);
        return NULL;
    }
    for (int i = 0; i < ret->num_threads; i++) {
        snprintf(ret->threads[i].command, 2048, "ping -c %d %s", config->num_pings, config->target);
        snprintf(ret->threads[i].username, 256, "%s", config->username);
        snprintf(ret->threads[i].pubkey, 1024, "%s", config->public_key);
        snprintf(ret->threads[i].private_key, 1024, "%s", config->private_key);
    }
    FREE_NULL(data->data);
    FREE_NULL(data);
    return ret;
}

void *nlnog_run(void *ring) {
    struct ring_control *control = ring;
    struct ring_conf *conf = control->config;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    clock_gettime(CLOCK_MONOTONIC, &end);
    end.tv_sec += control->config->poll_interval;
    struct t_container *tc = NULL;
    struct t_container *next = NULL;
    struct influx *db = influx_new(conf->db_host, conf->db_port, NULL, NULL, conf->database);
    if (!db) {
        goto thread_exit;
    }
    while (!control->stop) {
        if (!tc) {
            tc = ring_poll_thread_init(control->config);
        } else {
            next = ring_poll_thread_init(control->config);
            if (next) {
                FREE_NULL(tc->measure);
                FREE_NULL(tc->threads);
                FREE_NULL(tc);
                tc = next;
                next = NULL;
            }
        }
        struct timespec start_run;
        clock_gettime(CLOCK_REALTIME, &start_run);
        __u64 epoch_ts = (__u64)(start_run.tv_sec*NANOSECONDS_PER_SECOND)+start.tv_nsec;
        for (int i = 0; i < tc->num_threads; i++) {
            pthread_create(&tc->threads[i].thread, NULL, nlnog_thread, &tc->threads[i]);
        }
        for (int i = 0; i < tc->num_threads; i++) {
            pthread_join(tc->threads[i].thread, NULL);
        }
        for (int i = 0; i < tc->num_threads; i++) {
            if (tc->threads[i].error)
                continue;
            process_poll_data(&tc->threads[i]);
            struct latency_measurements *m = &tc->threads[i].measurement;
            struct influx_measurement *measurement = influx_add_measurement(db);
            if (!measurement) {
                printf("Failed allocating for influx measurement\n");
                continue;
            }
            char tmp_buf[4096];
            snprintf(tmp_buf, 4096, "country=%s,hostname=%s,address=%s",
                tc->threads[i].host.country,
                tc->threads[i].host.hostname,
                tc->threads[i].host.ipv4_addr);
            influx_set_tags(measurement, tmp_buf);
            influx_start_measurement(measurement, "latency");
            influx_add_double(measurement, "min", m->min);
            influx_add_double(measurement, "avg", m->avg);
            influx_add_double(measurement, "max", m->max);
            influx_add_double(measurement, "deviation", m->deviation);
            influx_end_measurement(measurement, epoch_ts);
        }
        printf("Starting influx push...\n");
        influx_push(db);
        printf("Done...\n");
        clock_gettime(CLOCK_MONOTONIC, &end);
        size_t remaining_time = control->config->poll_interval - (end.tv_sec - start.tv_sec);
        while (remaining_time > 0) {
            sleep(1);
            clock_gettime(CLOCK_MONOTONIC, &end);
            remaining_time = control->config->poll_interval - (end.tv_sec - start.tv_sec);
            printf("%ld seconds till next run\n", remaining_time);
            if (control->stop) {
                goto thread_exit;
            }
        }
        clock_gettime(CLOCK_MONOTONIC, &start);
    }
thread_exit:
    if (tc) {
        FREE_NULL(tc->measure);
        FREE_NULL(tc->threads);
        FREE_NULL(tc);
    }
    pthread_exit(NULL);
}

void *nlnog_thread(void *thread) {
    struct thread *t = thread;
    if (ring_host_connect(t)) {
        t->error = true;
        snprintf(t->error_msg, 2048, "Failed connection to remote host");
        pthread_exit(NULL);
    }
    if (ring_init_ssh_session(t)) {
        t->error = true;
        snprintf(t->error_msg, 2048, "Failed to initialise ssh session");
        pthread_exit(NULL);
    }
    if (ring_execute_command(t, t->command)) {
        t->error = true;
        snprintf(t->error_msg, 2048, "Failed to execute command on remote host");
        pthread_exit(NULL);
    }
    t->error = false;
    pthread_exit(NULL);
}

void ring_free_conf(struct ring_conf **conf) {
    if (!*conf) {
        return;
    }
    FREE_NULL((*conf)->api_link);
    FREE_NULL((*conf)->db_host);
    FREE_NULL((*conf)->database);
    FREE_NULL((*conf)->public_key);
    FREE_NULL((*conf)->private_key);
    FREE_NULL((*conf)->username);
    FREE_NULL((*conf)->password);
    FREE_NULL((*conf)->target);
    FREE_NULL(*conf);
}

struct ring_conf *read_nlnog_config(const char *fname) {
    struct ring_conf *conf = calloc(1, sizeof(struct ring_conf));
    if (!conf)
        return NULL;
    char *conf_buffer;
    json_object *obj = json_get_from_file(fname, &conf_buffer);
    if (!obj) {
        printf("Failed getting configuration from file %s\n", fname);
        goto conf_cleanup;
    }
    if (json_get_str(obj, "/nlnog_ring/api_link", &conf->api_link))
        goto conf_cleanup;
    if (json_get_int(obj, "/nlnog_ring/max_nodes", &conf->max_nodes))
        goto conf_cleanup;
    if (json_get_int(obj, "/nlnog_ring/num_pings", &conf->num_pings))
        goto conf_cleanup;
    if (json_get_int(obj, "/nlnog_ring/poll_interval", &conf->poll_interval))
        goto conf_cleanup;
    if (json_get_str(obj, "/nlnog_ring/database/host", &conf->db_host))
        goto conf_cleanup;
    if (json_get_str(obj, "/nlnog_ring/database/database", &conf->database))
        goto conf_cleanup;
    if (json_get_u16(obj, "/nlnog_ring/database/port", &conf->db_port))
        goto conf_cleanup;
    if (json_get_str(obj, "/nlnog_ring/public_key", &conf->public_key))
        goto conf_cleanup;
    if (json_get_str(obj, "/nlnog_ring/private_key", &conf->private_key))
        goto conf_cleanup;
    if (json_get_str(obj, "/nlnog_ring/username", &conf->username))
        goto conf_cleanup;
    if (json_get_str(obj, "/nlnog_ring/target", &conf->target))
        goto conf_cleanup;
    json_get_str(obj, "/nlnog_ring/password", &conf->password);
    json_object_put(obj);
    return conf;
conf_cleanup:
    ring_free_conf(&conf);
    return NULL;
}