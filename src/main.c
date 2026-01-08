#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ui.h"
#include "search.h"
#include "ipinfo.h"
#include "emailvalidator.h"
#include "tempmail.h"

static void handle_search(void) {
    clear_screen();
    show_logo();
    
    printf(COLOR_BOLD "╔════════════════════════════════════════════════╗\n");
    printf("║           ПОИСК В БАЗАХ ДАННЫХ                 ║\n");
    printf("╚════════════════════════════════════════════════╝\n" COLOR_RESET);
    printf("\n");
    
    char query[256];
    printf(COLOR_BOLD "Введите текст для поиска: " COLOR_RESET);
    
    if (!fgets(query, sizeof(query), stdin)) {
        return;
    }
    
    query[strcspn(query, "\n")] = 0;
    
    if (strlen(query) == 0) {
        printf(COLOR_RED "\n  Пустой запрос!\n" COLOR_RESET);
        press_enter();
        return;
    }
    
    search_results_t results;
    search_in_databases(query, &results);
    
    printf("\n");
    
    if (results.count == 0) {
        printf(COLOR_YELLOW "   Ничего не найдено\n" COLOR_RESET);
    } else {
        printf(COLOR_GREEN "  Найдено: %d результатов\n\n" COLOR_RESET, results.count);
        
        int show = results.count > 50 ? 50 : results.count;
        
        for (int i = 0; i < show; i++) {
            if (results.items[i].line) {
                printf(COLOR_DIM "[%s]" COLOR_RESET " %s\n", 
                       results.items[i].filename, results.items[i].line);
            }
        }
        
        if (results.count > 50) {
            printf(COLOR_DIM "\n... и ещё %d результатов\n" COLOR_RESET, results.count - 50);
        }
        
        search_free_results(&results);
    }
    
    press_enter();
}

static void handle_tempmail(void) {
    clear_screen();
    show_logo();
    
    printf(COLOR_BOLD "╔════════════════════════════════════════════════╗\n");
    printf("║           ВРЕМЕННАЯ ПОЧТА (TEMP MAIL)          ║\n");
    printf("╚════════════════════════════════════════════════╝\n" COLOR_RESET);
    printf("\n");
    
    printf(COLOR_CYAN "  Генерация временного email адреса..." COLOR_RESET "\n\n");
    
    char *email = tempmail_generate();
    if (!email) {
        printf(COLOR_RED "  Ошибка генерации email!\n" COLOR_RESET);
        press_enter();
        return;
    }
    
    printf(COLOR_GREEN "  Ваш временный email: " COLOR_BOLD "%s\n" COLOR_RESET, email);
    printf(COLOR_DIM   "  Используйте его для регистраций и проверок\n" COLOR_RESET);
    printf("\n");
    
    while (1) {
        printf(COLOR_DIM "Действия:\n" COLOR_RESET);
        printf(COLOR_DIM "  [1]" COLOR_RESET "   Проверить входящие письма\n");
        printf(COLOR_DIM "  [2]" COLOR_RESET "   Скопировать email в буфер\n");
        printf(COLOR_DIM "  [3]" COLOR_RESET "   Сгенерировать новый email\n");
        printf(COLOR_DIM "  [0]" COLOR_RESET "   Назад\n");
        printf("\n" COLOR_BOLD "Выбор: " COLOR_RESET);
        
        int choice;
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            continue;
        }
        while (getchar() != '\n');
        
        if (choice == 0) {
            free(email);
            break;
        }
        else if (choice == 1) {
            printf("\n" COLOR_CYAN "Проверка почты..." COLOR_RESET "\n");
            
            inbox_t inbox;
            if (tempmail_get_inbox(email, &inbox) == 0) {
                if (inbox.count == 0) {
                    printf(COLOR_YELLOW "\n Входящих писем нет\n" COLOR_RESET);
                } else {
                    printf(COLOR_GREEN "\n Найдено писем: %d\n\n" COLOR_RESET, inbox.count);
                    
                    for (int i = 0; i < inbox.count; i++) {
                        printf(COLOR_BOLD "[%d]" COLOR_RESET " От: %s\n", i + 1, inbox.messages[i].from);
                        printf("    Тема: %s\n", inbox.messages[i].subject);
                        printf("    Дата: %s\n", inbox.messages[i].date);
                        printf("\n");
                    }
                    
                    printf(COLOR_BOLD "Читать письмо [1-%d] или Enter для пропуска: " COLOR_RESET, inbox.count);
                    char input[10];
                    if (fgets(input, sizeof(input), stdin)) {
                        int msg_num = atoi(input);
                        if (msg_num >= 1 && msg_num <= inbox.count) {
                            char *body = NULL;
                            if (tempmail_read_message(email, inbox.messages[msg_num - 1].id, &body) == 0) {
                                printf("\n" COLOR_BOLD "╔══════════════════ СОДЕРЖИМОЕ ПИСЬМА ══════════════════╗\n" COLOR_RESET);
                                printf("%s\n", body);
                                printf(COLOR_BOLD "╚════════════════════════════════════════════════════════╝\n" COLOR_RESET);
                                free(body);
                            }
                        }
                    }
                }
                tempmail_free_inbox(&inbox);
            } else {
                printf(COLOR_RED "\n  Ошибка получения писем\n" COLOR_RESET);
            }
            
            printf("\n");
            press_enter();
        }
        else if (choice == 2) {
            printf(COLOR_GREEN "\n  Email: %s\n" COLOR_RESET, email);
            printf(COLOR_DIM "   (Скопируйте вручную)\n" COLOR_RESET);
            press_enter();
        }
        else if (choice == 3) {
            free(email);
            email = tempmail_generate();
            if (email) {
                printf(COLOR_GREEN "\n  Новый email: " COLOR_BOLD "%s\n" COLOR_RESET, email);
            } else {
                printf(COLOR_RED "\n  Ошибка генерации\n" COLOR_RESET);
                break;
            }
            printf("\n");
            press_enter();
        }
        
        clear_screen();
        show_logo();
        printf(COLOR_BOLD "╔════════════════════════════════════════════════╗\n");
        printf("║           ВРЕМЕННАЯ ПОЧТА (TEMP MAIL)        ║\n");
        printf("╚════════════════════════════════════════════════╝\n" COLOR_RESET);
        printf("\n");
        printf(COLOR_GREEN "  Ваш email: " COLOR_BOLD "%s\n\n" COLOR_RESET, email);
    }
}

