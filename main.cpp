#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

enum Mode {
    MODE_VIDEO = 0,
    MODE_AUDIO = 1,
};

struct Options {
    Mode mode;
    char* output_dir;
    char* url;
    int show_help;
};

struct ArgList {
    char** items;
    int count;
    int capacity;
};

static char* dup_cstr(const char* s) {
    if (s == NULL) {
        return NULL;
    }
    size_t n = strlen(s);
    char* copy = static_cast<char*>(malloc(n + 1));
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, s, n + 1);
    return copy;
}

static char* trim_copy(const char* s) {
    if (s == NULL) {
        return dup_cstr("");
    }

    const unsigned char* start = reinterpret_cast<const unsigned char*>(s);
    while (*start != '\0' && isspace(*start)) {
        ++start;
    }

    const unsigned char* end = start + strlen(reinterpret_cast<const char*>(start));
    while (end > start && isspace(*(end - 1))) {
        --end;
    }

    size_t len = static_cast<size_t>(end - start);
    char* out = static_cast<char*>(malloc(len + 1));
    if (out == NULL) {
        return NULL;
    }
    if (len > 0) {
        memcpy(out, start, len);
    }
    out[len] = '\0';
    return out;
}

static int is_http_url_str(const char* s) {
    if (s == NULL) {
        return 0;
    }

    for (const char* p = s; *p != '\0'; ++p) {
        if (isspace(static_cast<unsigned char>(*p))) {
            return 0;
        }
    }

    if (strncasecmp(s, "https://", 8) == 0) {
        return s[8] != '\0';
    }
    if (strncasecmp(s, "http://", 7) == 0) {
        return s[7] != '\0';
    }
    return 0;
}

static void print_help(const char* argv0) {
    const char* name = (argv0 != NULL && *argv0 != '\0') ? argv0 : "awesomeyt";
    printf("Usage:\n");
    printf("  %s\n", name);
    printf("  %s <url>\n", name);
    printf("  %s --audio <url>\n", name);
    printf("  %s --video <url>\n", name);
    printf("  %s --dir \"<folder>\" <url>\n", name);
    printf("  %s -h | --help\n", name);
    printf("\n");
    printf("Default mode: video\n");
}

static int arglist_init(struct ArgList* list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return 1;
}

