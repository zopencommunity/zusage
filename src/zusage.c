#include "usage_analytics.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <limits.h>
#include <stdarg.h>
#include <_Nascii.h>


// --- Macro Definitions ---

#define USAGE_ANALYTICS_URL "rogi21.fyre.ibm.com"
#define USAGE_ANALYTICS_PATH "/usage"
#define USAGE_ANALYTICS_PORT 3000
#define VERSION_FILE_RELATIVE_PATH "/../.version" // Assuming zopen / OEF structure

#define MAX_HOSTNAME_LENGTH _POSIX_HOST_NAME_MAX

#define MAX_IP_ADDRESS_LENGTH INET_ADDRSTRLEN

// ISO 8601 timestamp: YYYY-MM-DDTHH:MM:SSZ (20 characters)
#define MAX_TIMESTAMP_LENGTH 20

// Maximum size of POST data (4KB seems reasonable)
#define MAX_POST_DATA_SIZE 4096

#define MAX_APP_VERSION_LENGTH 100
#define MAX_OS_RELEASE_LENGTH 32
#define MAX_CPU_ARCH_LENGTH 16

#ifdef ZUSAGE_TIMING
#define START_TIMER clock_t start_time = clock();
#define END_TIMER(label) \
  duration = (double)(clock() - start_time) / CLOCKS_PER_SEC; \
  print_debug("%s: %.6f seconds", label, duration);
#else
#define START_TIMER
#define END_TIMER(label)
#endif

// Error handling macro (replace with your preferred method)
#define CHECK_ERROR(result, message) \
    if (result < 0) { \
      print_debug(message); \
    }


void print_debug(const char *format, ...) {
  if (getenv("ZUSAGE_DEBUG")) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "DEBUG: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
  }
}

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  if (!ptr || !stream) {
    print_debug("write_data: Invalid input parameters.");
    return 0; // Gracefully handle invalid input
  }
  return fwrite(ptr, size, nmemb, stream);
}

int is_ibm_domain(const char *hostname) {
  if (!hostname) {
    print_debug("is_ibm_domain: Null hostname provided.");
    return 0; // Null hostname is not an IBM domain
  }
  return strstr(hostname, "ibm.com") != NULL;
}

void get_fqdn(char *fqdn, size_t size) {
  if (!fqdn || size == 0) {
    print_debug("get_fqdn: Invalid input parameters.");
    return; // Avoid invalid input
  }

  char hostname[MAX_HOSTNAME_LENGTH];
  struct addrinfo hints, *res = NULL;
  int ret;

  if (gethostname(hostname, sizeof(hostname)) == 0) {
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    ret = getaddrinfo(hostname, NULL, &hints, &res);
    if (ret == 0 && res) {
      getnameinfo(res->ai_addr, res->ai_addrlen, fqdn, size, NULL, 0, 0);
      print_debug("get_fqdn: Resolved FQDN: %s", fqdn);
    } else {
      strncpy(fqdn, hostname, size - 1);
      print_debug("get_fqdn: Failed to resolve FQDN, using hostname: %s", fqdn);
    }
    fqdn[size - 1] = '\0';
  } else {
    strncpy(fqdn, "unknown", size - 1);
    fqdn[size - 1] = '\0';
    print_debug("get_fqdn: Failed to get hostname, using 'unknown'");
  }

  if (res) {
    freeaddrinfo(res);
  }
}

void get_system_info(char **os_release, char **cpu_arch) {
  if (!os_release || !cpu_arch) {
    print_debug("get_system_info: Invalid input parameters.");
    return; // Avoid null pointer dereferences
  }

  struct utsname uname_data;
  if (uname(&uname_data) == 0) {
    *os_release = strdup(uname_data.release);
    *cpu_arch = strdup(uname_data.machine);
    print_debug("get_system_info: OS Release: %s, CPU Arch: %s", *os_release, *cpu_arch);
  } else {
    *os_release = strdup("unknown");
    *cpu_arch = strdup("unknown");
    print_debug("get_system_info: Failed to get system info, using 'unknown'");
  }
}

