#include "jukebox.h"
#include "utils.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <openssl/sha.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/*

    Well here is a dev blog for the nerds that cared enough to inspect...how many files? 35-36? for implementation...First, sorry for having fuck ton of files and folders
    Second, This is where I said fuck youtube APIs and shit and resurrect the ShittyJukebox DB with the forbiden spell

    Web will use the same db with ShittyJukebox for making my life easier...
    and yes I am trying ffprobe and shit for new ShittyJukebox version this is merely a test 
    A cursed one but a test nonetheless...
*/

#define MAX_WORKERS 8
#define MAX_TRACKS 4096

static pid_t workers[MAX_WORKERS];  //Process array for handling the requests and handling songs
static int listener_fd = -1;

void jukebox_init(int listener) { listener_fd = listener; }

void jukebox_reap(void)
{
    for (size_t i = 0; i < MAX_WORKERS; i++) {
        if (workers[i] && waitpid(workers[i], NULL, WNOHANG) == workers[i]){ workers[i] = 0; }
    }
}

void jukebox_shutdown(void)
{
    for (size_t i = 0; i < MAX_WORKERS; i++) {
        if (!workers[i]){ continue; }
        
        kill(workers[i], SIGTERM);
        
        while (waitpid(workers[i], NULL, 0) < 0 && errno == EINTR) {/*Nothing to see here lol*/}
        workers[i] = 0;

    }
}


static void reply(TLSClient *client, int code, const char *reason, const char *type,
                  const char *body, int head)
{
    char header[512];
    
    int n = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
        "Cache-Control: private, no-store\r\nX-Content-Type-Options: nosniff\r\n"
        "Connection: close\r\n\r\n", code, reason, type, strlen(body));
    
    if (n > 0 && (size_t)n < sizeof(header) && tls_write_all(client, header, (size_t)n) == 0 && !head){
        tls_write_all(client, body, strlen(body));   
    }

}