static void arglist_free(struct ArgList* list) {
    if (list == NULL) {
        return;
    }
    for (int i = 0; i < list->count; ++i) {
        free(list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

static int arglist_push_copy(struct ArgList* list, const char* value) {
    if (list->count == list->capacity) {
        int new_capacity = (list->capacity == 0) ? 8 : list->capacity * 2;
        char** new_items = static_cast<char**>(realloc(list->items, sizeof(char*) * new_capacity));
        if (new_items == NULL) {
            return 0;
        }
        list->items = new_items;
        list->capacity = new_capacity;
    }

    char* copy = dup_cstr(value);
    if (copy == NULL) {
        return 0;
    }

    list->items[list->count++] = copy;
    return 1;
}

static int is_executable_file(const char* path) {
    if (path == NULL || *path == '\0') {
        return 0;
    }

    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    if (!S_ISREG(st.st_mode)) {
        return 0;
    }
    return access(path, X_OK) == 0;
}

static char* join_path(const char* dir, const char* name) {
    if (dir == NULL || *dir == '\0') {
        return dup_cstr(name);
    }

    size_t dir_len = strlen(dir);
    size_t name_len = strlen(name);

    int need_sep = (dir_len > 0 && dir[dir_len - 1] != '/');
    size_t total = dir_len + (need_sep ? 1 : 0) + name_len + 1;

    char* out = static_cast<char*>(malloc(total));
    if (out == NULL) {
        return NULL;
    }

    memcpy(out, dir, dir_len);
    size_t pos = dir_len;
    if (need_sep) {
        out[pos++] = '/';
    }
    memcpy(out + pos, name, name_len);
    out[pos + name_len] = '\0';
    return out;
}

static int path_already_seen(const char* const* seen, int seen_count, const char* dir) {
    for (int i = 0; i < seen_count; ++i) {
        if (strcmp(seen[i], dir) == 0) {
            return 1;
        }
    }
    return 0;
}

static int try_find_in_dir(const char* dir, const char* name, char** out_path) {
    char* candidate = join_path(dir, name);
    if (candidate == NULL) {
        return 0;
    }

    if (is_executable_file(candidate)) {
        *out_path = candidate;
        return 1;
    }

    free(candidate);
    return 0;
}

static int find_executable(const char* name, char** out_path) {
    *out_path = NULL;

    const char* preferred[] = {"/opt/homebrew/bin", "/usr/local/bin"};
    const int preferred_count = 2;

    const char* seen_dirs[512];
    int seen_count = 0;

    for (int i = 0; i < preferred_count; ++i) {
        if (!path_already_seen(seen_dirs, seen_count, preferred[i])) {
            if (seen_count < 512) {
                seen_dirs[seen_count++] = preferred[i];
            }
            if (try_find_in_dir(preferred[i], name, out_path)) {
                return 1;
            }
        }
    }

    const char* path_env = getenv("PATH");
    if (path_env == NULL || *path_env == '\0') {
        return 0;
    }

    char* path_copy = dup_cstr(path_env);
    if (path_copy == NULL) {
        return 0;
    }

    char* cursor = path_copy;
    while (1) {
        char* colon = strchr(cursor, ':');
        if (colon != NULL) {
            *colon = '\0';
        }

        const char* dir = cursor;
        if (dir == NULL) {
            dir = "";
        }

        if (!path_already_seen(seen_dirs, seen_count, dir)) {
            if (seen_count < 512) {
                seen_dirs[seen_count++] = dir;
            }
            if (try_find_in_dir(dir, name, out_path)) {
                free(path_copy);
                return 1;
            }
        }

        if (colon == NULL) {
            break;
        }
        cursor = colon + 1;
    }

    free(path_copy);
    return 0;
}

static char* expand_tilde_path(const char* raw_path) {
    if (raw_path == NULL) {
        return NULL;
    }

    if (raw_path[0] != '~') {
        return dup_cstr(raw_path);
    }

    const char* home = getenv("HOME");
    if (home == NULL || *home == '\0') {
        return NULL;
    }

    if (raw_path[1] == '\0') {
        return dup_cstr(home);
    }

    if (raw_path[1] == '/') {
        size_t home_len = strlen(home);
        size_t rest_len = strlen(raw_path + 1);
        char* out = static_cast<char*>(malloc(home_len + rest_len + 1));
        if (out == NULL) {
            return NULL;
        }
        memcpy(out, home, home_len);
        memcpy(out + home_len, raw_path + 1, rest_len + 1);
        return out;
    }

    return dup_cstr(raw_path);
}

static int ensure_directory_exists(const char* path) {
    if (path == NULL || *path == '\0') {
        errno = EINVAL;
        return 0;
    }

    char* tmp = dup_cstr(path);
    if (tmp == NULL) {
        return 0;
    }

    size_t len = strlen(tmp);
    while (len > 1 && tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
        --len;
    }

    for (char* p = tmp + 1; *p != '\0'; ++p) {
        if (*p != '/') {
            continue;
        }

        *p = '\0';
        if (strlen(tmp) > 0) {
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                free(tmp);
                return 0;
            }

            struct stat st;
            if (stat(tmp, &st) != 0 || !S_ISDIR(st.st_mode)) {
                errno = ENOTDIR;
                free(tmp);
                return 0;
            }
        }
        *p = '/';
    }

    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        free(tmp);
        return 0;
    }

    struct stat st;
    if (stat(tmp, &st) != 0 || !S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        free(tmp);
        return 0;
    }

    free(tmp);
    return 1;
}

static int append_to_buffer(char** data, size_t* size, size_t* capacity, const char* chunk, size_t chunk_len) {
    if (*size + chunk_len + 1 > *capacity) {
        size_t new_cap = (*capacity == 0) ? 1024 : *capacity;
        while (new_cap < *size + chunk_len + 1) {
            new_cap *= 2;
        }
        char* new_data = static_cast<char*>(realloc(*data, new_cap));
        if (new_data == NULL) {
            return 0;
        }
        *data = new_data;
        *capacity = new_cap;
    }

    memcpy(*data + *size, chunk, chunk_len);
    *size += chunk_len;
    (*data)[*size] = '\0';
    return 1;
}

static int read_command_stdout(const char* command, char** out_text) {
    *out_text = NULL;

    int fds[2];
    if (pipe(fds) != 0) {
        return 0;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return 0;
    }

    if (pid == 0) {
        close(fds[0]);
        if (dup2(fds[1], STDOUT_FILENO) == -1) {
            _exit(127);
        }
        close(fds[1]);

        char* argv_exec[2];
        argv_exec[0] = const_cast<char*>(command);
        argv_exec[1] = NULL;
        execvp(command, argv_exec);
        _exit(127);
    }

    close(fds[1]);

    char* data = NULL;
    size_t size = 0;
    size_t capacity = 0;
    char buffer[4096];

    while (1) {
        ssize_t n = read(fds[0], buffer, sizeof(buffer));
        if (n > 0) {
            if (!append_to_buffer(&data, &size, &capacity, buffer, static_cast<size_t>(n))) {
                free(data);
                close(fds[0]);
                int status = 0;
                waitpid(pid, &status, 0);
                return 0;
            }
            continue;
        }

        if (n == 0) {
            break;
        }

        if (errno == EINTR) {
            continue;
        }

        free(data);
        close(fds[0]);
        int status = 0;
        waitpid(pid, &status, 0);
        return 0;
    }

    close(fds[0]);

    int status = 0;
    while (waitpid(pid, &status, 0) == -1) {
        if (errno != EINTR) {
            free(data);
            return 0;
        }
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        free(data);
        return 0;
    }

    if (data == NULL) {
        data = dup_cstr("");
        if (data == NULL) {
            return 0;
        }
    }

    *out_text = data;
    return 1;
}

static int run_process(const char* executable, const struct ArgList* args) {
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Error: failed to fork process: %s\n", strerror(errno));
        return 1;
    }

    if (pid == 0) {
        char** argv_exec = static_cast<char**>(calloc(static_cast<size_t>(args->count) + 2, sizeof(char*)));
        if (argv_exec == NULL) {
            fprintf(stderr, "Error: failed to allocate process arguments.\n");
            _exit(127);
        }

        argv_exec[0] = const_cast<char*>(executable);
        for (int i = 0; i < args->count; ++i) {
            argv_exec[i + 1] = args->items[i];
        }
        argv_exec[args->count + 1] = NULL;

        execvp(executable, argv_exec);
        fprintf(stderr, "Error: failed to execute '%s': %s\n", executable, strerror(errno));
        _exit(127);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) == -1) {
        if (errno != EINTR) {
            fprintf(stderr, "Error: failed waiting for process: %s\n", strerror(errno));
            return 1;
        }
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return 1;
}

