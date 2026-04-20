//
// Created by andrew on 4/10/26.
//

#ifndef NETWORK_BRAIN_JSON_HELPER_H
#define NETWORK_BRAIN_JSON_HELPER_H

/** \latexonly\newpage\endlatexonly
 * @brief Reads the contents of a file into a buffer and returns a json object
 * @details Note, this function allocates space for the file contents in buffer, which must be freed by the caller
 * @param[in] fname File name to read
 * @param[in] buffer A double pointer where a buffer will be allocated and the file contents will be placed
 * @returns A parsed json object (or NULL if parsing failed)
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
json_object *json_get_from_file(const char *fname, char **buffer);

/** \latexonly\newpage\endlatexonly
 * @brief Gets a numeric value from a json object at the specified path and stores the result as a __u64
 * @param[in] obj A parsed json object
 * @param[in] path The json path to the value being sought
 * @param[in] target A pointer to a __u64 in which to store the result
 * @returns 0 on success or -1 on failure
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
int json_get_int64(json_object *obj, const char *path, __u64 *target);


/** \latexonly\newpage\endlatexonly
 * @brief Gets a numeric value from a json object at the specified path and stores the result as a __u32
 * @param[in] obj A parsed json object
 * @param[in] path The json path to the value being sought
 * @param[in] target A pointer to a __u32 in which to store the result
 * @returns 0 on success or -1 on failure
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
int json_get_int(json_object *obj, const char *path, __u32 *target);


/** \latexonly\newpage\endlatexonly
 * @brief Gets a numeric value from a json object at the specified path and stores the result as a __u16
 * @param[in] obj A parsed json object
 * @param[in] path The json path to the value being sought
 * @param[in] target A pointer to a __u16 in which to store the result
 * @returns 0 on success or -1 on failure
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
int json_get_u16(json_object *obj, const char *path, __u16 *target);


/** \latexonly\newpage\endlatexonly
 * @brief Gets a string value from a json object at the specified path and stores the result
 * @details This function allocates for the retrieved string, and the retrieved string must be freed by the caller
 * @param[in] obj A parsed json object
 * @param[in] path The json path to the value being sought
 * @param[in] str A double pointer, a single dereference of which will be used for allocating and storing the string
 * @returns 0 on success or -1 on failure
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
int json_get_str(json_object *obj, const char *path, char **str);

/** \latexonly\newpage\endlatexonly
 * @brief Gets a boolean value from a json object at the specified path and stores the result in *value
 * @param[in] obj A parsed json object
 * @param[in] path The json path to the value being sought
 * @param[in] value A pointer to a boolean in which to store the result
 * @returns 0 on success or -1 on failure
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
int json_get_bool(json_object *obj, const char *path, bool *value);

/** \latexonly\newpage\endlatexonly
 * @brief Frees up a json object
 * @callgraph
 * @callergraph
 * \latexonly\newpage\endlatexonly
 */
void json_free(json_object *obj);

#endif //NETWORK_BRAIN_JSON_HELPER_H