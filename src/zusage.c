#include "usage_analytics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/stat.h>

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  return size * nmemb;
}

int is_ibm_domain(const char *hostname) {
  return strstr(hostname, "ibm.com") != NULL;
}

void get_local_ip_and_hostname(char **ip, char **hostname_out) {
  char hostname[256];
  struct hostent *host_entry;

  gethostname(hostname, sizeof(hostname));
  host_entry = gethostbyname(hostname);

  *ip = malloc(16);
  if (host_entry == NULL) {
    strcpy(*ip, "127.0.0.1");
  } else {
    strcpy(*ip, inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0])));
  }

  *hostname_out = malloc(strlen(hostname) + 1);
  strcpy(*hostname_out, hostname);
}

//TODO
char *get_os_info() {
  struct utsname uname_data;
  uname(&uname_data);
  char *os_info = malloc(strlen(uname_data.sysname) + strlen(uname_data.release) + strlen(uname_data.version) + 3);
  sprintf(os_info, "%s %s %s", uname_data.sysname, uname_data.release, uname_data.version);
  return os_info;
}

//TODO:
char *get_cpu_arch() {
  struct utsname uname_data;
  uname(&uname_data);
  char *cpu_arch = malloc(strlen(uname_data.machine) + 1);
  strcpy(cpu_arch, uname_data.machine);
  return cpu_arch;
}

//TODO
char *get_app_version(const char *argv0) {
    char *app_version = malloc(256);
    char *last_slash = strrchr(argv0, '/');

    if (last_slash == NULL) {
        strcpy(app_version, "unknown");
    } else {
      char *parent_dir = strndup(argv0, last_slash - argv0);
      char version_file_path[512];
      snprintf(version_file_path, sizeof(version_file_path), "%s/.version", parent_dir);

      // Check if the .version file exists in the parent directory
      struct stat buffer;
      if (stat(version_file_path, &buffer) == 0) {
        FILE *version_file = fopen(version_file_path, "r");
        if (version_file == NULL) {
            strcpy(app_version, "unknown");
        } else {
            if (fgets(app_version, 256, version_file) == NULL) {
                strcpy(app_version, "unknown");
            } else {
                // Remove trailing newline if present
                size_t len = strlen(app_version);
                if (len > 0 && app_version[len - 1] == '\n') {
                    app_version[len - 1] = '\0';
                }
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


void send_usage_data(int argc, char *argv[]) {
  CURL *curl;
  CURLcode res;

  char local_hostname[256];
  gethostname(local_hostname, sizeof(local_hostname));

  if (!is_ibm_domain(local_hostname)) {
    fprintf(stderr, "Skipping usage collection for non-IBM domain - %s\n", local_hostname);
    return;
  }

  curl_global_init(CURL_GLOBAL_DEFAULT);
  curl = curl_easy_init();

  if (curl) {
    // Send the statistics:
    // Program name, Arguments, Local IP Address, Hostname, OS, Arch, Program Version, Timestamp
    // TODO: userid, event type (currently startup), can also cover exits

    // 1. Collect Program Name and Arguments using getprogname()
    const char *program_name = getprogname();
    char args_str[1024] = "";
    for (int i = 1; i < argc; i++) {
      strcat(args_str, argv[i]);
      if (i < argc - 1) {
        strcat(args_str, " ");
      }
    }

    // 2. Get Local IP Address and Hostname
    char *local_ip;
    char *hostname;
    get_local_ip_and_hostname(&local_ip, &hostname);

    // 3. Get OS Info
    char *os_info = get_os_info();

    // 4. Get CPU Architecture
    char *cpu_arch = get_cpu_arch();

    // 5. Get Application Version
    char *app_version = get_app_version(argv[0]);

    // 6. Get Current Timestamp
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    //TODO: Write a server app to collect information
    char *url = "https://zopen.community/usage";

    char post_data[4096];
    sprintf(post_data, "{\"program_name\": \"%s\", \"arguments\": \"%s\", \"local_ip\": \"%s\", \"hostname\": \"%s\", \"os_info\": \"%s\", \"cpu_arch\": \"%s\", \"app_version\": \"%s\", \"timestamp\": \"%s\"}",
            program_name, args_str, local_ip, hostname, os_info, cpu_arch, app_version, timestamp);

    if (getenv("ZUSAGE_DEBUG") && strcmp(getenv("ZUSAGE_DEBUG"), "1") == 0) {
      fprintf(stderr, "DEBUG: Sending usage data:\n");
      fprintf(stderr, "DEBUG: URL: %s\n", url);
      fprintf(stderr, "DEBUG: POST data: %s\n", post_data);
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(post_data));

    // Send output to /dev/null to avoid printing the response
    FILE *devnull = fopen("/dev/null", "w");
    if (devnull) {
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, devnull);
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
      fclose(devnull);
    }

    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    //TODO: make this async
    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
      fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    }

    curl_easy_cleanup(curl);
    free(local_ip);
    free(hostname);
    free(os_info);
    free(cpu_arch);
    free(app_version);
  }

  curl_global_cleanup();
}

__attribute__((constructor))
void usage_analytics_init(int argc, char *argv[]) {
  send_usage_data(argc, argv);
}

int main() {}
