#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <getopt.h>
#include <err.h>

// --- Forward Declarations to Bypass Missing .h Files ---
// (The compiler will link these to asgn4_helper_funcs.a later)
typedef struct Listener_Socket Listener_Socket_t;
Listener_Socket_t *ls_new(int port);
int ls_accept(Listener_Socket_t *server);
void ls_delete(Listener_Socket_t **server);

typedef struct queue queue_t;
queue_t *queue_new(int size);
bool queue_push(queue_t *q, void *elem);
bool queue_pop(queue_t *q, void **elem);

typedef struct rwlock rwlock_t;
rwlock_t *rwlock_new(int priority, int n);
void reader_lock(rwlock_t *rw);
void reader_unlock(rwlock_t *rw);
void writer_lock(rwlock_t *rw);
void writer_unlock(rwlock_t *rw);

ssize_t write_n_bytes(int fd, const char *buf, size_t n);
ssize_t pass_n_bytes(int src_fd, int dst_fd, size_t n);

// --- Constants & Macros ---
#define BUFFER_SIZE 2048
#define DEFAULT_THREADS 4
#define NUM_URI_LOCKS 128

// --- Regex Patterns ---
#define RE_REQUEST "^([a-zA-Z]{1,8}) /([a-zA-Z0-9.-]{1,63}) (HTTP/[0-9]\\.[0-9])\r\n"
#define RE_HEADER "([a-zA-Z0-9.-]{1,128}): ([ -~]{1,128})\r\n"

// --- Globals for Concurrency ---
queue_t *job_queue;
rwlock_t *uri_locks[NUM_URI_LOCKS];

// --- Function Prototypes ---
void *worker_thread(void *arg);
int handle_get(int conn_fd, char *uri);
int handle_put(int conn_fd, char *uri, int content_length);
void send_response(int conn_fd, int status_code, char *status_phrase, int body_len, char *body);

