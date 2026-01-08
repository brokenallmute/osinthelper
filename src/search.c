#define _GNU_SOURCE
#include "search.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdatomic.h>

#define DATABASE_DIR "databases"
#define MAX_FILES 10000
#define MAX_RESULTS 100000

typedef struct {
    char *path;
    const char *basename;
    size_t size;
} file_entry_t;

// Глобальные данные модуля
static file_entry_t g_files[MAX_FILES];
static int g_file_count = 0;
static _Atomic int g_file_idx = 0;
static const char *g_query;
static size_t g_query_len;
static _Atomic int g_processed = 0;
static _Atomic int g_running = 1;
static search_results_t *g_results_ptr = NULL;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

static void add_result(const char *line_start, size_t line_len, const char *filename) {
    if (line_len > 4096) line_len = 4096;
    
    pthread_mutex_lock(&g_mutex);
    
    if (g_results_ptr->count >= g_results_ptr->capacity) {
        pthread_mutex_unlock(&g_mutex);
        return;
    }
    
    int idx = g_results_ptr->count++;
    
    g_results_ptr->items[idx].line = malloc(line_len + 1);
    if (g_results_ptr->items[idx].line) {
        memcpy(g_results_ptr->items[idx].line, line_start, line_len);
        g_results_ptr->items[idx].line[line_len] = '\0';
        g_results_ptr->items[idx].filename = filename;
    }
    
    pthread_mutex_unlock(&g_mutex);
}

static inline void extract_line(const char *data, size_t data_len, const char *match_pos,
                                const char **line_start, size_t *line_len) {
    const char *start = match_pos;
    const char *end = match_pos;
    const char *data_start = data;
    const char *data_end = data + data_len;
    
    while (start > data_start && start[-1] != '\n' && start[-1] != '\r') start--;
    while (end < data_end && *end != '\n' && *end != '\r') end++;
    
    *line_start = start;
    *line_len = end - start;
}

static void search_file(const char *filepath, const char *basename) {
    int fd = open(filepath, O_RDONLY);
    if (fd == -1) return;
    
    struct stat sb;
    if (fstat(fd, &sb) == -1 || sb.st_size == 0) {
        close(fd);
        return;
    }
    
    size_t size = sb.st_size;
    char *data = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    
    if (data == MAP_FAILED) return;
    
    madvise(data, size, MADV_SEQUENTIAL | MADV_WILLNEED);
    
    const char *p = data;
    const char *end = data + size;
    size_t remaining = size;
    
    while (remaining >= g_query_len) {
        const char *found = memmem(p, remaining, g_query, g_query_len);
        if (!found) break;
        
        const char *line_start;
        size_t line_len;
        extract_line(data, size, found, &line_start, &line_len);
        add_result(line_start, line_len, basename);
        
        p = found + 1;
        while (p < end && *p != '\n') p++;
        if (p < end) p++;
        remaining = end - p;
    }
    
    munmap(data, size);
}

static void* worker(void *arg) {
    (void)arg;
    
    while (1) {
        int idx = atomic_fetch_add(&g_file_idx, 1);
        if (idx >= g_file_count) break;
        
        search_file(g_files[idx].path, g_files[idx].basename);
        atomic_fetch_add(&g_processed, 1);
    }
    
    return NULL;
}

static void* progress_worker(void *arg) {
    (void)arg;
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    while (atomic_load(&g_running)) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double elapsed = (now.tv_sec - start.tv_sec) + (now.tv_nsec - start.tv_nsec) / 1e9;
        
        int done = atomic_load(&g_processed);
        int found = atomic_load(&g_results_ptr->count);
        
        printf("\r" COLOR_CYAN "[%d/%d]" COLOR_RESET " Найдено: " COLOR_GREEN "%d" COLOR_RESET 
               " | %.1fs   ", done, g_file_count, found, elapsed);
        fflush(stdout);
        
        usleep(50000);
    }
    return NULL;
}

