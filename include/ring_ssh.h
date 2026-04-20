//
// Created by andrew on 3/27/26.
//

#ifndef NETWORK_BRAIN_RING_SSH_H
#define NETWORK_BRAIN_RING_SSH_H

#include <libssh2.h>
#define KNOWN_HOST_MASK (LIBSSH2_KNOWNHOST_TYPE_PLAIN|LIBSSH2_KNOWNHOST_KEYENC_RAW)

/** \latexonly\newpage\endlatexonly
 * @brief Used for waiting on non-blocking sockets
 * @param[in] socket_fd An SSH2 socket
 * @param[in] session An SSH2 session
 * @returns The result of a select() call
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
int ssh_wait_socket(libssh2_socket_t socket_fd, LIBSSH2_SESSION *session);

/** \latexonly\newpage\endlatexonly
 * @brief Establish an initial socket connection using the supplied parameters
 * @param[in] t A thread structure populated by ring_poll_thread_init()
 * @returns 0 on success or -1 on failure
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
int ring_host_connect(struct thread *t);

/** \latexonly\newpage\endlatexonly
 * @brief Cleans up a ring ssh session
 * @param[in] t A thread structure populated by ring_poll_thread_init()
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
void ring_ssh_cleanup(struct thread *t);

/** \latexonly\newpage\endlatexonly
 * @brief Initiate an SSH session
 * @details This must be called only after a successful call to ring_host_connect
 * @param[in] t A thread structure populated by ring_poll_thread_init() and against which ring_host_connect() has been called
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
int ring_init_ssh_session(struct thread *t);

/** \latexonly\newpage\endlatexonly
 * @brief Execute a command on an SSH session initiated by ring_init_ssh_session()
 * @param[in] t A thread structure containing an initialised ssh session
 * @param[in] command A command string to execute across the initialised ssh session
 * @returns 0 on success or -1 on failure
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
int ring_execute_command(struct thread *t, const char *command);

#endif //NETWORK_BRAIN_RING_SSH_H