#include "emailvalidator.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#define NEVERBOUNCE_KEY "private_365d1c164f97ade2f0bb6cce137359d7"
#define NEVERBOUNCE_URL "https://api.neverbounce.com/v4/single/check?key=" NEVERBOUNCE_KEY "&email=%s"

typedef struct {
    char *data;
    size_t size;
} response_t;

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    response_t *resp = (response_t *)userp;
    
    char *ptr = realloc(resp->data, resp->size + realsize + 1);
    if (!ptr) return 0;
    
    resp->data = ptr;
    memcpy(&(resp->data[resp->size]), contents, realsize);
    resp->size += realsize;
    resp->data[resp->size] = 0;
    
    return realsize;
}

static char* extract_json_value(const char *json, const char *key) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\":", key);
    
    char *start = strstr(json, search);
    if (!start) return NULL;
    
    start += strlen(search);
    while (*start == ' ' || *start == '\t' || *start == '\n') start++;
    
    if (*start == '"') {
        start++;
        char *end = strchr(start, '"');
        if (!end) return NULL;
        
        size_t len = end - start;
        char *value = malloc(len + 1);
        memcpy(value, start, len);
        value[len] = 0;
        return value;
    }
    
    char *end = start;
    while (*end && *end != ',' && *end != '}' && *end != '\n' && *end != ' ') end++;
    
    size_t len = end - start;
    char *value = malloc(len + 1);
    memcpy(value, start, len);
    value[len] = 0;
    
    return value;
}

static bool parse_bool(const char *str) {
    if (!str) return false;
    return (strcmp(str, "true") == 0 || strcmp(str, "1") == 0);
}

int email_validate(const char *email, email_validation_t *result) {
    if (!email || strlen(email) == 0) {
        return -1;
    }
    
    memset(result, 0, sizeof(email_validation_t));
    strncpy(result->email, email, sizeof(result->email) - 1);
    
    CURL *curl;
    CURLcode res;
    response_t response = {NULL, 0};
    
    curl = curl_easy_init();
    if (!curl) {
        return -1;
    }
    
    char *encoded_email = curl_easy_escape(curl, email, strlen(email));
    if (!encoded_email) {
        curl_easy_cleanup(curl);
        return -1;
    }
    
    char url[512];
    snprintf(url, sizeof(url), NEVERBOUNCE_URL, encoded_email);
    curl_free(encoded_email);
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "OsintHelper/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        fprintf(stderr, "CURL ошибка: %s\n", curl_easy_strerror(res));
        free(response.data);
        return -1;
    }
    
    if (!response.data) {
        return -1;
    }
    
    // Парсинг JSON от NeverBounce
    char *val;
    
    // status: success или error
    if ((val = extract_json_value(response.data, "status"))) {
        if (strcmp(val, "success") != 0) {
            free(val);
            
            // Проверяем сообщение об ошибке
            if ((val = extract_json_value(response.data, "message"))) {
                strncpy(result->reason, val, sizeof(result->reason) - 1);
                free(val);
            }
            
            free(response.data);
            return -1;
        }
        free(val);
    }
    
    // result: valid, invalid, disposable, catchall, unknown
    if ((val = extract_json_value(response.data, "result"))) {
        if (strcmp(val, "valid") == 0) {
            result->valid = true;
            result->smtp_valid = true;
            result->mx_found = true;
            result->score = 95;
            strncpy(result->reason, "Email существует и принимает письма", sizeof(result->reason) - 1);
        } 
        else if (strcmp(val, "invalid") == 0) {
            result->valid = false;
            result->score = 10;
            strncpy(result->reason, "Email адрес не существует или неверен", sizeof(result->reason) - 1);
        }
        else if (strcmp(val, "disposable") == 0) {
            result->valid = false;
            result->disposable = true;
            result->score = 30;
            strncpy(result->reason, "Одноразовый email адрес (temporary/disposable)", sizeof(result->reason) - 1);
        }
        else if (strcmp(val, "catchall") == 0) {
            result->valid = true;
            result->smtp_valid = true;
            result->mx_found = true;
            result->score = 70;
            strncpy(result->reason, "Catch-all домен (принимает все письма, невозможно точно проверить)", sizeof(result->reason) - 1);
        }
        else if (strcmp(val, "unknown") == 0) {
            result->valid = false;
            result->score = 40;
            strncpy(result->reason, "Не удалось определить статус email", sizeof(result->reason) - 1);
        }
        free(val);
    }
    
    // suggested_correction
    if ((val = extract_json_value(response.data, "suggested_correction"))) {
        if (strlen(val) > 0 && strcmp(val, "null") != 0 && strcmp(val, "") != 0) {
            strncpy(result->suggestion, val, sizeof(result->suggestion) - 1);
        }
        free(val);
    }
    
    // flags - дополнительная информация
    char *flags_start = strstr(response.data, "\"flags\":");
    if (flags_start) {
        // has_dns_info
        if (strstr(flags_start, "\"has_dns_info\":true")) {
            result->mx_found = true;
        }
        
        // has_dns_mx
        if (strstr(flags_start, "\"has_dns_mx\":true")) {
            result->mx_found = true;
        }
        
        // free_email_host
        // Это Gmail, Yahoo и т.д.
        
        // role_account
        if (strstr(flags_start, "\"role_account\":true")) {
            result->role_account = true;
        }
    }
    
    free(response.data);
    return 0;
}