static void open_finder_async(const char* output_dir) {
    if (output_dir == NULL || output_dir[0] == '\0') {
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        return;
    }

    if (pid > 0) {
        int status = 0;
        waitpid(pid, &status, WNOHANG);
        return;
    }

    pid_t pid2 = fork();
    if (pid2 < 0) {
        _exit(0);
    }
    if (pid2 > 0) {
        _exit(0);
    }

    char* argv_exec[3];
    argv_exec[0] = const_cast<char*>("open");
    argv_exec[1] = const_cast<char*>(output_dir);
    argv_exec[2] = NULL;
    execvp("open", argv_exec);
    _exit(0);
}

static char* build_output_template(const char* output_dir) {
    const char* pattern = "%(title).200s [%(id)s].%(ext)s";
    size_t dir_len = strlen(output_dir);
    size_t pattern_len = strlen(pattern);
    int need_sep = (dir_len > 0 && output_dir[dir_len - 1] != '/');

    char* out = static_cast<char*>(malloc(dir_len + (need_sep ? 1 : 0) + pattern_len + 1));
    if (out == NULL) {
        return NULL;
    }

    memcpy(out, output_dir, dir_len);
    size_t pos = dir_len;
    if (need_sep) {
        out[pos++] = '/';
    }
    memcpy(out + pos, pattern, pattern_len + 1);
    return out;
}

