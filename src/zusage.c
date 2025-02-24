#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/ps.h>
#include <arpa/inet.h>
#include <limits.h>
#include <stdarg.h>
#include <_Nascii.h>
#include <pwd.h>

// --- Macro Definitions ---

#define USAGE_ANALYTICS_URL "zusage1.fyre.ibm.com"
#define USAGE_ANALYTICS_PATH "/usage"
#define USAGE_ANALYTICS_PORT 3000
#define VERSION_FILE_RELATIVE_PATH "/../.version"
#define PATH_MAX 1024*4

#define MAX_HOSTNAME_LENGTH _POSIX_HOST_NAME_MAX
#define MAX_IP_ADDRESS_LENGTH INET_ADDRSTRLEN
#define MAX_TIMESTAMP_LENGTH 32
#define MAX_POST_DATA_SIZE 4096
#define MAX_APP_VERSION_LENGTH 100
#define MAX_OS_RELEASE_LENGTH 32
#define MAX_CPU_ARCH_LENGTH 16
#define MAX_DEBUG_BUFFER_SIZE 8192
#define MAX_USERNAME_LENGTH 64 
#define MAX_FQDN_LENGTH MAX_HOSTNAME_LENGTH // Assuming FQDN won't exceed hostname length

// --- Environment Variables ---
#define DISABLE_ENV_VAR "ZUSAGE_DISABLE"
#define DEBUG_ENV_VAR "ZUSAGE_DEBUG"

#define IBM_CHECK_CACHE_EXPIRY (14 * 24 * 3600) // 2 weeks in seconds
#define IBM_CHECK_CACHE_FILE_NAME "zusage_check.cache"

static int debug_fd = -1;

// --- Hostname Cache ---
static char cached_hostname[MAX_HOSTNAME_LENGTH] = "";
static int hostname_cached = 0;

// --- IBM Domain Check Cache ---
static time_t last_ibm_check_time = 0;
static int is_ibm_cached = -1; // -1: not checked, 0: not IBM, 1: IBM
static char ibm_check_cache_path[PATH_MAX] = ""; // Path to cache file

// --- FQDN Cache ---
static char cached_fqdn[MAX_FQDN_LENGTH] = "";
static int fqdn_cached = 0;

// --- Username Cache ---
static char cached_username_val[MAX_USERNAME_LENGTH] = "";
static int username_cached = 0;


char *generate_unique_filename() {
  static int sequence = 0;
  pid_t pid = getpid();
  struct timeval tv;
  gettimeofday(&tv, NULL);

  char *filename = malloc(256);
  if (!filename) {
    return NULL;
  }
  snprintf(filename, 256, "/tmp/zusagedebug-%d-%ld-%ld-%d.log", pid,
           (long)tv.tv_sec, (long)tv.tv_usec, ++sequence);
  return filename;
}

void print_debug(const char *format, ...) {
  if (getenv(DEBUG_ENV_VAR) == NULL) {
    return;
  }

  if (debug_fd == -1) {
    char *filename = generate_unique_filename();
    if (filename == NULL)
      return;
    debug_fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
    free(filename);
    if (debug_fd == -1) {
      return;
    }
  }

  va_list args;
  va_start(args, format);

  char timestamp[30];
  struct timeval tv;
  gettimeofday(&tv, NULL);
  struct tm *tm_info = localtime(&tv.tv_sec);
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

  char buffer[MAX_DEBUG_BUFFER_SIZE];
  int len = snprintf(buffer, sizeof(buffer), "%s.%06ld: ", timestamp, (long)tv.tv_usec);
  if (len < 0 || len >= sizeof(buffer)) {
    va_end(args);
    return;
  }

  int remaining_space = sizeof(buffer) - len;
  int msg_len = vsnprintf(buffer + len, remaining_space, format, args);
  va_end(args);

  if (msg_len < 0 || msg_len >= remaining_space) {
    return;
  }
  len += msg_len;

  if (len < sizeof(buffer) - 1) {
    buffer[len++] = '\n';
    buffer[len] = '\0';
  }
  write(debug_fd, buffer, len);
}