void email_print_result(const email_validation_t *result) {
    printf("\n");
    printf(COLOR_BOLD "╔════════════════════════════════════════════════╗\n");
    printf("║           РЕЗУЛЬТАТ ПРОВЕРКИ EMAIL             ║\n");
    printf("╚════════════════════════════════════════════════╝\n" COLOR_RESET);
    printf("\n");
    
    printf(COLOR_CYAN "  📧 Email:         " COLOR_RESET "%s\n", result->email);
    printf("\n");
    
    // Главный статус
    if (result->valid) {
        printf(COLOR_GREEN "    Статус:        СУЩЕСТВУЕТ\n" COLOR_RESET);
    } else {
        printf(COLOR_RED "    Статус:        НЕ СУЩЕСТВУЕТ\n" COLOR_RESET);
    }
    
    printf("\n");
    printf(COLOR_BOLD "Детали проверки:\n" COLOR_RESET);
    printf("\n");
    
    // MX записи
    if (result->mx_found) {
        printf(COLOR_GREEN "    DNS/MX записи:  Найдены\n" COLOR_RESET);
    } else {
        printf(COLOR_RED "    DNS/MX записи:  Не найдены\n" COLOR_RESET);
    }
    
    // SMTP проверка
    if (result->smtp_valid) {
        printf(COLOR_GREEN "    SMTP проверка:  Mailbox доступен\n" COLOR_RESET);
    } else {
        printf(COLOR_YELLOW "    SMTP проверка:  Не подтвержден\n" COLOR_RESET);
    }
    
    // Disposable email
    if (result->disposable) {
        printf(COLOR_RED "    Тип:            Одноразовый/временный email\n" COLOR_RESET);
    } else {
        printf(COLOR_GREEN "    Тип:            Постоянный email\n" COLOR_RESET);
    }
    
    // Role account
    if (result->role_account) {
        printf(COLOR_YELLOW "    Аккаунт:        Ролевой (info@, admin@, support@)\n" COLOR_RESET);
    } else {
        printf(COLOR_GREEN "    Аккаунт:        Персональный\n" COLOR_RESET);
    }
    
    // Рейтинг качества
    if (result->score > 0) {
        printf("\n");
        printf(COLOR_CYAN "    Качество:      " COLOR_RESET);
        
        if (result->score >= 90) {
            printf(COLOR_GREEN "%d/100 (Отлично - Реальный email)\n" COLOR_RESET, result->score);
        } else if (result->score >= 70) {
            printf(COLOR_YELLOW "%d/100 (Хорошо - Вероятно работает)\n" COLOR_RESET, result->score);
        } else if (result->score >= 40) {
            printf(COLOR_YELLOW "%d/100 (Средне - Сомнительный)\n" COLOR_RESET, result->score);
        } else {
            printf(COLOR_RED "%d/100 (Плохо - Не рекомендуется)\n" COLOR_RESET, result->score);
        }
    }
    
    // Предложение исправления
    if (result->suggestion[0]) {
        printf("\n");
        printf(COLOR_YELLOW "    Возможно вы имели в виду: " COLOR_BOLD "%s\n" COLOR_RESET, result->suggestion);
    }
    
    // Причина
    if (result->reason[0]) {
        printf("\n");
        printf(COLOR_DIM "  !  %s\n" COLOR_RESET, result->reason);
    }
    
    printf("\n");
    printf(COLOR_DIM "  Проверено через: NeverBounce API\n" COLOR_RESET);
}