static int parse_args(int argc, char** argv, struct Options* options) {
    options->mode = MODE_VIDEO;
    options->output_dir = dup_cstr("~/Downloads/AwesomeYT");
    options->url = NULL;
    options->show_help = 0;

    if (options->output_dir == NULL) {
        fprintf(stderr, "Error: out of memory.\n");
        return 0;
    }

    int url_count = 0;

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];

        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            options->show_help = 1;
            continue;
        }

        if (strcmp(arg, "--audio") == 0) {
            options->mode = MODE_AUDIO;
            continue;
        }

        if (strcmp(arg, "--video") == 0) {
            options->mode = MODE_VIDEO;
            continue;
        }

        if (strcmp(arg, "--dir") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --dir requires a folder path.\n");
                return 0;
            }

            char* trimmed_dir = trim_copy(argv[++i]);
            if (trimmed_dir == NULL) {
                fprintf(stderr, "Error: out of memory.\n");
                return 0;
            }

            free(options->output_dir);
            options->output_dir = trimmed_dir;
            continue;
        }

        if (arg[0] == '-') {
            fprintf(stderr, "Error: unknown option '%s'.\n", arg);
            return 0;
        }

        ++url_count;
        if (url_count > 1) {
            fprintf(stderr, "Error: multiple URLs provided. Pass only one URL.\n");
            return 0;
        }

        free(options->url);
        options->url = trim_copy(arg);
        if (options->url == NULL) {
            fprintf(stderr, "Error: out of memory.\n");
            return 0;
        }
    }

    return 1;
}

