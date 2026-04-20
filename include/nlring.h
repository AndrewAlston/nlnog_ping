//
// Created by andrew on 3/26/26.
//

#ifndef NETWORK_BRAIN_RING_H
#define NETWORK_BRAIN_RING_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <asm-generic/types.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <libssh2.h>
#include <pthread.h>
#include <json-c/json.h>


#ifndef BUFFER_SIZE
#define BUFFER_SIZE (1024*1024)
#endif

#define KNOWN_HOSTS_FILE "/home/aalston/.ssh/known_hosts"

#define GET_RING_DOUBLE(target, ptr, end) ({ (target) = strtod((ptr), (&end)); (ptr) = end+1; })

/** @def MAX_NODES
 * @brief The maximum number of nodes we will process
 */
#define MAX_NODES 1024

/** \latexonly\newpage\endlatexonly
 * @struct url_data
 * @brief A structure containing the URL data passed to libcurl to retrieve the NLNOG data
 */
struct url_data {
    char *data; /**< The API string to be executed on the host */
    size_t length; /**< The length of the API string */
};

/** \latexonly\newpage\endlatexonly
 * @struct hosts
 * @brief Structure containing data about each of the NLNOG hosts
 */
struct hosts {
    char ipv4_addr[INET_ADDRSTRLEN]; /**< The IPv4 address of the host */
    char ipv6_addr[INET6_ADDRSTRLEN]; /**< The IPv6 address of the host */
    char hostname[1024]; /**< The hostname of the host */
    char country[16]; /**< The country code of the host */
    int active; /**< If set this host is active */
};


/** \latexonly\newpage\endlatexonly
 * @struct ring_conf
 * @brief Structure to hold nlnog ring ping configuration
 */
struct ring_conf {
    char *api_link;
    __u32 max_nodes;
    __u32 num_pings;
    __u32 poll_interval;
    char *db_host;
    __u16 db_port;
    char *database;
    char *public_key;
    char *private_key;
    char *username;
    char *password;
    char *target;
    bool bind_core;
    __u16 core;
};

/** \latexonly\newpage\endlatexonly
 * @struct json_data
 * @brief Structure used for retrieving JSON data from the NLNOG API
 */
struct json_data {
    json_object *active; /**< Pointer to the json object specifying if the node is active or not */
    json_object *alive_ipv4; /**< Pointer to the json object specifying if the node is alive */
    json_object *countrycode; /**< Pointer to the json object containing the country code for the server */
    json_object *ipv4; /**< Pointer to the json object containing the IPv4 address of the server */
    json_object *hostname; /**< Pointer to the json object containing the server host name */
};

/** \latexonly\newpage\endlatexonly
 * @struct ring_control
 * Struct passed to an nlnog_run thread
 */
struct ring_control {
    bool stop; /**< Is set stop running nlnog polling */
    pthread_t control_thread;
    bool bind_core;
    __u16 core;
    struct ring_conf *config;

};

/** \latexonly\newpage\endlatexonly
 * @struct latency_measurements
 * @brief Struct for containing retrieved latency measurements
 */
struct latency_measurements {
    char hostname[2048]; /**< The hostname on the server used to ping our defined destination */
    char address[2048]; /**< The address that is to be pinged */
    char country[16]; /**< Country code specifying the location of the server being used */
    char min_str[512]; /**< String value for the minimum ping time */
    char avg_str[512]; /**< String value for the average ping time */
    char max_str[512]; /**< String value for the maximum ping time */
    char dev_str[512]; /**< String value for the deviation time */
    char country_string[512]; /**< The full country name corresponding to the country code */
    char tag_string[4096]; /**< Influx tags to be used when pushing this to the database */
    double min; /**< The minimum ping time specified as a double */
    double avg; /**< The average ping time specified as a double */
    double max; /**< The maximum ping time specified as a double */
    double deviation; /**< The deviation time specified as a double */
};

/** \latexonly\newpage\endlatexonly
 * @struct thread "41.86.34.234",
 * @brief Thread structure for each processing thread
 */