static void handle_list(void) {
    clear_screen();
    show_logo();
    search_list_databases();
    press_enter();
}

// НОВАЯ ФУНКЦИЯ
static void handle_ipinfo(void) {
    clear_screen();
    show_logo();
    
    printf(COLOR_BOLD "╔════════════════════════════════════════════════╗\n");
    printf("║              ПРОБИВ IP АДРЕСА                  ║\n");
    printf("╚════════════════════════════════════════════════╝\n" COLOR_RESET);
    printf("\n");
    
    char ip[256];
    printf(COLOR_BOLD "Введите IP адрес (или Enter для проверки своего): " COLOR_RESET);
    
    if (!fgets(ip, sizeof(ip), stdin)) {
        return;
    }
    
    ip[strcspn(ip, "\n")] = 0;
    
    printf("\n" COLOR_CYAN "🔍 Запрос к IPinfo..." COLOR_RESET "\n");
    
    ip_info_t info;
    if (ipinfo_lookup(strlen(ip) > 0 ? ip : NULL, &info) == 0) {
        ipinfo_print(&info);
    } else {
        printf(COLOR_RED "\n  Ошибка получения данных!\n" COLOR_RESET);
    }
    
    press_enter();
}

static void handle_email_validation(void) {
    clear_screen();
    show_logo();
    
    printf(COLOR_BOLD "╔════════════════════════════════════════════════╗\n");
    printf("║            ПРОВЕРКА EMAIL АДРЕСА               ║\n");
    printf("╚════════════════════════════════════════════════╝\n" COLOR_RESET);
    printf("\n");
    
    char email[256];
    printf(COLOR_BOLD "Введите email адрес: " COLOR_RESET);
    
    if (!fgets(email, sizeof(email), stdin)) {
        return;
    }
    
    email[strcspn(email, "\n")] = 0;
    
    if (strlen(email) == 0) {
        printf(COLOR_RED "\n  Пустой email!\n" COLOR_RESET);
        press_enter();
        return;
    }
    
    printf("\n" COLOR_CYAN "  Проверка email адреса..." COLOR_RESET "\n");
    
    email_validation_t result;
    if (email_validate(email, &result) == 0) {
        email_print_result(&result);
    } else {
        printf(COLOR_RED "\n  Ошибка проверки email!\n" COLOR_RESET);
    }
    
    press_enter();
}

static void handle_settings(void) {
    // TODO
}

static void handle_stats(void) {
    clear_screen();
    show_logo();
    
    printf(COLOR_BOLD "  Статистика:\n" COLOR_RESET);
    printf("\n");
    printf("  Файлов в базе: " COLOR_CYAN "%d\n" COLOR_RESET, search_get_file_count());
    printf("  Общий размер:  " COLOR_CYAN "%.2f GB\n" COLOR_RESET, 
           search_get_total_size() / (1024.0 * 1024 * 1024));
    printf("  CPU ядер:      " COLOR_CYAN "%ld\n" COLOR_RESET, sysconf(_SC_NPROCESSORS_ONLN));
    
    press_enter();
}

int main(void) {
    search_init();
    
    while (1) {
        clear_screen();
        show_logo();
        
        int choice = show_menu();
        
        switch (choice) {
            case 1:
                handle_search();
                break;
            case 2:
                handle_ipinfo();
                break;
            case 3:
                handle_email_validation();
                break;
            case 4:
                handle_tempmail();
                break;
            case 5:
                handle_list();
                break;
            case 6:
                handle_stats();
                break;
            case 0:
                clear_screen();
                printf(COLOR_GREEN "До встречи!\n" COLOR_RESET);
                search_cleanup();
                return 0;
            default:
                printf(COLOR_RED "\n  Неверный выбор!\n" COLOR_RESET);
                press_enter();
        }
    }
    
    return 0;
}