#ifdef ZUSAGE_TIMING
#define START_TIMER clock_t start_time = clock();
#define END_TIMER(label)                                                       \
  do {                                                                         \
    double duration = (double)(clock() - start_time) / CLOCKS_PER_SEC;         \
    print_debug("%s: %.6f seconds", label, duration);                          \
  } while (0)
#else
#define START_TIMER
#define END_TIMER(label)
#endif

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  if (!ptr || !stream) {
    print_debug("write_data: Invalid input parameters.");
    return 0;
  }
  return fwrite(ptr, size, nmemb, stream);
}

int is_ibm_domain(const char *hostname) {
  if (!hostname) {
    print_debug("is_ibm_domain: Null hostname");
    return 0;
  }
  return strstr(hostname, "ibm.com") != NULL;
}

void get_fqdn(char *fqdn, size_t size) {
    if (fqdn_cached) {
        strncpy(fqdn, cached_fqdn, size - 1);
        fqdn[size - 1] = '\0';
        print_debug("get_fqdn: Using cached FQDN: %s", fqdn);
        return;
    }

  if (!fqdn || size == 0) {
    print_debug("get_fqdn: Invalid input");
    return;
  }

  char hostname[MAX_HOSTNAME_LENGTH];
  if (gethostname(hostname, sizeof(hostname)) != 0) {
    print_debug("get_fqdn: gethostname failed");
    strncpy(fqdn, "unknown", size -1);
    fqdn[size - 1] = '\0';
    return;
  }
  hostname[sizeof(hostname) - 1] = '\0';

  struct addrinfo hints, *res = NULL;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_CANONNAME;

  int ret = getaddrinfo(hostname, NULL, &hints, &res);
  if (ret != 0) {
    print_debug("get_fqdn: getaddrinfo failed: %s", gai_strerror(ret));
    strncpy(fqdn, hostname, size - 1);
    fqdn[size - 1] = '\0';
    return;
  }

  if (res != NULL && res->ai_canonname != NULL) {
    strncpy(fqdn, res->ai_canonname, size - 1);
    fqdn[size - 1] = '\0';
    strncpy(cached_fqdn, fqdn, sizeof(cached_fqdn) - 1);
    cached_fqdn[sizeof(cached_fqdn) - 1] = '\0';
    fqdn_cached = 1;
    print_debug("get_fqdn: Resolved and cached FQDN: %s", fqdn);
  } else {
    print_debug("get_fqdn: Could not resolve FQDN, using hostname");
    strncpy(fqdn, hostname, size - 1);
    fqdn[size-1] = '\0';
  }
  freeaddrinfo(res);

  // Convert FQDN to lowercase
  for (int i = 0; fqdn[i]; i++) {
    fqdn[i] = tolower(fqdn[i]);
  }
}

// Function to get hostname and cache it
char* get_cached_hostname() {
    if (hostname_cached) {
        return cached_hostname;
    }

    if (gethostname(cached_hostname, sizeof(cached_hostname)) != 0) {
        print_debug("get_cached_hostname: gethostname failed");
        strncpy(cached_hostname, "unknown", sizeof(cached_hostname) - 1);
        cached_hostname[sizeof(cached_hostname) - 1] = '\0';
    } else {
        cached_hostname[sizeof(cached_hostname) - 1] = '\0';
        print_debug("get_cached_hostname: Hostname cached: %s", cached_hostname);
        hostname_cached = 1;
    }
    return cached_hostname;
}


