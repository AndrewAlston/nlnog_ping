//
// Created by andrew on 1/28/26.
//

#ifndef BGP_OPTIMISER_INFLUX_WIRE_H
#define BGP_OPTIMISER_INFLUX_WIRE_H

#include <asm-generic/types.h>
#include <netinet/in.h>

#ifdef DEBUG
#define DEBUG_PRINT(x) {((x));}
#else
#define DEBUG_PRINT(x)
#endif


/** @def BUFFER_SIZE
 * @brief The size of the influx read / write buffer */
#define BUFFER_SIZE (1024*1024)

/** @def MAX_TAG_LIMIT
 * @brief Maximum size of tag strings added to the influx db structure
 */
#define MAX_TAG_LIMIT 1023

/** @def INFLUX_DB_FINISH
 * @brief Simple macro used to close off influx queries / seconds and perform function return
 * @param[in] ret_code The function return code to be used
 */
#define INFLUX_DB_FINISH(ret_code) ({ close(db->socket); memset(db->buffer, 0, BUFFER_SIZE); return (ret_code); })

/** @def INFLUX_WRITE
 * @brief Set the header to use influx write API
 */
#define INFLUX_WRITE 1

/** @def INFLUX_QUERY
 * @brief Set the header to use influx query API
 */
#define INFLUX_QUERY 2

/** @def INFLUX_REMAINING
 * @brief Basic macro to return the remaining available capacity in the read / write buffer
 */
#define INFLUX_REMAINING ({ BUFFER_SIZE - strlen(measurement->buffer); })

/** @def RESULT_SIZE
 * @brief Amount to read when waiting for a result
 */
#define RESULT_SIZE 8192

/** \latexonly\newpage\endlatexonly
 * @struct url_encoder
 * @brief Used for performing URL encoding for use in API calls
 */
struct url_encoder {
    char rfc3985[256]; /**< Host encoding for RFC3986 encodes */
    char html5[256]; /**< Hold encoding for html 5 encodes */
};

/** \latexonly\newpage\endlatexonly
 * @struct influx_measurement
 * @brief Structure containing an influx measurement
 */
struct influx_measurement {
     char influx_tags[MAX_TAG_LIMIT]; /**< The tags applied to this measurement */
     char buffer[BUFFER_SIZE]; /**< The full measurement as it will be written to the database */
};

/** \latexonly\newpage\endlatexonly
 * @struct influx
 * @brief Structure used for influx communications
 */
struct influx {
    char hostname[256]; /**< Hostname of the influx server (currently requires an IP address) */
    __u32 host_ip; /**< IPv4 address of the server as derived from the IP address in the hostname */
    __u16 host_port; /**< Port on which the server is listening */
    char buffer[BUFFER_SIZE]; /**< Buffer for reading/writing from the influx database */
    char encoded[BUFFER_SIZE*3]; /**< Used for holding URL encoded query strings */
    char username[256]; /**< Username for the influx database if using authentication */
    char password[256]; /**< Password for the influx database if using authentication */
    char influx_tags[MAX_TAG_LIMIT]; /**< Tags to be applied to measurements on insertion */
    char database[256]; /**< Name of the database on the influx server */
    char result[8192]; /**< Buffer to store results of queries to the influx server */
    int socket; /**< Socket used to communicate with the influx server */
    struct sockaddr_in serv_addr; /**< Socket information structure */
    struct influx_measurement *measurement; /**< Measurement structure array containing the actual measurements */
    int n_measurement; /**< Number of elements in the measurement array */
};