void get_local_ip(char *local_ip, size_t size) {
  if (!local_ip || size == 0) {
    print_debug("get_local_ip: Invalid input parameters.");
    return;
  }

  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    strncpy(local_ip, "127.0.0.1", size - 1);
    local_ip[size - 1] = '\0';
    print_debug("get_local_ip: Failed to create socket, using '127.0.0.1'");
    return;
  }

  struct sockaddr_in serv = {0};
  serv.sin_family = AF_INET;
  serv.sin_port = htons(80);
  inet_pton(AF_INET, "8.8.8.8", &serv.sin_addr);

  if (connect(sock, (struct sockaddr *)&serv, sizeof(serv)) == 0) {
    struct sockaddr_in local = {0};
    socklen_t local_len = sizeof(local);
    if (getsockname(sock, (struct sockaddr *)&local, &local_len) == 0) {
      inet_ntop(AF_INET, &local.sin_addr, local_ip, size);
      print_debug("get_local_ip: Resolved local IP: %s", local_ip);
    } else {
      strncpy(local_ip, "127.0.0.1", size - 1);
      print_debug("get_local_ip: Failed to get local IP, using '127.0.0.1'");
    }
  } else {
    strncpy(local_ip, "127.0.0.1", size - 1);
    print_debug("get_local_ip: Failed to connect, using '127.0.0.1'");
  }

  local_ip[size - 1] = '\0';
  close(sock);
}

char *get_app_version() {
    char *app_version = malloc(MAX_APP_VERSION_LENGTH);
    if (!app_version) {
        print_debug("get_app_version: Memory allocation failed.");
        return strdup("unknown"); 
    }

    char *program_dir = __getprogramdir();
    if (!program_dir) {
        print_debug("get_app_version: Failed to get program directory, using 'unknown'");
        strncpy(app_version, "unknown", MAX_APP_VERSION_LENGTH - 1);
        app_version[MAX_APP_VERSION_LENGTH - 1] = '\0';
        return app_version; 
    }

    print_debug("get_app_version: program_dir: %s", program_dir);
    char version_file_path[PATH_MAX];
    int snprintf_result = snprintf(version_file_path, sizeof(version_file_path), "%s%s", program_dir, VERSION_FILE_RELATIVE_PATH);

    if (snprintf_result < 0 || snprintf_result >= sizeof(version_file_path)) {
        print_debug("get_app_version: snprintf failed or truncated the path, using 'unknown'");
        strncpy(app_version, "unknown", MAX_APP_VERSION_LENGTH - 1);
        app_version[MAX_APP_VERSION_LENGTH - 1] = '\0';
        return app_version;
    }

    FILE *version_file = fopen(version_file_path, "r");
    if (version_file) {
        if (fgets(app_version, MAX_APP_VERSION_LENGTH, version_file)) {
            size_t len = strlen(app_version);
            if (len > 0 && app_version[len - 1] == '\n') {
                app_version[len - 1] = '\0'; 
            }
            print_debug("get_app_version: Resolved app version: %s", app_version);
        } else {
            print_debug("get_app_version: Failed to read version file, using 'unknown'");
            strncpy(app_version, "unknown", MAX_APP_VERSION_LENGTH - 1);
            app_version[MAX_APP_VERSION_LENGTH - 1] = '\0';
        }
        fclose(version_file);
    } else {
        print_debug("get_app_version: Failed to open version file, using 'unknown'");
        strncpy(app_version, "unknown", MAX_APP_VERSION_LENGTH - 1);
        app_version[MAX_APP_VERSION_LENGTH - 1] = '\0';
    }

    return app_version;
}

