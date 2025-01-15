#include "usage_analytics.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>


size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    return size * nmemb;
}

int is_ibm_domain(const char *hostname) {
    return strstr(hostname, "ibm.com") != NULL;
}

void get_fqdn(char *fqdn, size_t size) {
    char hostname[256];
    struct addrinfo hints, *res;
    int ret;

    gethostname(hostname, sizeof(hostname));
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    ret = getaddrinfo(hostname, NULL, &hints, &res);
    if (ret == 0 && res) {
        getnameinfo(res->ai_addr, res->ai_addrlen, fqdn, size, NULL, 0, 0);
        freeaddrinfo(res);
    } else {
        strncpy(fqdn, hostname, size - 1);
        fqdn[size - 1] = '\0';
    }
}

void get_system_info(char **os_release, char **cpu_arch) {
    struct utsname uname_data;
    uname(&uname_data);

    *os_release = malloc(strlen(uname_data.sysname) + strlen(uname_data.release) + 2);
    sprintf(*os_release, "%s %s", uname_data.sysname, uname_data.release);

    *cpu_arch = malloc(strlen(uname_data.machine) + 1);
    strcpy(*cpu_arch, uname_data.machine);
}

void get_local_ip(char *local_ip, size_t size) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in serv;

    if (sock < 0) {
        strncpy(local_ip, "127.0.0.1", size);
        return;
    }

    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port = htons(80);
    inet_pton(AF_INET, "8.8.8.8", &serv.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv, sizeof(serv)) == 0) {
        struct sockaddr_in local;
        socklen_t local_len = sizeof(local);
        if (getsockname(sock, (struct sockaddr *)&local, &local_len) == 0) {
            inet_ntop(AF_INET, &local.sin_addr, local_ip, size);
        } else {
            strncpy(local_ip, "127.0.0.1", size);
        }
    } else {
        strncpy(local_ip, "127.0.0.1", size);
    }

    close(sock);
}

char *get_app_version(const char *argv0) {
    char *app_version = malloc(256);
    char *last_slash = strrchr(argv0, '/');

    if (last_slash == NULL) {
        strcpy(app_version, "unknown");
    } else {
        char *parent_dir = strndup(argv0, last_slash - argv0);
        char version_file_path[512];
        snprintf(version_file_path, sizeof(version_file_path), "%s/.version", parent_dir);

        struct stat buffer;
        if (stat(version_file_path, &buffer) == 0) {
            FILE *version_file = fopen(version_file_path, "r");
            if (version_file) {
                if (fgets(app_version, 256, version_file) != NULL) {
                    size_t len = strlen(app_version);
                    if (len > 0 && app_version[len - 1] == '\n') {
                        app_version[len - 1] = '\0';
                    }
                } else {
                    strcpy(app_version, "unknown");
                }
                fclose(version_file);
            }
        } else {
            strcpy(app_version, "unknown");
        }
        free(parent_dir);
    }
    return app_version;
}

void *send_usage_data_thread(void *arg) {
    CURL *easy_handle;
    double duration;
    CURLcode res;

    // Measure start time
    clock_t start_time = clock();

    duration = (double)(clock() - start_time) / CLOCKS_PER_SEC;
    printf("1. Total duration: %.6f seconds\n", duration);
    char fqdn[256];
    get_fqdn(fqdn, sizeof(fqdn)); // This call is the biggest cost now

    duration = (double)(clock() - start_time) / CLOCKS_PER_SEC;
    printf("2. Total duration: %.6f seconds\n", duration);
    if (!is_ibm_domain(fqdn)) {
        fprintf(stderr, "Skipping usage collection for non-IBM domain: %s\n", fqdn);
        return NULL;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    easy_handle = curl_easy_init();

    if (easy_handle) {
        const char *program_name = getprogname();
    duration = (double)(clock() - start_time) / CLOCKS_PER_SEC;
    printf("3. Total duration: %.6f seconds\n", duration);

        char local_ip[INET_ADDRSTRLEN];
        get_local_ip(local_ip, sizeof(local_ip));
    duration = (double)(clock() - start_time) / CLOCKS_PER_SEC;
    printf("4. Total duration: %.6f seconds\n", duration);

        char *os_release = NULL;
        char *cpu_arch = NULL;
        get_system_info(&os_release, &cpu_arch);
    duration = (double)(clock() - start_time) / CLOCKS_PER_SEC;
    printf("5. Total duration: %.6f seconds\n", duration);

        char *app_version = get_app_version(program_name);

        time_t now = time(NULL);
        struct tm *timeinfo = gmtime(&now);
        char timestamp[25];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", timeinfo);
    duration = (double)(clock() - start_time) / CLOCKS_PER_SEC;
    printf("6. Total duration: %.6f seconds\n", duration);

        char *url = "http://rogi21.fyre.ibm.com:3000/usage";

        char post_data[4096];
        snprintf(post_data, sizeof(post_data),
                 "{\"program_name\": \"%s\", \"fqdn\": \"%s\", \"local_ip\": \"%s\", \"os_release\": \"%s\", \"cpu_arch\": \"%s\", \"app_version\": \"%s\", \"timestamp\": \"%s\"}",
                 program_name, fqdn, local_ip, os_release, cpu_arch, app_version, timestamp);

        if (getenv("ZUSAGE_DEBUG") && strcmp(getenv("ZUSAGE_DEBUG"), "1") == 0) {
            fprintf(stderr, "DEBUG: Sending usage data:\n");
            fprintf(stderr, "DEBUG: URL: %s\n", url);
            fprintf(stderr, "DEBUG: POST data: %s\n", post_data);
        }

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(easy_handle, CURLOPT_HTTPHEADER, headers);
    duration = (double)(clock() - start_time) / CLOCKS_PER_SEC;
    printf("7. Total duration: %.6f seconds\n", duration);

    curl_easy_setopt(easy_handle, CURLOPT_URL, url);
    curl_easy_setopt(easy_handle, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(easy_handle, CURLOPT_POSTFIELDSIZE, (long)strlen(post_data));

    curl_easy_setopt(easy_handle, CURLOPT_NOSIGNAL, 1L);

    curl_easy_perform(easy_handle);

    // Calculate and print the duration
 // Measure end time
    duration = (double)(clock() - start_time) / CLOCKS_PER_SEC;
    printf("\n8. Total duration: %.6f seconds\n", duration);

    // Cleanup after the call, regardless of whether it has finished
    curl_easy_cleanup(easy_handle);

        free(os_release);
        free(cpu_arch);
        free(app_version);
    }

    curl_global_cleanup();
    
}

__attribute__((constructor))
void usage_analytics_init() {
    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, send_usage_data_thread, NULL) != 0) {
        fprintf(stderr, "Failed to create thread for usage analytics\n");
    } else {
        pthread_detach(thread_id); // Ensure resources are cleaned up when thread exits
    }
}

int main() {
    // Main program logic
    printf("BLABLA");
    return 0;
}