void get_system_info(char **os_release, char **cpu_arch) {
  if (!os_release || !cpu_arch) {
    print_debug("get_system_info: Invalid input parameters.");
    return;
  }

  struct utsname uname_data;
  if (uname(&uname_data) == 0) {
    *os_release = strdup(uname_data.release);
    *cpu_arch = strdup(uname_data.machine);
    print_debug("get_system_info: OS Release: %s, CPU Arch: %s", *os_release, *cpu_arch);
  } else {
    print_debug("get_system_info: uname failed");
    *os_release = strdup("unknown");
    *cpu_arch = strdup("unknown");
  }

   if (!*os_release || !*cpu_arch) {
    print_debug("get_system_info: Memory allocation failed during strdup.");
    if (*os_release) {
      free(*os_release);
      *os_release = strdup("unknown");
      }
    if (*cpu_arch) {
      free(*cpu_arch);
      *cpu_arch = strdup("unknown");
    }
  }
}

void get_local_ip(char *local_ip, size_t size) {
  if (!local_ip || size == 0) {
    print_debug("get_local_ip: invalid args");
    return;
  }
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    print_debug("get_local_ip: socket creation failed");
    strncpy(local_ip, "127.0.0.1", size - 1);
    local_ip[size-1] = '\0';
    return;
  }

  struct sockaddr_in serv;
  memset(&serv, 0, sizeof(serv));
  serv.sin_family = AF_INET;
  serv.sin_addr.s_addr = inet_addr("8.8.8.8");
  serv.sin_port = htons(53);

  if (connect(sock, (struct sockaddr *)&serv, sizeof(serv)) < 0) {
    print_debug("get_local_ip: connect failed");
    strncpy(local_ip, "127.0.0.1", size - 1);
  } else {
    struct sockaddr_in name;
    socklen_t namelen = sizeof(name);
    if (getsockname(sock, (struct sockaddr *)&name, &namelen) < 0) {
      print_debug("get_local_ip: getsockname failed");
      strncpy(local_ip, "127.0.0.1", size - 1);
    } else {
      inet_ntop(AF_INET, &name.sin_addr, local_ip, size);
       print_debug("get_local_ip: Resolved local IP: %s", local_ip);
    }
  }
  local_ip[size - 1] = '\0';
  close(sock);
}

char* __tool_getprogramdir() {
  char argv[PATH_MAX];
  W_PSPROC buf;
  int token = 0;
  pid_t mypid = getpid();

  memset(&buf, 0, sizeof(buf));
  buf.ps_pathlen = PATH_MAX;
  buf.ps_pathptr = &argv[0];

  while ((token = w_getpsent(token, &buf, sizeof(buf))) > 0) {
    if (buf.ps_pid == mypid) {
      char* parent = realpath(argv, NULL);

      if (parent == NULL) {
        print_debug("__tool_getprogramdir: failed to resolve program directory");
        return NULL;
      }

      dirname(parent);
      return parent;
    }
  }

  print_debug("__tool_getprogramdir: Failed to get program directory");
  return NULL;
}

char* __tool_getprogname() {
  char argv[PATH_MAX];
  W_PSPROC buf;
  int token = 0;
  pid_t mypid = getpid();

  memset(&buf, 0, sizeof(buf));
  buf.ps_pathlen = PATH_MAX;
  buf.ps_pathptr = &argv[0];

  while ((token = w_getpsent(token, &buf, sizeof(buf))) > 0) {
    if (buf.ps_pid == mypid) {
        char* progname = strdup(basename(buf.ps_pathptr));
        if (!progname) {
          print_debug("__tool_getprogname: strdup failed");
          return NULL;
        }
      return progname;
    }
  }
    print_debug("__tool_getprogname: w_getpsent failed");
  return NULL;
}

