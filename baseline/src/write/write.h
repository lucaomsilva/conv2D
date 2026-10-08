#ifndef WRITE_H
#define WRITE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Writes execution time, binary name, and date of run to a CSV file inside the 'docs' directory.
 * 
 * Line format written: <binary name>, <data>, <time>
 *
 * If binary_name is NULL, the running binary name is automatically determined via /proc/self/exe.
 * If data_run is NULL, current date/timestamp is automatically generated.
 *
 * @param data_run String representing date/time of run or label (if NULL or empty, current ISO timestamp is used).
 * @param time_sec Execution time in seconds.
 * @return int 0 on success, non-zero (-1) on error.
 */
int write_time_csv(const char *data_run, double time_sec);

/**
 * @brief Writes execution time with explicit binary name.
 *
 * @param binary_name Name of the binary (if NULL, auto-detected).
 * @param data_run Date of run string (if NULL, current ISO timestamp is used).
 * @param time_sec Execution time in seconds.
 * @return int 0 on success, non-zero (-1) on error.
 */
int write_time_csv_ex(const char *binary_name, const char *data_run, double time_sec);

/**
 * @brief Custom version allowing specification of CSV filename inside the docs directory.
 * 
 * @param filename CSV file name inside 'docs' (if NULL, defaults to "execution_time.csv").
 * @param binary_name Name of the binary (if NULL, auto-detected).
 * @param data_run Date of run string (if NULL, current ISO timestamp is used).
 * @param time_sec Execution time in seconds.
 * @return int 0 on success, non-zero (-1) on error.
 */
int write_time_csv_file(const char *filename, const char *binary_name, const char *data_run, double time_sec);

/**
 * @brief Low-level function allowing time string directly.
 * 
 * @param filename CSV file name inside 'docs' (if NULL, defaults to "execution_time.csv").
 * @param binary_name Name of the binary (if NULL, auto-detected).
 * @param data_run Date of run string (if NULL, current ISO timestamp is used).
 * @param time_str String representing execution time.
 * @return int 0 on success, non-zero (-1) on error.
 */
int write_time_csv_str(const char *filename, const char *binary_name, const char *data_run, const char *time_str);

/**
 * @brief Writes execution time to a specific target CSV filepath.
 * 
 * Line format written: <binary name>, <data>, <time>
 *
 * @param filepath Destination filepath for the CSV file.
 * @param binary_name Name of binary (if NULL, auto-detected).
 * @param data_run Date/timestamp (if NULL, current ISO timestamp).
 * @param time_sec Execution time in seconds.
 * @return int 0 on success, non-zero (-1) on error.
 */
int write_time_to_filepath(const char *filepath, const char *binary_name, const char *data_run, double time_sec);

#ifdef __cplusplus
}
#endif

#endif // WRITE_H