static DIR *music_dir(void)
{
    char path[PATH_MAX];
    
    if (get_server_path(path, sizeof(path), "music") != OK) { errno = EINVAL; return(NULL); }
    int fd = open(path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    
    if (fd < 0){ return NULL; }
    DIR *dir = fdopendir(fd);

    if (!dir){ close(fd); }
    return(dir);
}

static int is_track(DIR *dir, const char *name)
{
    size_t n = strlen(name);
    
    struct stat info;
    
    return (name[0] != '.' && n > 4 && !strcasecmp(name + n - 4, ".mp3")
        && fstatat(dirfd(dir), name, &info, AT_SYMLINK_NOFOLLOW) == 0
        && S_ISREG(info.st_mode) && info.st_size > 0);  //Longest return I've fucking written like wtf?
}

static void track_id(const char *name, char id[65])
{
    unsigned char digest[SHA256_DIGEST_LENGTH];
    
    SHA256((const unsigned char *)name, strlen(name), digest);
    for (size_t i = 0; i < sizeof(digest); i++){ sprintf(id + i * 2, "%02x", digest[i]); }
}

static void json_string(FILE *out, const char *value)
{
    fputc('"', out);
    for (const unsigned char *p = (const unsigned char *)value; *p; p++) {
        if (*p == '"' || *p == '\\'){ fprintf(out, "\\%c", *p); }
        else if (*p < 32){ fprintf(out, "\\u%04x", *p); }
        else{ fputc(*p, out); }
    }

    fputc('"', out);
}

static void catalogue(TLSClient *client, int head)
{
    DIR *dir = music_dir();
    // An empty/unconfigured library is usable; inaccessible libraries are errors.
    if (!dir && errno != ENOENT) {
        reply(client, 503, "Service Unavailable", "text/plain", "You call this music bitch?", head);
        return;
    }
    char *body = NULL;

    size_t length = 0;
    
    FILE *out = open_memstream(&body, &length);
    
    if (!out) {
        if (dir){ closedir(dir); }
        reply(client, 503, "Service Unavailable", "text/plain", "Music library unavailable", head);
        return;
    }
    fputc('[', out);
    
    struct dirent *entry;
    size_t count = 0;
    
    while (dir && count < MAX_TRACKS && (entry = readdir(dir))) {
        if (!is_track(dir, entry->d_name)) continue;
        char id[65];
        track_id(entry->d_name, id);
        fprintf(out, "%s{\"url\":\"/jukebox/audio/%s\",\"title\":", count++ ? "," : "", id);
        json_string(out, entry->d_name);
        fputc('}', out);
    }
    
    fputc(']', out);
    
    if (dir){ closedir(dir); }

    int is_fucked= ferror(out);
    
    if (fclose(out) != 0){ is_fucked = 1; }
    if (is_fucked){ reply(client, 503, "Service Unavailable", "text/plain", "Music library bit the dust", head); }
    else{ reply(client, 200, "OK", "application/json; charset=utf-8", body, head); }

    free(body);
}

static int open_track(const char *id, struct stat *info)
{
    if (strlen(id) != 64 || strspn(id, "0123456789abcdef") != 64){ return(-1); }
    DIR *dir = music_dir();
    if (!dir){ return(-1); }
    int fd = -1;
    
    struct dirent *entry;
    
    while ((entry = readdir(dir))) {
        if (!is_track(dir, entry->d_name)) continue;
    
        char candidate[65];
    
        track_id(entry->d_name, candidate);
    
        if (strcmp(id, candidate)){ continue; }
    
        fd = openat(dirfd(dir), entry->d_name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
        
        if (fd >= 0 && (fstat(fd, info) < 0 || !S_ISREG(info->st_mode) || info->st_size <= 0)) {
            close(fd);
            fd = -1;
        }
        
        break;
    }
    
    closedir(dir);
    
    return(fd);
}

static int number(const char **p, uintmax_t *out)
{
    if (**p < '0' || **p > '9') return 0;
    *out = 0;
    do {
        unsigned int digit = (unsigned int)(**p - '0');
        if (*out > (UINTMAX_MAX - digit) / 10){ return(0); }
        *out = *out * 10 + digit;
        (*p)++;
    } while (**p >= '0' && **p <= '9');
    
    return(1);
}

static int byte_range(const char *raw, uintmax_t size, uintmax_t *start, uintmax_t *end)
{
    //Me (Documentation Neuro fixing/formatting Programming Neuro's drunk-ass formatting and it's fucking har)
    *start = 0;

    *end = size - 1;

    char value[128] = {0};

    int found = 0;
    
    const char *line = strstr(raw, "\r\n");

    while (line && line[2] && line[2] != '\r') {
    
        line += 2;
    
        const char *finish = strstr(line, "\r\n");
    
        if (!finish) break;
        // No validators are issued for mutable local files, so If-Range cannot match.
        
        if ((size_t)(finish - line) >= 9 && !strncasecmp(line, "If-Range:", 9)){ return(200); }
        
        if ((size_t)(finish - line) >= 6 && !strncasecmp(line, "Range:", 6)) {
            if (found++){ return(200); } 
            const char *v = line + 6;
            while (v < finish && (*v == ' ' || *v == '\t')) v++;
            while (finish > v && (finish[-1] == ' ' || finish[-1] == '\t')){ finish--; }
            if ((size_t)(finish - v) >= sizeof(value)){ return(200); }

            memcpy(value, v, (size_t)(finish - v));
        }

        line = strstr(line, "\r\n");
    }

    if (!found || strncmp(value, "bytes=", 6) || strchr(value, ',')){ return(200); } 
    
    const char *p = value + 6;
    uintmax_t first, last;
    
    if (*p == '-') {
        p++;
        if (!number(&p, &last) || *p){ return(200); } 
        if (!last){ return(416); }

        *start = last >= size ? 0 : size - last;
    } 
    
    else {
        if (!number(&p, &first) || *p++ != '-'){ return(200); }
        if (!*p) last = size - 1;
        
        else if (!number(&p, &last) || *p || last < first){ return(200); }
        
        if (first >= size){ return(416); }
        
        *start = first;
        *end = last >= size ? size - 1 : last;
    }
    return(206);
}

static void stream_track(TLSClient *client, int fd, int code, uintmax_t size,
                         uintmax_t start, uintmax_t end, int head)
{
    char range[128] = "", header[640];
    uintmax_t length = code == 416 ? 0 : end - start + 1;
    
    if (code == 206) snprintf(range, sizeof(range), "Content-Range: bytes %ju-%ju/%ju\r\n", start, end, size);
    if (code == 416) snprintf(range, sizeof(range), "Content-Range: bytes */%ju\r\n", size);
    
    int n = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\nContent-Type: audio/mpeg\r\nContent-Length: %ju\r\n"
        "Accept-Ranges: bytes\r\n%sCache-Control: private, no-store\r\n"
        "X-Content-Type-Options: nosniff\r\nConnection: close\r\n\r\n",
        code, code == 206 ? "Partial Content" : code == 416 ? "Range Not Satisfiable" : "OK", length, range);
    
    if (n < 0 || (size_t)n >= sizeof(header) || tls_write_all(client, header, (size_t)n) < 0 || head || !length){ return; }
    if (lseek(fd, (off_t)start, SEEK_SET) < 0){ return; } 
    
    char buffer[16384];
    
    while (length) {
        size_t chunk = length < sizeof(buffer) ? (size_t)length : sizeof(buffer);
        ssize_t received = read(fd, buffer, chunk);
        
        if (received < 0 && errno == EINTR){ continue; }
        if (received <= 0 || tls_write_all(client, buffer, (size_t)received) < 0){ break; }
        
        length -= (uintmax_t)received;
    }
}

int jukebox_handle(TLSClient *client, const char *method, const char *path, const char *raw)
{
    int head = !strcmp(method, "HEAD");
    
    if (strcmp(method, "GET") && !head){ return(0); }
    
    if (!strcmp(path, "/jukebox/songs")) {
        catalogue(client, head);
        return(1);
    }

    const char *prefix = "/jukebox/audio/";
    
    if (strncmp(path, prefix, strlen(prefix))){ return(0); }
    
    struct stat info;
    
    int fd = open_track(path + strlen(prefix), &info);
    
    if (fd < 0) {
        reply(client, 404, "Not Found", "text/plain", "Track not found", head);
        return(1);
    }
    
    uintmax_t start = 0, end = (uintmax_t)info.st_size - 1;
    // Range only applies to GET, never HEAD.
    
    int code = head ? 200 : byte_range(raw, (uintmax_t)info.st_size, &start, &end);
    
    if (head || code == 416) {
        stream_track(client, fd, code, (uintmax_t)info.st_size, start, end, head);
        close(fd);
        return(1);
    }

    jukebox_reap();
    
    size_t slot = 0;
    
    while (slot < MAX_WORKERS && workers[slot]) slot++;
    
    sigset_t blocked, previous;
    
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGTERM);
    sigaddset(&blocked, SIGINT);
    
    if (sigprocmask(SIG_BLOCK, &blocked, &previous) < 0) {
        close(fd);
        reply(client, 503, "Service Unavailable", "text/plain", "Could not start audio worker", 0);
        return(1);
    }
    pid_t pid = slot < MAX_WORKERS ? fork() : -1;
    
    if (pid < 0) {    
        close(fd);
        reply(client, 503, "Service Unavailable", "text/plain", "Audio workers busy. Try again shortly.", 0);
    } 
    
    else if (pid == 0) {
        // The worker owns this TLS connection. It never accesses the inherited DB.
        signal(SIGTERM, SIG_DFL);
        signal(SIGINT, SIG_DFL);
        sigprocmask(SIG_SETMASK, &previous, NULL);
        if (listener_fd >= 0) close(listener_fd);
        alarm(600); // Bound even a client that keeps accepting just a few bytes.
        stream_track(client, fd, code, (uintmax_t)info.st_size, start, end, 0);
        close(fd);
        tls_close(client);
        _exit(0);
    } 
    
    else {
        workers[slot] = pid;
        close(fd);
        // Parent must free its copy without sending TLS close_notify to the child’s peer.
        client->failed = 1;
    }
    
    sigprocmask(SIG_SETMASK, &previous, NULL);
    return(1);
}