// --- Helper: Hash URI to a specific lock ---
unsigned int hash_uri(const char *uri) {
    unsigned int hash = 5381;
    int c;
    while ((c = *uri++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % NUM_URI_LOCKS;
}

// --- Main Entry Point (Dispatcher Thread) ---
int main(int argc, char *argv[]) {
    int threads = DEFAULT_THREADS;
    int port = -1;
    int opt;

    // Parse command-line arguments: ./httpserver [-t threads] <port> [cite: 24]
    while ((opt = getopt(argc, argv, "t:")) != -1) {
        switch (opt) {
            case 't':
                threads = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s [-t threads] <port>\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Invalid Port\n");
        exit(EXIT_FAILURE);
    }
    
    port = atoi(argv[optind]);
    if (port < 1 || port > 65535) {
        fprintf(stderr, "Invalid Port\n");
        return 1;
    }

    // Initialize Thread-Safe Queue [cite: 56]
    job_queue = queue_new(threads);

    // Initialize Reader-Writer Locks 
    for (int i = 0; i < NUM_URI_LOCKS; i++) {
        uri_locks[i] = rwlock_new(1, 1); 
    }

    // Create exactly 'threads' Worker Threads [cite: 57]
    pthread_t *thread_pool = malloc(threads * sizeof(pthread_t));
    for (int i = 0; i < threads; i++) {
        if (pthread_create(&thread_pool[i], NULL, worker_thread, NULL) != 0) {
            err(EXIT_FAILURE, "Failed to create worker thread");
        }
    }

    // Initialize Listener Socket
    Listener_Socket_t *server = ls_new(port);
    if (server == NULL) {
        fprintf(stderr, "Invalid Port\n");
        return 1;
    }

    // Dispatcher Loop [cite: 63]
    while (1) {
        int conn_fd = ls_accept(server);
        if (conn_fd < 0) {
            continue; 
        }
        
        // Push the connection to the queue. Worker threads will wake up and pop it.
        queue_push(job_queue, (void *)(intptr_t)conn_fd);
    }

    ls_delete(&server);
    return 0;
}

// --- Worker Thread Logic ---
void *worker_thread(void *arg) {
    (void)arg;

    while (1) {
        // Pop connection off the queue (Blocks if empty)
        void *conn_fd_ptr;
        queue_pop(job_queue, &conn_fd_ptr);
        int conn_fd = (int)(intptr_t)conn_fd_ptr;

        char buf[BUFFER_SIZE + 1];
        ssize_t bytes_read = 0;
        int nbytes;

        // Variables for Audit Log
        char req_id[128] = "0"; // Default Request-Id [cite: 34]
        int status_code = 500;  // Default Status Code

        // Read byte-by-byte until \r\n\r\n
        memset(buf, 0, sizeof(buf));
        while (bytes_read < BUFFER_SIZE && strstr(buf, "\r\n\r\n") == NULL) {
            nbytes = read(conn_fd, buf + bytes_read, 1);
            if (nbytes <= 0) break;
            bytes_read += nbytes;
            buf[bytes_read] = '\0';
        }

        if (strstr(buf, "\r\n\r\n") == NULL) {
            send_response(conn_fd, 400, "Bad Request", 12, "Bad Request\n");
            fprintf(stderr, "Invalid,/%s,%d,%s\n", "unknown", 400, req_id);
            close(conn_fd);
            continue;
        }

        // Parse the Request Line
        regex_t regex;
        regmatch_t matches[4];
        if (regcomp(&regex, RE_REQUEST, REG_EXTENDED) != 0) {
            send_response(conn_fd, 500, "Internal Server Error", 22, "Internal Server Error\n");
            close(conn_fd);
            continue;
        }

        if (regexec(&regex, buf, 4, matches, 0) != 0) {
            regfree(&regex);
            send_response(conn_fd, 400, "Bad Request", 12, "Bad Request\n");
            fprintf(stderr, "Invalid,/%s,%d,%s\n", "unknown", 400, req_id);
            close(conn_fd);
            continue;
        }

        buf[matches[1].rm_eo] = '\0'; 
        buf[matches[2].rm_eo] = '\0'; 
        buf[matches[3].rm_eo] = '\0'; 

        char *method = buf + matches[1].rm_so;
        char *uri = buf + matches[2].rm_so;
        char *version = buf + matches[3].rm_so;

        if (strcmp(version, "HTTP/1.1") != 0) {
            regfree(&regex);
            send_response(conn_fd, 505, "Version Not Supported", 22, "Version Not Supported\n");
            fprintf(stderr, "%s,/%s,%d,%s\n", method, uri, 505, req_id);
            close(conn_fd);
            continue;
        }

        // Parse Headers (Content-Length & Request-Id)
        int content_length = 0;
        char *header_start = buf + matches[3].rm_eo + 2; 

        regex_t header_re;
        regmatch_t h_matches[3];
        regcomp(&header_re, RE_HEADER, REG_EXTENDED);

        char *cursor = header_start;
        while (regexec(&header_re, cursor, 3, h_matches, 0) == 0) {
            cursor[h_matches[1].rm_eo] = '\0'; 
            cursor[h_matches[2].rm_eo] = '\0'; 

            char *key = cursor + h_matches[1].rm_so;
            char *val = cursor + h_matches[2].rm_so;

            if (strcasecmp(key, "Content-Length") == 0) {
                content_length = atoi(val);
            } else if (strcasecmp(key, "Request-Id") == 0) {
                strncpy(req_id, val, sizeof(req_id) - 1);
            }

            cursor += h_matches[0].rm_eo;
        }
        regfree(&header_re);
        regfree(&regex);

        // Determine which lock to use based on the URI
        int lock_idx = hash_uri(uri);

        // Dispatch to Method Handler inside critical section
        if (strcmp(method, "GET") == 0) {
            reader_lock(uri_locks[lock_idx]);
            status_code = handle_get(conn_fd, uri);
            reader_unlock(uri_locks[lock_idx]);
        } else if (strcmp(method, "PUT") == 0) {
            writer_lock(uri_locks[lock_idx]);
            status_code = handle_put(conn_fd, uri, content_length);
            writer_unlock(uri_locks[lock_idx]);
        } else {
            send_response(conn_fd, 501, "Not Implemented", 16, "Not Implemented\n");
            status_code = 501;
        }

        // Output Atomic Audit Log to stderr [cite: 29]
        fprintf(stderr, "%s,/%s,%d,%s\n", method, uri, status_code, req_id); 

        // Close connection to prevent memory leaks [cite: 67]
        close(conn_fd);
    }
    return NULL;
}

// --- Modified Method Handlers (Now return status codes) ---

int handle_get(int conn_fd, char *uri) {
    int fd = open(uri, O_RDONLY);
    if (fd < 0) {
        if (errno == ENOENT) { 
            send_response(conn_fd, 404, "Not Found", 10, "Not Found\n");
            return 404;
        } else if (errno == EACCES) { 
            send_response(conn_fd, 403, "Forbidden", 10, "Forbidden\n");
            return 403;
        } else {
            send_response(conn_fd, 500, "Internal Server Error", 22, "Internal Server Error\n");
            return 500;
        }
    }

    struct stat st;
    fstat(fd, &st);
    if (S_ISDIR(st.st_mode)) {
        close(fd);
        send_response(conn_fd, 403, "Forbidden", 10, "Forbidden\n");
        return 403;
    }

    char msg[128];
    int len = snprintf(msg, sizeof(msg), "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n\r\n", st.st_size);
    write_n_bytes(conn_fd, msg, len);
    pass_n_bytes(fd, conn_fd, st.st_size);

    close(fd);
    return 200;
}

int handle_put(int conn_fd, char *uri, int content_length) {
    int exists = access(uri, F_OK) == 0;
    int fd = open(uri, O_WRONLY | O_CREAT | O_TRUNC, 0666);

    if (fd < 0) {
        if (errno == EACCES) {
            send_response(conn_fd, 403, "Forbidden", 10, "Forbidden\n");
            return 403;
        } else {
            send_response(conn_fd, 500, "Internal Server Error", 22, "Internal Server Error\n");
            return 500;
        }
    }

    pass_n_bytes(conn_fd, fd, content_length);
    close(fd);

    if (exists) {
        send_response(conn_fd, 200, "OK", 3, "OK\n");
        return 200;
    } else {
        send_response(conn_fd, 201, "Created", 8, "Created\n");
        return 201;
    }
}

void send_response(int conn_fd, int status_code, char *status_phrase, int body_len, char *body) {
    char msg[128];
    int len = snprintf(msg, sizeof(msg), "HTTP/1.1 %d %s\r\nContent-Length: %d\r\n\r\n",
                       status_code, status_phrase, body_len);
    write_n_bytes(conn_fd, msg, len);
    write_n_bytes(conn_fd, body, body_len);
}