#if 0 /* Deprecated curl code */
void *send_usage_data_thread(void *arg) {
  CURL *easy_handle;
  double duration;
  CURLcode res;

  // Measure start time
  START_TIMER;

  END_TIMER("1. Initial setup");
  char fqdn[MAX_HOSTNAME_LENGTH];
  get_fqdn(fqdn, sizeof(fqdn));

  END_TIMER("2. After get_fqdn");
  if (!is_ibm_domain(fqdn)) {
    fprintf(stderr, "Skipping usage collection for non-IBM domain: %s\n", fqdn);
    return NULL;
  }

  curl_global_init(CURL_GLOBAL_DEFAULT);
  easy_handle = curl_easy_init();

  if (easy_handle) {
    const char *app_name = getprogname();
    END_TIMER("3. After getprogname");

    char local_ip[MAX_IP_ADDRESS_LENGTH];
    get_local_ip(local_ip, sizeof(local_ip));
    END_TIMER("4. After get_local_ip");

    char *os_release = NULL;
    char *cpu_arch = NULL;
    get_system_info(&os_release, &cpu_arch);
    END_TIMER("5. After get_system_info");

    char *app_version = get_app_version();

    time_t now = time(NULL);
    struct tm *timeinfo = gmtime(&now);
    char timestamp[MAX_TIMESTAMP_LENGTH];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", timeinfo);
    END_TIMER("6. After timestamp");

    char *url = USAGE_ANALYTICS_URL;

    char post_data[MAX_POST_DATA_SIZE];
    snprintf(post_data, sizeof(post_data),
             "{\"app_name\": \"%s\", \"fqdn\": \"%s\", \"local_ip\": \"%s\", \"os_release\": \"%s\", \"cpu_arch\": \"%s\", \"app_version\": \"%s\", \"timestamp\": \"%s\"}",
             app_name, fqdn, local_ip, os_release, cpu_arch, app_version, timestamp);

    print_debug("Sending usage data:");
    print_debug("URL: %s", url);
    print_debug("POST data: %s", post_data);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(easy_handle, CURLOPT_HTTPHEADER, headers);
    END_TIMER("7. After curl setup");

    curl_easy_setopt(easy_handle, CURLOPT_URL, url);
    curl_easy_setopt(easy_handle, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(easy_handle, CURLOPT_POSTFIELDSIZE, (long)strlen(post_data));
    curl_easy_setopt(easy_handle, CURLOPT_NOSIGNAL, 1L);

    res = curl_easy_perform(easy_handle);

    if (res != CURLE_OK)
      print_debug("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

    // Calculate and print the duration
    END_TIMER("8. After curl_easy_perform");

    // Cleanup after the call, regardless of whether it has finished
    curl_easy_cleanup(easy_handle);

    free(os_release);
    free(cpu_arch);
    free(app_version);
  }

  curl_global_cleanup();
  return NULL;
}
#endif

int is_ibm_internal_ip(const char *ip_address) {
    if (!ip_address) {
        print_debug("is_ibm_internal_ip: Null IP address provided.");
        return 0;
    }

    struct in_addr addr;
    if (inet_pton(AF_INET, ip_address, &addr) != 1) {
        print_debug("is_ibm_internal_ip: Invalid IP address format: %s", ip_address);
        return 0;
    }

    uint32_t ip = ntohl(addr.s_addr);

    // Check 9.x.x.x range
    if ((ip >> 24) == 9) {
        return 1;
    }

    // Check 129.42.x.x range
    if ((ip >> 16) == (129 << 8 | 42)) {
        return 1;
    }

    return 0;
}


int is_ibm_domain_or_internal_ip(const char *hostname) {
    if (!hostname) {
        print_debug("is_ibm_domain_or_internal_ip: Null hostname provided.");
        return 0;
    }

    if (strstr(hostname, "ibm.com") != NULL) {
        return 1; // It's an IBM domain
    }

    struct addrinfo hints, *res, *rp;
    int sfd, s;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // We only care about IPv4 for this check
    hints.ai_socktype = SOCK_STREAM;

    s = getaddrinfo(hostname, NULL, &hints, &res);
    if (s != 0) {
        print_debug("getaddrinfo: %s", gai_strerror(s));
        return 0;
    }

    for (rp = res; rp != NULL; rp = rp->ai_next) {
        if (rp->ai_family == AF_INET) {
            char ip_address[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &((struct sockaddr_in *)rp->ai_addr)->sin_addr, ip_address, INET_ADDRSTRLEN);
            print_debug("Resolved IP for %s: %s", hostname, ip_address);
            if (is_ibm_internal_ip(ip_address)) {
                freeaddrinfo(res);
                return 1;
            }
        }
    }

    freeaddrinfo(res);
    return 0;
}

void *send_usage_data_thread() {
    double duration;

    START_TIMER;

    END_TIMER("1. Initial setup");

    char fqdn[MAX_HOSTNAME_LENGTH];
    get_fqdn(fqdn, sizeof(fqdn));

    END_TIMER("2. After get_fqdn");

    if (!is_ibm_domain_or_internal_ip(fqdn)) { 
        fprintf(stderr, "Skipping usage collection for non-IBM domain or non-internal IP: %s\n", fqdn);
        free(os_release);
        free(cpu_arch);
        free(app_version);
        return NULL;
    }

    const char *app_name = getprogname();
    END_TIMER("3. After getprogname");

    char local_ip[MAX_IP_ADDRESS_LENGTH];
    get_local_ip(local_ip, sizeof(local_ip));
    END_TIMER("4. After get_local_ip");

    char *os_release = NULL;
    char *cpu_arch = NULL;
    get_system_info(&os_release, &cpu_arch);
    END_TIMER("5. After get_system_info");

    char *app_version = get_app_version();

    time_t now = time(NULL);
    struct tm *timeinfo = gmtime(&now);
    char timestamp[MAX_TIMESTAMP_LENGTH];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", timeinfo);
    END_TIMER("6. After timestamp");

    const char *hostname = USAGE_ANALYTICS_URL;
    const int port = USAGE_ANALYTICS_PORT;
    const char *path = USAGE_ANALYTICS_PATH;

    // 1. Resolve hostname
    struct hostent *server = gethostbyname(hostname);
    if (server == NULL) {
        print_debug("ERROR, no such host");
        return NULL;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    CHECK_ERROR(sockfd, "ERROR opening socket");

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        print_debug("ERROR connecting");
        close(sockfd);
        return NULL;
    }

    char post_data[MAX_POST_DATA_SIZE];
    snprintf(post_data, sizeof(post_data),
             "{\"app_name\": \"%s\", \"fqdn\": \"%s\", \"local_ip\": \"%s\", \"os_release\": \"%s\", \"cpu_arch\": \"%s\", \"app_version\": \"%s\", \"timestamp\": \"%s\"}",
             app_name, fqdn, local_ip, os_release, cpu_arch, app_version, timestamp);

    char request[MAX_POST_DATA_SIZE * 2]; 
    snprintf(request, sizeof(request),
             "POST %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             path, hostname, strlen(post_data), post_data);

    ssize_t bytes_sent = send(sockfd, request, strlen(request), 0);

#if 0 /* To optimize, ignore the response */
    CHECK_ERROR(bytes_sent, "ERROR writing to socket");
    char buffer[256];
    ssize_t bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        print_debug("Server response:\n%s", buffer);
    } else if (bytes_received < 0) {
        perror("ERROR reading from socket");
    }
#endif

    close(sockfd);
    END_TIMER("8. After sending and receiving data");

    free(os_release);
    free(cpu_arch);
    free(app_version);

    return NULL;
}

__attribute__((constructor))
void usage_analytics_init() {
  int cvstate = __ae_autoconvert_state(_CVTSTATE_QUERY);
  if (_CVTSTATE_OFF == cvstate) {
    __ae_autoconvert_state(_CVTSTATE_ON);
  } 

  pthread_t thread_id;
  if (pthread_create(&thread_id, NULL, send_usage_data_thread, NULL) != 0) {
    fprintf(stderr, "Failed to create thread for usage analytics\n");
  } else {
    pthread_detach(thread_id);
  }
}

#ifdef ZUSAGE_TEST_MAIN
int main() {
  // Main program logic
  sleep(1);
  return 0;
}
#endif
