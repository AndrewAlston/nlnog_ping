//
// Created by andrew on 3/21/26.
//


#ifndef NETWORK_BRAIN_GENERIC_H
#define NETWORK_BRAIN_GENERIC_H

#include <asm-generic/types.h>
#include <arpa/inet.h>

/** @def FREE_NULL
 * @brief Free's a given pointer and sets it to NULL
 * @param[in] ptr Pointer to be freed
 */
#define FREE_NULL(ptr) ({ if((ptr) != NULL) { free((ptr)); ((ptr)) = NULL; }})


/** @brief Dump size bytes from void pointer in hex format with offset markers
 * @param[in] buffer Void pointer to buffer
 * @param[in] size Number of bytes to dump
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
void dump_buffer(void *buffer,__u16 size);

/** @def Macro for setting functions as always inline */
#define INLINE static __attribute__((always_inline)) inline


#endif //NETWORK_BRAIN_GENERIC_H