int main(int argc, char** argv) {
#ifndef __APPLE__
    fprintf(stderr, "Error: this tool is intended for macOS only.\n");
    return 1;
#endif

    struct Options options;
    memset(&options, 0, sizeof(options));

    if (!parse_args(argc, argv, &options)) {
        print_help((argc > 0) ? argv[0] : "awesomeyt");
        free(options.output_dir);
        free(options.url);
        return 1;
    }

    if (options.show_help) {
        print_help((argc > 0) ? argv[0] : "awesomeyt");
        free(options.output_dir);
        free(options.url);
        return 0;
    }

    if (options.output_dir == NULL || options.output_dir[0] == '\0') {
        fprintf(stderr, "Error: output directory cannot be empty.\n");
        free(options.output_dir);
        free(options.url);
        return 1;
    }

    char* url = NULL;
    if (options.url != NULL) {
        url = trim_copy(options.url);
        if (url == NULL) {
            fprintf(stderr, "Error: out of memory.\n");
            free(options.output_dir);
            free(options.url);
            return 1;
        }
    } else {
        char* clipboard = NULL;
        if (read_command_stdout("pbpaste", &clipboard)) {
            char* trimmed_clip = trim_copy(clipboard);
            free(clipboard);
            clipboard = NULL;

            if (trimmed_clip != NULL) {
                if (is_http_url_str(trimmed_clip)) {
                    url = trimmed_clip;
                    printf("Using URL from clipboard.\n");
                } else {
                    free(trimmed_clip);
                }
            }
        }

        if (url == NULL) {
            printf("Paste URL: ");
            fflush(stdout);

            char* line = NULL;
            size_t cap = 0;
            ssize_t nread = getline(&line, &cap, stdin);
            if (nread < 0) {
                free(line);
                line = NULL;
            }

            url = trim_copy((line != NULL) ? line : "");
            free(line);
            if (url == NULL) {
                fprintf(stderr, "Error: out of memory.\n");
                free(options.output_dir);
                free(options.url);
                return 1;
            }
        }
    }

    if (url[0] == '\0') {
        fprintf(stderr, "Error: URL is required and cannot be empty.\n");
        free(options.output_dir);
        free(options.url);
        free(url);
        return 1;
    }

    char* expanded_dir = expand_tilde_path(options.output_dir);
    if (expanded_dir == NULL || expanded_dir[0] == '\0') {
        fprintf(stderr, "Error: could not expand output directory '%s'.\n", options.output_dir);
        free(options.output_dir);
        free(options.url);
        free(url);
        free(expanded_dir);
        return 1;
    }

    if (!ensure_directory_exists(expanded_dir)) {
        fprintf(stderr, "Error: failed to create output directory '%s': %s\n",
                expanded_dir, strerror(errno));
        free(options.output_dir);
        free(options.url);
        free(url);
        free(expanded_dir);
        return 1;
    }

    char* yt_dlp_path = NULL;
    if (!find_executable("yt-dlp", &yt_dlp_path)) {
        fprintf(stderr, "Error: yt-dlp was not found.\n");
        fprintf(stderr, "Install it with Homebrew:\n");
        fprintf(stderr, "  brew install yt-dlp\n");
        free(options.output_dir);
        free(options.url);
        free(url);
        free(expanded_dir);
        return 1;
    }

    char* ffmpeg_path = NULL;
    int has_ffmpeg = find_executable("ffmpeg", &ffmpeg_path);
    if (options.mode == MODE_AUDIO && !has_ffmpeg) {
        fprintf(stderr, "Error: ffmpeg is required for audio mode but was not found.\n");
        fprintf(stderr, "Install it with Homebrew:\n");
        fprintf(stderr, "  brew install ffmpeg\n");
        free(options.output_dir);
        free(options.url);
        free(url);
        free(expanded_dir);
        free(yt_dlp_path);
        return 1;
    }

    char* output_template = build_output_template(expanded_dir);
    if (output_template == NULL) {
        fprintf(stderr, "Error: out of memory.\n");
        free(options.output_dir);
        free(options.url);
        free(url);
        free(expanded_dir);
        free(yt_dlp_path);
        free(ffmpeg_path);
        return 1;
    }

    struct ArgList yt_args;
    arglist_init(&yt_args);

    int ok = 1;
    ok = ok && arglist_push_copy(&yt_args, "--newline");
    ok = ok && arglist_push_copy(&yt_args, "--progress");
    ok = ok && arglist_push_copy(&yt_args, "--no-playlist");
    ok = ok && arglist_push_copy(&yt_args, "--restrict-filenames");
    ok = ok && arglist_push_copy(&yt_args, "-o");
    ok = ok && arglist_push_copy(&yt_args, output_template);

    if (has_ffmpeg) {
        ok = ok && arglist_push_copy(&yt_args, "--ffmpeg-location");
        ok = ok && arglist_push_copy(&yt_args, ffmpeg_path);
    }

    if (options.mode == MODE_AUDIO) {
        ok = ok && arglist_push_copy(&yt_args, "-x");
        ok = ok && arglist_push_copy(&yt_args, "--audio-format");
        ok = ok && arglist_push_copy(&yt_args, "mp3");
        ok = ok && arglist_push_copy(&yt_args, "--audio-quality");
        ok = ok && arglist_push_copy(&yt_args, "0");
    } else {
        ok = ok && arglist_push_copy(&yt_args, "-f");
        ok = ok && arglist_push_copy(&yt_args, "bv*+ba/b");
        ok = ok && arglist_push_copy(&yt_args, "--merge-output-format");
        ok = ok && arglist_push_copy(&yt_args, "mp4");
    }

    ok = ok && arglist_push_copy(&yt_args, url);

    if (!ok) {
        fprintf(stderr, "Error: out of memory while preparing arguments.\n");
        arglist_free(&yt_args);
        free(output_template);
        free(options.output_dir);
        free(options.url);
        free(url);
        free(expanded_dir);
        free(yt_dlp_path);
        free(ffmpeg_path);
        return 1;
    }

    printf("Only download content you own or have permission to download.\n");
    printf("Mode: %s\n", options.mode == MODE_AUDIO ? "audio" : "video");
    printf("Output directory: %s\n", expanded_dir);
    fflush(stdout);

    int exit_code = run_process(yt_dlp_path, &yt_args);
    if (exit_code == 0) {
        printf("Done.\n");
        printf("Download complete.\n");
        printf("To update awesomeyt later, run:\n");
        printf("  cd <project-dir> && ./deploy.sh\n");
        fflush(stdout);
        open_finder_async(expanded_dir);
    }

    arglist_free(&yt_args);
    free(output_template);
    free(options.output_dir);
    free(options.url);
    free(url);
    free(expanded_dir);
    free(yt_dlp_path);
    free(ffmpeg_path);

    return exit_code;
}
