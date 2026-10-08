#include "write.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

/**
 * Recursive directory creation helper (similar to mkdir -p).
 */
static int create_dir_p(const char *path) {
    char temp[PATH_MAX + 128];
    char *p = NULL;
    size_t len;

    if (!path || strlen(path) == 0) {
        return -1;
    }

    snprintf(temp, sizeof(temp), "%s", path);
    len = strlen(temp);
    if (temp[len - 1] == '/') {
        temp[len - 1] = '\0';
    }

    for (p = temp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(temp, 0755) != 0 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }
    if (mkdir(temp, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

/**
 * Determines the target 'docs' directory relative to the binary executable root path.
 */
static void get_docs_directory(char *docs_dir_buf, size_t buf_size) {
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);

    if (len != -1) {
        exe_path[len] = '\0';

        // Extract directory containing the executable (e.g. .../baseline/bin)
        char *last_slash = strrchr(exe_path, '/');
        if (last_slash) {
            *last_slash = '\0';

            // If binary is inside a "bin" folder, strip "/bin" to reach binary root
            char *bin_suffix = strrchr(exe_path, '/');
            if (bin_suffix && strcmp(bin_suffix, "/bin") == 0) {
                *bin_suffix = '\0';
            }

            // Check if parent directory contains 'docs' directory or '.git' (project root)
            char parent_docs[PATH_MAX + 64];
            snprintf(parent_docs, sizeof(parent_docs), "%s/../docs", exe_path);

            struct stat st;
            if (stat(parent_docs, &st) == 0 && S_ISDIR(st.st_mode)) {
                snprintf(docs_dir_buf, buf_size, "%s", parent_docs);
                return;
            }

            char parent_git[PATH_MAX + 64];
            snprintf(parent_git, sizeof(parent_git), "%s/../.git", exe_path);
            if (stat(parent_git, &st) == 0) {
                snprintf(docs_dir_buf, buf_size, "%s/../docs", exe_path);
                return;
            }

            // Fallback to binary root / docs
            snprintf(docs_dir_buf, buf_size, "%s/docs", exe_path);
            return;
        }
    }

    // Fallback if readlink is unavailable
    struct stat st;
    if (stat("../docs", &st) == 0 && S_ISDIR(st.st_mode)) {
        snprintf(docs_dir_buf, buf_size, "../docs");
    } else {
        snprintf(docs_dir_buf, buf_size, "docs");
    }
}

/**
 * Auto-detects the binary executable name via /proc/self/exe.
 */
static void get_binary_name(char *buf, size_t size) {
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len != -1) {
        exe_path[len] = '\0';
        char *last_slash = strrchr(exe_path, '/');
        if (last_slash) {
            snprintf(buf, size, "%s", last_slash + 1);
            return;
        }
    }
    snprintf(buf, size, "unknown_binary");
}

/**
 * Gets current timestamp in ISO format: YYYY-MM-DD HH:MM:SS
 */
static void get_current_iso_time(char *buf, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (tm_info) {
        strftime(buf, size, "%Y-%m-%d %H:%M:%S", tm_info);
    } else {
        snprintf(buf, size, "unknown_date");
    }
}

int write_time_csv_str(const char *filename, const char *binary_name, const char *data_run, const char *time_str) {
    if (!filename || strlen(filename) == 0) {
        filename = "execution_time.csv";
    }

    char docs_dir[PATH_MAX + 64];
    get_docs_directory(docs_dir, sizeof(docs_dir));

    // Ensure docs directory exists
    if (create_dir_p(docs_dir) != 0) {
        fprintf(stderr, "Error: Could not create or access directory '%s'\n", docs_dir);
        return -1;
    }

    char full_filepath[PATH_MAX + 128];
    snprintf(full_filepath, sizeof(full_filepath), "%s/%s", docs_dir, filename);

    // Check if file exists and has content > 0 bytes
    struct stat st;
    int file_has_content = (stat(full_filepath, &st) == 0 && st.st_size > 0);

    FILE *file = fopen(full_filepath, "a");
    if (!file) {
        fprintf(stderr, "Error: Could not open file '%s' for writing\n", full_filepath);
        return -1;
    }

    // Write header if creating a new file or empty file
    if (!file_has_content) {
        fprintf(file, "binary_name, data_run, time\n");
    }

    // Determine binary_name string if not provided
    char auto_binary_name[256];
    if (!binary_name || strlen(binary_name) == 0) {
        get_binary_name(auto_binary_name, sizeof(auto_binary_name));
        binary_name = auto_binary_name;
    }

    // Determine data_run string if not provided
    char date_buf[128];
    if (!data_run || strlen(data_run) == 0) {
        get_current_iso_time(date_buf, sizeof(date_buf));
        data_run = date_buf;
    }

    // Write row in format: <binary name>, <data>, <time>
    fprintf(file, "%s, %s, %s\n", binary_name, data_run, time_str ? time_str : "0.000000");

    fclose(file);
    return 0;
}

int write_time_csv_file(const char *filename, const char *binary_name, const char *data_run, double time_sec) {
    char time_str[64];
    snprintf(time_str, sizeof(time_str), "%.6f", time_sec);
    return write_time_csv_str(filename, binary_name, data_run, time_str);
}

int write_time_csv_ex(const char *binary_name, const char *data_run, double time_sec) {
    return write_time_csv_file("execution_time.csv", binary_name, data_run, time_sec);
}

int write_time_csv(const char *data_run, double time_sec) {
    return write_time_csv_ex(NULL, data_run, time_sec);
}

int write_time_to_filepath(const char *filepath, const char *binary_name, const char *data_run, double time_sec) {
    if (!filepath || strlen(filepath) == 0) {
        return 0;
    }

    // Ensure parent directory of target filepath exists
    char dir_buf[PATH_MAX + 128];
    snprintf(dir_buf, sizeof(dir_buf), "%s", filepath);
    char *last_slash = strrchr(dir_buf, '/');
    if (last_slash) {
        *last_slash = '\0';
        if (strlen(dir_buf) > 0) {
            create_dir_p(dir_buf);
        }
    }

    // Check if file exists and has content > 0 bytes
    struct stat st;
    int file_has_content = (stat(filepath, &st) == 0 && st.st_size > 0);

    FILE *file = fopen(filepath, "a");
    if (!file) {
        fprintf(stderr, "Error: Could not open file '%s' for writing\n", filepath);
        return -1;
    }

    // Write header if creating a new file or empty file
    if (!file_has_content) {
        fprintf(file, "binary_name, data_run, time\n");
    }

    // Determine binary_name string if not provided
    char auto_binary_name[256];
    if (!binary_name || strlen(binary_name) == 0) {
        get_binary_name(auto_binary_name, sizeof(auto_binary_name));
        binary_name = auto_binary_name;
    }

    // Determine data_run string if not provided
    char date_buf[128];
    if (!data_run || strlen(data_run) == 0) {
        get_current_iso_time(date_buf, sizeof(date_buf));
        data_run = date_buf;
    }

    // Write row in format: <binary name>, <data>, <time>
    fprintf(file, "%s, %s, %.6f\n", binary_name, data_run, time_sec);

    fclose(file);
    return 0;
}