char *get_app_version() {
  char *app_version = malloc(MAX_APP_VERSION_LENGTH);
  if (!app_version) {
    print_debug("get_app_version: Memory allocation failure");
    return strdup("unknown");
  }
  char *program_dir = __tool_getprogramdir();
  if (!program_dir)
  {
    print_debug("get_app_version: Failed to get program directory, returning unknown");
    return strdup("unknown");
  }

  print_debug("get_app_version: program_dir: %s", program_dir);

  char version_file_path[PATH_MAX];
  int snprintf_result = snprintf(version_file_path, sizeof(version_file_path), "%s%s", program_dir, VERSION_FILE_RELATIVE_PATH);

  if (snprintf_result < 0 || snprintf_result >= sizeof(version_file_path)) {
    print_debug("get_app_version: Version file path too long or snprintf error.");
    strncpy(app_version, "unknown", MAX_APP_VERSION_LENGTH - 1);
    app_version[MAX_APP_VERSION_LENGTH-1] = '\0';
    free(program_dir);
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
      print_debug("get_app_version: fgets failed");
      strncpy(app_version, "unknown", MAX_APP_VERSION_LENGTH - 1);
      app_version[MAX_APP_VERSION_LENGTH-1] = '\0';
    }
    fclose(version_file);
  } else {
    print_debug("get_app_version: Failed to open version file: %s", version_file_path);
    strncpy(app_version, "unknown", MAX_APP_VERSION_LENGTH - 1);
    app_version[MAX_APP_VERSION_LENGTH - 1] = '\0';
  }
  free(program_dir);
  return app_version;
}

int is_ibm_internal_ip(struct sockaddr_in *addr) {
  if (!addr)
  {
    print_debug("is_ibm_internal_ip: addr is NULL");
    return 0;
  }
  uint32_t ip = ntohl(addr->sin_addr.s_addr);

  if ((ip >> 24) == 9) {
    print_debug("is_ibm_internal_ip: ip is internal");
    return 1;
  }
  print_debug("is_ibm_internal_ip: ip is not internal");
  return 0;
}

int resolve_and_check_ibm() {
  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if (getaddrinfo(USAGE_ANALYTICS_URL, NULL, &hints, &res) != 0) {
    print_debug("resolve_and_check_ibm: getaddrinfo failed");
    return 0;
  }

  int is_internal = 0;
  for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
    if (p->ai_family == AF_INET) {
      struct sockaddr_in *addr = (struct sockaddr_in *)p->ai_addr;
      if (is_ibm_internal_ip(addr)) {
        is_internal = 1;
        break;
      }
    }
  }
  freeaddrinfo(res);
  return is_internal;
}

// Function to check if it's IBM domain and cache the result to file
int check_and_cache_ibm_domain() {
    time_t current_time = time(NULL);
    if (current_time == (time_t)-1) {
        print_debug("check_and_cache_ibm_domain: time() failed, cannot check cache expiry. Proceeding with check.");
        //Proceed to check even if time fails, rather than skipping usage data.
    } else if (is_ibm_cached != -1 && difftime(current_time, last_ibm_check_time) < IBM_CHECK_CACHE_EXPIRY) {
        print_debug("check_and_cache_ibm_domain: Using in-memory cached IBM domain check result: %d", is_ibm_cached);
        return is_ibm_cached; // Return cached result if not expired
    }

    // --- Check file cache ---
    if (ibm_check_cache_path[0] != '\0') { // If cache path is initialized
        FILE *cache_file = fopen(ibm_check_cache_path, "r");
        if (cache_file) {
            int cached_ibm_result;
            time_t cached_timestamp;
            if (fscanf(cache_file, "%d\n%ld", &cached_ibm_result, &cached_timestamp) == 2) {
                if (current_time == (time_t)-1 || difftime(current_time, cached_timestamp) < IBM_CHECK_CACHE_EXPIRY) {
                    fclose(cache_file);
                    is_ibm_cached = cached_ibm_result; // Update in-memory cache as well
                    print_debug("check_and_cache_ibm_domain: Using file cached IBM domain check result: %d", is_ibm_cached);
                    last_ibm_check_time = current_time; // keep updating last_ibm_check_time for in-memory cache expiry
                    return is_ibm_cached;
                } else {
                    print_debug("check_and_cache_ibm_domain: Cached IBM check result expired.");
                }
            } else {
                print_debug("check_and_cache_ibm_domain: Could not read data from cache file, invalid format.");
            }
            fclose(cache_file);
        } else {
            print_debug("check_and_cache_ibm_domain: Failed to open cache file for reading: %s", ibm_check_cache_path);
        }
    }


    char fqdn[MAX_HOSTNAME_LENGTH];
    get_fqdn(fqdn, sizeof(fqdn));
    int is_ibm = is_ibm_domain(fqdn) && resolve_and_check_ibm();

    is_ibm_cached = is_ibm ? 1 : 0;
    last_ibm_check_time = current_time;

    // --- Update file cache ---
    if (ibm_check_cache_path[0] != '\0') {
        FILE *cache_file = fopen(ibm_check_cache_path, "w");
        if (cache_file) {
            fprintf(cache_file, "%d\n%ld\n", is_ibm_cached, (long)current_time);
            fclose(cache_file);
            print_debug("check_and_cache_ibm_domain: Updated IBM domain cache file to: %d", is_ibm_cached);
        } else {
            print_debug("check_and_cache_ibm_domain: Failed to open cache file for writing: %s", ibm_check_cache_path);
        }
    } else {
        print_debug("check_and_cache_ibm_domain: Cache file path not initialized, cannot update cache file.");
    }


    print_debug("check_and_cache_ibm_domain: Updated IBM domain in-memory cache to: %d", is_ibm_cached);
    return is_ibm_cached;
}