struct thread {
    pthread_t thread; /**< Thread controller */
    struct hosts host; /**< Host structure containing information about the host being connected to */
    int socket; /**< Socket used for this connection */
    LIBSSH2_SESSION *session; /**< Pointer to a LIBSSH2_SESSION for the ssh session */
    LIBSSH2_CHANNEL *channel; /**< Pointer to a LIBSSH2_CHANNEL structure for the ssh channel */
    LIBSSH2_KNOWNHOSTS *known_hosts; /**< Pointer to a LIBSSH2_KNOWNHOSTS structure for known hosts */
    char pubkey[1024]; /**< The public key used by this thread */
    char private_key[1024]; /**< The private key used by this thread */
    char username[256]; /**< The username to use for the ssh session */
    char password[256]; /**< The password to use for the ssh session (if password authentication is used) */
    char target[256]; /**< The Target of this ssh session */
    char buffer[BUFFER_SIZE]; /**< Buffer used for reading and writing on this ssh session */
    bool skip; /**< If set, skip this thread */
    bool error; /**< If set an error occurred and a message is stored in error_msg */
    char error_msg[2048]; /**< An error message (if error is set) */
    bool running; /**< If set this thread is still running */
    struct sockaddr_in serv_socket; /**< Socket info for the server we are connecting to */
    struct influx *db; /**< Pointer to an influx database structure */
    struct latency_measurements measurement; /**< The latency measurement for this thread */
    __u64 epoch_ts; /**< The epoch timestamp for this measurement */
    char command[2048]; /**< The command to be executed on the remote host */
};

/** \latexonly\newpage\endlatexonly
 * @struct t_container
 * @brief Thread container structure
 */
struct t_container {
    struct thread *threads; /**< Array of thread structures */
    int num_threads; /**< Number of threads in thread structure array */
    struct measurements *measure; /**< Measurement structure */
};


/** \latexonly\newpage\endlatexonly
 * @brief Used by curl to handle received data
 * @param[in] buffer The data buffer containing received data
 * @param[in] size The original size of the data
 * @param[in] n_members Size multiplier
 * @param[in] url_data Void pointer passed to store received data
 * @returns The size of the data in the buffer
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
size_t get_curl_data(const void *buffer, size_t size, size_t n_members, void *url_data);

/** \latexonly\newpage\endlatexonly
 * @brief Retrieves json encoded ring node data
 * @returns A pointer to a url_data structure on success, or NULL on failure
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
struct url_data *retrieve_node_data();

/** \latexonly\newpage\endlatexonly
 * @brief Populates a host structure with data parsed from the json data returned by retrieve_node_data()
 * @param[in] node_count The node count to retrieve
 * @param[in] obj Pointer to the parsed json object
 * @param[in] host Pointer to a host structure to store the retrieved information
 * @returns 0 on success or -1 on failure
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
int populate_node_check(int node_count, json_object *obj, struct hosts *host);

/** \latexonly\newpage\endlatexonly
 * @brief Parses a json encoded string buffer into an array of host structures held in a t_container struct
 * @param[in] data Pointer to a url_data structure storing the encoded data string
 * @returns A pointer to a t_container structure containing a list of hosts to be polled
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
struct t_container *parse_node_data(const struct url_data *data);

/** \latexonly\newpage\endlatexonly
 * @brief Returns a latency measurement structure to be further populated post latency poll
 * @param[in] t A pointer to a thread structure containing a valid host entry to poll
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
struct latency_measurements *allocate_latency_measurement(struct thread *t);

/** \latexonly\newpage\endlatexonly
 * @brief Process returned poll data storing the result in the threads measurement structure
 * @param[in] t A pointer to a thread structure containing the measurement data
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
int process_poll_data(struct thread *t);

/** \latexonly\newpage\endlatexonly
 * @brief Initialise an array of thread structures contained in a t_container structure to be used for polling
 * @returns A populated t_container structure pointer
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
struct t_container *ring_poll_thread_init();

/** \latexonly\newpage\endlatexonly
 * @brief Frees up a NLNOG Ring Configuration structure
 * @param[in] conf Double pointer to the configuration to be freed up
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
void ring_free_conf(struct ring_conf **conf);

/** \latexonly\newpage\endlatexonly
 * @brief Reads the configuration file for NLNOG ring information
 * @param[in] fname The file name storing the configuration data
 * @returns A pointer to a populated ring_conf structure on success or NULL on failure
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
struct ring_conf *read_nlnog_config(const char *fname);

/** \latexonly\newpage\endlatexonly
 * @brief Runs the NLNOG ring poller
 * @param[in] ring Pointer to a ring_control structure
 * @returns NULL
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
void *nlnog_run(void *ring);

/** \latexonly\newpage\endlatexonly
 * @brief Thread function to execute remote ping commands
 * @param[in] thread Pointer to a thread structure that contains the relevant polling details
 * @returns NULL
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
void *nlnog_thread(void *thread);
#endif //NETWORK_BRAIN_RING_H