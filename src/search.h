#ifndef SEARCH_H
#define SEARCH_H

#include <stddef.h>

typedef struct {
    char *line;
    const char *filename;
} search_result_t;

typedef struct {
    search_result_t *items;
    int count;
    int capacity;
} search_results_t;

// Инициализация
void search_init(void);
void search_cleanup(void);

// Основные функции
int search_in_databases(const char *query, search_results_t *results);
void search_list_databases(void);
size_t search_get_total_size(void);
int search_get_file_count(void);

// Освобождение результатов
void search_free_results(search_results_t *results);

#endif