char* get_username() {
    if (username_cached) {
        return cached_username_val;
    }

    char *username = malloc(MAX_USERNAME_LENGTH);
    if (!username) {
        print_debug("get_username: Memory allocation failed.");
        return strdup("unknown");
    }

    struct passwd *pwd;
    uid_t uid = getuid();
    pwd = getpwuid(uid);
    if (pwd != NULL) {
        strncpy(username, pwd->pw_name, MAX_USERNAME_LENGTH - 1);
        username[MAX_USERNAME_LENGTH - 1] = '\0';
        strncpy(cached_username_val, username, sizeof(cached_username_val) - 1);
        cached_username_val[sizeof(cached_username_val) - 1] = '\0';
        username_cached = 1;
        print_debug("get_username: Resolved and cached username: %s", username);
    } else {
        print_debug("get_username: getpwuid failed.");
        strncpy(username, "unknown", MAX_USERNAME_LENGTH - 1);
        username[MAX_USERNAME_LENGTH - 1] = '\0';
    }
    return username;
}


void *send_usage_data() {
  double duration;

  START_TIMER;

  END_TIMER("1. Initial setup");

  char fqdn[MAX_HOSTNAME_LENGTH];
  get_fqdn(fqdn, sizeof(fqdn)); // Will use cache if available

  END_TIMER("2. After get_fqdn");


  char *app_name = __tool_getprogname();
  if (!app_name) {
    app_name = strdup("unknown");
    if (!app_name) {
      print_debug("send_usage_data: Memory allocation failed");
      return NULL;
    }
  }
  END_TIMER("3. After getprogname");

  char local_ip[MAX_IP_ADDRESS_LENGTH];
  get_local_ip(local_ip, sizeof(local_ip));
  END_TIMER("4. After get_local_ip");

  char *os_release = NULL;
  char *cpu_arch = NULL;
  get_system_info(&os_release, &cpu_arch);
  if (!os_release || !cpu_arch) {
    print_debug("send_usage_data: Memory allocation error in get_system_info");
    free(app_name);
    return NULL;
  }
  END_TIMER("5. After get_system_info");

  char *app_version = get_app_version();
  if (!app_version) {
    print_debug("send_usage_data: app_version is NULL");
    app_version = strdup("unknown");
    if (!app_version) {
      print_debug("send_usage_data: memory alloc failed");
      free(os_release);
      free(cpu_arch);
      free(app_name);
      return NULL;
    }
  }

  END_TIMER("6. After get_app_version");

    char *username = get_username(); // Will use cache if available
    if (!username) {
        print_debug("send_usage_data: username is NULL");
        username = strdup("unknown"); // Fallback to "unknown" username
        if (!username) {
            print_debug("send_usage_data: memory alloc failed for username");
            free(os_release);
            free(cpu_arch);
            free(app_version);
            free(app_name);
            return NULL;
        }
    }
    END_TIMER("6.1. After get_username");


  const char *hostname = USAGE_ANALYTICS_URL;
  const int port = USAGE_ANALYTICS_PORT;
  const char *path = USAGE_ANALYTICS_PATH;

  struct hostent *server = gethostbyname(hostname);
  if (server == NULL) {
    print_debug("ERROR, no such host: %s", hostname);
    free(os_release);
    free(cpu_arch);
    free(app_version);
    free(app_name);
    free(username); // Free username as well
    return NULL;
  }
  END_TIMER("7. After gethostbyname");

  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    print_debug("ERROR opening socket");
    free(os_release);
    free(cpu_arch);
    free(app_version);
    free(app_name);
    free(username); // Free username as well
    return NULL;
  }

  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  memcpy(&serv_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);

  struct timeval connect_timeout;
  connect_timeout.tv_sec = 2;
  connect_timeout.tv_usec = 0;
  if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &connect_timeout, sizeof(connect_timeout)) < 0) {
    print_debug("ERROR setting connect timeout");
    close(sockfd);
    free(os_release);
    free(cpu_arch);
    free(app_version);
    free(app_name);
    free(username); // Free username as well
  }

  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    print_debug("ERROR connecting to %s:%d", hostname, port);
    close(sockfd);
    free(os_release);
    free(cpu_arch);
    free(app_version);
    free(app_name);
    free(username); // Free username as well
    return NULL;
  }

  END_TIMER("8. After connect");

  char post_data[MAX_POST_DATA_SIZE];
  int post_data_len = snprintf(post_data, sizeof(post_data),
           "{\"app_name\": \"%s\", \"fqdn\": \"%s\", \"local_ip\": \"%s\", \"os_release\": \"%s\", \"cpu_arch\": \"%s\", \"app_version\": \"%s\", \"username\": \"%s\"}",
           app_name, fqdn, local_ip, os_release, cpu_arch, app_version, username);

  if (post_data_len < 0 || post_data_len >= sizeof(post_data)) {
    print_debug("send_usage_data: post data creation failed");
    close(sockfd);
    free(os_release);
    free(cpu_arch);
    free(app_version);
    free(app_name);
    free(username); // Free username as well
    return NULL;
  }

  char request[MAX_POST_DATA_SIZE * 2];
  int request_len = snprintf(request, sizeof(request),
           "POST %s HTTP/1.1\r\n"
           "Host: %s\r\n"
           "Content-Type: application/json\r\n"
           "Content-Length: %d\r\n"
           "Connection: close\r\n"
           "\r\n"
           "%s",
           path, hostname, post_data_len, post_data);

  if (request_len < 0 || request_len >= sizeof(request)) {
    print_debug("send_usage_data: snprintf failed for request");
    close(sockfd);
    free(os_release);
    free(cpu_arch);
    free(app_version);
    free(app_name);
    free(username); // Free username as well
    return NULL;
  }

  ssize_t bytes_sent = send(sockfd, request, request_len, 0);
  if (bytes_sent < 0) {
    print_debug("ERROR writing to socket");
  } else if (bytes_sent != request_len) {
    print_debug("Incomplete data sent");
  }