static void collect_files(const char *dir) {
    DIR *d = opendir(dir);
    if (!d) return;
    
    struct dirent *e;
    while ((e = readdir(d)) && g_file_count < MAX_FILES) {
        if (e->d_name[0] == '.') continue;
        
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        
        struct stat st;
        if (stat(path, &st) == -1) continue;
        
        if (S_ISDIR(st.st_mode)) {
            collect_files(path);
        } else if (S_ISREG(st.st_mode) && st.st_size > 0) {
            char *ext = strrchr(e->d_name, '.');
            if (ext && (
                strcmp(ext, ".txt") == 0 ||
                strcmp(ext, ".csv") == 0 ||
                strcmp(ext, ".log") == 0 ||
                strcmp(ext, ".dat") == 0 ||
                strcmp(ext, ".json") == 0 ||
                strcmp(ext, ".sql") == 0 ||
                strcmp(ext, ".tsv") == 0 ||
                strcmp(ext, ".lst") == 0 ||
                strcmp(ext, ".conf") == 0 ||
                strcmp(ext, ".ini") == 0 ||
                strcmp(ext, ".dump") == 0 ||
                strcmp(ext, ".bak") == 0
            )) {
                g_files[g_file_count].path = strdup(path);
                g_files[g_file_count].basename = strrchr(g_files[g_file_count].path, '/') + 1;
                g_files[g_file_count].size = st.st_size;
                g_file_count++;
            }
        }
    }
    closedir(d);
}

static int cmp_files(const void *a, const void *b) {
    const file_entry_t *fa = a, *fb = b;
    return (fb->size > fa->size) - (fb->size < fa->size);
}

void search_init(void) {
    g_file_count = 0;
    g_file_idx = 0;
    g_processed = 0;
    collect_files(DATABASE_DIR);
    qsort(g_files, g_file_count, sizeof(file_entry_t), cmp_files);
}

void search_cleanup(void) {
    for (int i = 0; i < g_file_count; i++) {
        free(g_files[i].path);
    }
    g_file_count = 0;
}

int search_in_databases(const char *query, search_results_t *results) {
    if (!query || strlen(query) == 0) return -1;
    
    g_query = query;
    g_query_len = strlen(query);
    g_file_idx = 0;
    g_processed = 0;
    g_running = 1;
    g_results_ptr = results;
    
    results->items = calloc(MAX_RESULTS, sizeof(search_result_t));
    results->count = 0;
    results->capacity = MAX_RESULTS;
    
    if (g_file_count == 0) {
        printf(COLOR_RED "Файлов не найдено!\n" COLOR_RESET);
        return -1;
    }
    
    size_t total_size = 0;
    for (int i = 0; i < g_file_count; i++) {
        total_size += g_files[i].size;
    }
    
    int nthreads = sysconf(_SC_NPROCESSORS_ONLN);
    if (nthreads < 1) nthreads = 4;
    if (nthreads > g_file_count) nthreads = g_file_count;
    
    printf("\n" COLOR_BOLD "  Поиск: " COLOR_RESET "'%s'\n", query);
    printf(COLOR_DIM "  Файлов: %d |   Размер: %.2f GB |   Потоков: %d\n" COLOR_RESET,
           g_file_count, total_size / (1024.0 * 1024 * 1024), nthreads);
    printf("\n");
    
    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);
    
    pthread_t progress_tid;
    pthread_create(&progress_tid, NULL, progress_worker, NULL);
    
    pthread_t *threads = malloc(nthreads * sizeof(pthread_t));
    for (int i = 0; i < nthreads; i++) {
        pthread_create(&threads[i], NULL, worker, NULL);
    }
    
    for (int i = 0; i < nthreads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    atomic_store(&g_running, 0);
    pthread_join(progress_tid, NULL);
    
    clock_gettime(CLOCK_MONOTONIC, &t_end);
    double elapsed = (t_end.tv_sec - t_start.tv_sec) + 
                     (t_end.tv_nsec - t_start.tv_nsec) / 1e9;
    
    printf("\r\033[K");
    
    double speed = (total_size / (1024.0 * 1024)) / elapsed;
    printf(COLOR_GREEN "  Завершено за %.2f сек (%.0f MB/s)\n" COLOR_RESET, elapsed, speed);
    
    free(threads);
    
    return 0;
}