/** \latexonly\newpage\endlatexonly
 * @brief Creates a new influx structure for database communication
 * @param[in] ip IP Address of the server in string format
 * @param[in] port Port the influx server is listening on
 * @param[in] username Username to use if authentication is enabled, NULL if no authentication
 * @param[in] password Password to use in authentication is enabled, NULL if no authentication
 * @param[in] database Name of the database on the influx server
 * @returns An influx structure on success or NULL on failure
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
struct influx *influx_new(const char *ip, __u16 port, const char *username, const char *password, const char *database);

/** \latexonly\newpage\endlatexonly
 * @brief Sets the tags to apply to an inserted measurement
 * @param[in] measurement Pointer to an influx database measurement structure
 * @param[in] tags String of tags to use for insertion of measurements
 * @returns 0 on succcess or -1 on failure
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
int influx_set_tags(struct influx_measurement *measurement, char *tags);

/** \latexonly\newpage\endlatexonly
 * @brief Used to start a database measurement - this sets the measurement name and must be called before adding columns
 * @param[in] measurement Pointer to an influx database measurement structure
 * @param[in] section Name of the measurement
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
void influx_start_measurement(struct influx_measurement *measurement, char *section);

/** \latexonly\newpage\endlatexonly
 * @brief Used to end a database measurement - this must be called before calling influx_push
 * @param[in] measurement Pointer to an influx database measurement structure
 * @param[in] ts Timestamp in nanoseconds for the measurement
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
void influx_end_measurement(struct influx_measurement *measurement, __u64 ts);

/** \latexonly\newpage\endlatexonly
 * @brief Used to add a long unsigned value to a measurement
 * @param[in] measurement Pointer to an influx database measurement structure
 * @param[in] name Name of the measurement
 * @param[in] value The value specified as a double long unsigned number
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
void influx_add_long(struct influx_measurement *measurement, char *name, long long value);

/** \latexonly\newpage\endlatexonly
 * @brief Used to add a double (64bit float) to a measurement
 * @param[in] measurement Pointer to an influx database measurement structure
 * @param[in] name Name of the measurement
 * @param[in] value The value specified as a double
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
void influx_add_double(struct influx_measurement *measurement, char *name, double value);

/** \latexonly\newpage\endlatexonly
 * @brief Used to add a string measurement
 * @param[in] measurement Pointer to an influx database measurement structure
 * @param[in] name The name of the measurement
 * @param[in] value The string to add
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
void influx_add_string(struct influx_measurement *measurement, const char *name, char *value);

/** \latexonly\newpage\endlatexonly
 * @brief Adds a boolean field to the database
 * @param[in] measurement Pointer to an influx database measurement structure
 * @param[in] name The name of the field to be inserted
 * @param[in] value A __u64 value, if zero the field is set to false, else field is set to true
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
void influx_add_boolean(struct influx_measurement *measurement, char *name, __u64 value);

/** \latexonly\newpage\endlatexonly
 * @brief Creates a socket connection to the influx database
 * @param[in] db Pointer to a populated influx database structure
 * @returns 0 on success or -1 on failure
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
int influx_connect_socket(struct influx *db);

/** \latexonly\newpage\endlatexonly
 * @brief Zero out descriptor sets used for select
 * @param[in] fds_read Pointer to a fd_set used for read
 * @param[in] fds_write Pointer to a fd_set used for write
 * @param[in] fds_except Pointer to a fd_set used for exceptions
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
void influx_zero_descriptors(fd_set *fds_read, fd_set *fds_write, fd_set *fds_except);

/** \latexonly\newpage\endlatexonly
 * @brief Waits for a second based on the supplied descriptors
 * @param[in] db A pointer to the influx database structure
 * @param[in] read A pointer to a populated file descriptor set for reading
 * @param[in] write A pointer to a populated file descriptor set for writing
 * @param[in] expect A pointer to a populated file descriptor set for exceptions
 * @param[in] timeout A timeout value in seconds
 * @returns 0 on success or -1 on failure
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
int influx_wait_socket(struct influx *db, fd_set *read, fd_set *write, fd_set *expect, int timeout);

/** \latexonly\newpage\endlatexonly
 * @brief Sends the contents of db->buffer to the socket
 * @param[in] db Pointer to a populated db structure containing a connected socket and the buffer to be sent
 * @returns 0 on success or -1 on failure
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
int influx_send_buffer(struct influx *db);

/** \latexonly\newpage\endlatexonly
 * @brief Sends a query to the database
 * @details If action is INFLUX_WRITE this sends only a write API call and must be followed by a further send
 * @details If action is INFLUX_QUERY this will send a query API call and the contents of db->encoded
 * @param[in] db A pointer to a populated influx structure
 * @param[in] action The action to perform (either INFLUX_WRITE or INFLUX_QUERY)
 * @returns 0 on success or -1 on failure
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
int influx_send_query(struct influx *db, int action);

/** \latexonly\newpage\endlatexonly
 * @brief Used to run the final database push.  This is the last step in adding to the database
 * @details influx_new, influx_set_tags, influx_start_measurement, Various adds, and influx_end_measurement must be called before this
 * @param[in] db Pointer to the influx database control structure
 * @returns
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
int influx_push(struct influx *db);

/** \latexonly\newpage\endlatexonly
 * @brief Returns a structure used for URL encoding
 * @returns A url_encoder structure on success or NULL on failure
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
struct url_encoder *url_init_encoder();

/** \latexonly\newpage\endlatexonly
 * @brief Encodes the contents of s using the specified encoder table, storing the result in enc
 * @param[in] table An encoder table generated with url_init_encoder()
 * @param[in] s The string to encode
 * @param[in] enc The output buffer in which to store the encoded string
 * @param[in] end The end of the output buffer (used to prevent overflows)
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
int url_encode(const char *table, const unsigned char *s, char *enc, const char *end);

/** \latexonly\newpage\endlatexonly
 * @brief Run an influx query
 * @param[in] db Pointer to a populated influx database structure
 * @param[in] query Query to perform
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
int influx_run_query(struct influx *db, const unsigned char *query);

/** \latexonly\newpage\endlatexonly
 * @brief Adds a new influx measurement to the database structure
 * @param[in] db Pointer to a populated influx database structure
 * @returns Pointer to the newly added measurement
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
struct influx_measurement *influx_add_measurement(struct influx *db);
#endif //BGP_OPTIMISER_INFLUX_WIRE_H