#if 0
  char buffer[256];
  ssize_t bytes_received;
  do {
    bytes_received = recv(sockfd, buffer, sizeof(buffer) -1, 0);
    if (bytes_received > 0) {
      buffer[bytes_received] = 0;
      print_debug("recv: %s", buffer);
    }
  } while(bytes_received > 0);

  if (bytes_received < 0)
  {
    print_debug("send_usage_data: recv failed");
  }
#endif

  close(sockfd);
  END_TIMER("9. After sending and receiving data");

  free(os_release);
  free(cpu_arch);
  free(app_version);
  free(app_name);
    free(username); // Free username
  return NULL;
}

__attribute__((constructor))
void usage_analytics_init() {
  int cvstate = __ae_autoconvert_state(_CVTSTATE_QUERY);
  if (_CVTSTATE_OFF == cvstate) {
    __ae_autoconvert_state(_CVTSTATE_ON);
  }

  if (getenv(DISABLE_ENV_VAR) != NULL) {
    return;
  }

  // --- Initialize cache file path ---
  char *home_dir = getenv("HOME");
  if (home_dir == NULL) {
      print_debug("usage_analytics_init: HOME environment variable not set, cannot initialize cache file path. Using in-memory cache only.");
  } else {
      snprintf(ibm_check_cache_path, sizeof(ibm_check_cache_path), "%s/.cache/%s", home_dir, IBM_CHECK_CACHE_FILE_NAME);

      // Create cache directory if it doesn't exist
      char cache_dir[PATH_MAX];
      strncpy(cache_dir, ibm_check_cache_path, sizeof(cache_dir) - 1);
      cache_dir[sizeof(cache_dir) - 1] = '\0';
      char *dir_path = dirname(cache_dir); // modifies cache_dir
      struct stat st;
      if (stat(dir_path, &st) != 0) {
          if (mkdir(dir_path, 0700) == -1) {
              print_debug("usage_analytics_init: Failed to create cache directory: %s, errno: %d", dir_path, errno);
              ibm_check_cache_path[0] = '\0'; // Invalidate cache path
          } else {
              print_debug("usage_analytics_init: Created cache directory: %s", dir_path);
          }
      } else if (!S_ISDIR(st.st_mode)) {
          print_debug("usage_analytics_init: Cache directory path exists but is not a directory: %s", dir_path);
          ibm_check_cache_path[0] = '\0'; // Invalidate cache path
      }
  }


  // --- Check and cache IBM domain status ---
  if (!check_and_cache_ibm_domain()) {
      print_debug("Skipping usage collection: Not IBM domain or internal IP (cached check).");
      return; // Skip forking if not IBM domain after checking cache
  }

  pid_t pid = fork();

  if (pid == -1) {
    print_debug("Failed to fork process for usage analytics\n");
    return;
  }

  if (pid == 0) {
    int devnull = open("/dev/null", O_RDWR);
    if (devnull == -1) {
      print_debug("Failed to open /dev/null");
      exit(EXIT_FAILURE);
    }

    if (dup2(devnull, STDIN_FILENO) == -1) {
      print_debug("dup2(stdin) to /dev/null failed");
      close(devnull);
      exit(EXIT_FAILURE);
    }
    if (dup2(devnull, STDOUT_FILENO) == -1) {
      print_debug("dup2(stdout) to /dev/null failed");
      close(devnull);
      exit(EXIT_FAILURE);
    }
    if (dup2(devnull, STDERR_FILENO) == -1) {
      print_debug("dup2(stderr) to /dev/null failed");
      close(devnull);
      exit(EXIT_FAILURE);
    }

    close(devnull);
    send_usage_data();
    exit(EXIT_SUCCESS);
  } else {
    // Parent process
   // signal(SIGCHLD, SIG_IGN);
  }
}

#ifdef ZUSAGE_TEST_MAIN
int main(int argc, char **argv) {
  sleep(2);

  if (getenv(DEBUG_ENV_VAR) == NULL && debug_fd != -1)
  {
    print_debug("Main function completed. Check /tmp/zusagedebug-*.log for output.");
  }

  return 0;
}
#endif