void search_list_databases(void) {
    if (g_file_count == 0) {
        printf(COLOR_RED "Файлов не найдено в '%s'\n" COLOR_RESET, DATABASE_DIR);
        return;
    }
    
    const int items_per_page = 10;  // Уменьшено до 10
    int total_pages = (g_file_count + items_per_page - 1) / items_per_page;
    int current_page = 0;
    
    while (1) {
        printf("\033[2J\033[H"); // Очистка экрана
        
        printf(COLOR_BOLD "╔════════════════════════════════════════════════════════════╗\n");
        printf("║               СПИСОК БАЗ ДАННЫХ (стр %d/%d)%*s║\n", 
               current_page + 1, total_pages, 
               17 - (current_page + 1 >= 10 ? 1 : 0) - (total_pages >= 10 ? 1 : 0), "");
        printf("╚════════════════════════════════════════════════════════════╝\n" COLOR_RESET);
        printf("\n");
        
        int start = current_page * items_per_page;
        int end = start + items_per_page;
        if (end > g_file_count) end = g_file_count;
        
        for (int i = start; i < end; i++) {
            double mb = g_files[i].size / 1048576.0;
            
            const char *color = COLOR_RESET;
            if (mb > 1000) color = COLOR_RED;
            else if (mb > 500) color = COLOR_YELLOW;
            else if (mb > 100) color = COLOR_CYAN;
            else color = COLOR_GREEN;
            
            printf(" " COLOR_DIM "[%2d]" COLOR_RESET " %-46s %s%8.1f MB" COLOR_RESET "\n", 
                   i + 1, g_files[i].basename, color, mb);
        }
        
        printf("\n");
        printf(COLOR_DIM "────────────────────────────────────────────────────────────\n" COLOR_RESET);
        
        double total_mb = 0;
        for (int i = 0; i < g_file_count; i++) {
            total_mb += g_files[i].size / 1048576.0;
        }
        
        printf(COLOR_BOLD "Всего: %d файлов | %.2f GB\n" COLOR_RESET, 
               g_file_count, total_mb / 1024.0);
        
        printf("\n");
        
        if (current_page > 0) {
            printf(COLOR_GREEN " [P]" COLOR_RESET " Назад   ");
        } else {
            printf("             ");
        }
        
        if (current_page < total_pages - 1) {
            printf(COLOR_GREEN "[N]" COLOR_RESET " Далее   ");
        } else {
            printf("            ");
        }
        
        printf(COLOR_YELLOW "[1-%d]" COLOR_RESET " Страница   ", total_pages);
        printf(COLOR_RED "[Q]" COLOR_RESET " Выход\n");
        
        printf("\n" COLOR_BOLD "Выбор: " COLOR_RESET);
        
        char input[10];
        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }
        
        input[strcspn(input, "\n")] = 0;
        
        if (strlen(input) == 0 || input[0] == 'q' || input[0] == 'Q') {
            break;
        }
        else if (input[0] == 'n' || input[0] == 'N') {
            if (current_page < total_pages - 1) current_page++;
        }
        else if (input[0] == 'p' || input[0] == 'P') {
            if (current_page > 0) current_page--;
        }
        else {
            int page_num = atoi(input);
            if (page_num >= 1 && page_num <= total_pages) {
                current_page = page_num - 1;
            }
        }
    }
}

size_t search_get_total_size(void) {
    size_t total = 0;
    for (int i = 0; i < g_file_count; i++) {
        total += g_files[i].size;
    }
    return total;
}

int search_get_file_count(void) {
    return g_file_count;
}

void search_free_results(search_results_t *results) {
    for (int i = 0; i < results->count; i++) {
        free(results->items[i].line);
    }
    free(results->items);
    results->items = NULL;
    results->count = 0;
}
