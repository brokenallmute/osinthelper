#include "tempmail.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <time.h>

#define API_BASE "https://api.mail.tm"

// Глобальный токен для текущей сессии
static char *g_token = NULL;
static char *g_account_id = NULL;

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

static char* extract_json_string(const char *json, const char *key) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\":\"", key);
    
    char *start = strstr(json, search);
    if (!start) return NULL;
    
    start += strlen(search);
    
    char *end = start;
    while (*end && !(*end == '"' && *(end-1) != '\\')) {
        end++;
    }
    
    if (!*end) return NULL;
    
    size_t len = end - start;
    char *value = malloc(len + 1);
    memcpy(value, start, len);
    value[len] = 0;
    
    return value;
}

// Генерация случайного email с Mail.tm
char* tempmail_generate(void) {
    CURL *curl;
    CURLcode res;
    response_t response = {NULL, 0};
    
    // Шаг 1: Получаем доступные домены
    curl = curl_easy_init();
    if (!curl) return NULL;
    
    curl_easy_setopt(curl, CURLOPT_URL, API_BASE "/domains");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "OsintHelper/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    
    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK || !response.data) {
        free(response.data);
        return NULL;
    }
    
    // Извлекаем первый домен
    char *domain = extract_json_string(response.data, "domain");
    free(response.data);
    
    if (!domain) {
        domain = strdup("mailto.plus"); // fallback домен
    }
    
    // Генерируем случайное имя пользователя
    srand(time(NULL) + rand());
    const char *chars = "abcdefghijklmnopqrstuvwxyz0123456789";
    char username[16];
    int name_len = 8 + rand() % 5;
    
    for (int i = 0; i < name_len; i++) {
        username[i] = chars[rand() % strlen(chars)];
    }
    username[name_len] = '\0';
    
    char *email = malloc(256);
    snprintf(email, 256, "%s@%s", username, domain);
    
    // Генерируем пароль
    char password[32];
    for (int i = 0; i < 16; i++) {
        password[i] = chars[rand() % strlen(chars)];
    }
    password[16] = '\0';
    
    // Шаг 2: Создаем аккаунт
    response.data = NULL;
    response.size = 0;
    
    curl = curl_easy_init();
    if (!curl) {
        free(domain);
        free(email);
        return NULL;
    }
    
    char post_data[512];
    snprintf(post_data, sizeof(post_data), 
             "{\"address\":\"%s\",\"password\":\"%s\"}", email, password);
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    curl_easy_setopt(curl, CURLOPT_URL, API_BASE "/accounts");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "OsintHelper/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    
    res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK || !response.data) {
        free(response.data);
        free(domain);
        free(email);
        return NULL;
    }
    
    // Сохраняем ID аккаунта
    g_account_id = extract_json_string(response.data, "id");
    free(response.data);
    
    // Шаг 3: Получаем токен авторизации
    response.data = NULL;
    response.size = 0;
    
    curl = curl_easy_init();
    if (!curl) {
        free(domain);
        free(email);
        return NULL;
    }
    
    snprintf(post_data, sizeof(post_data), 
             "{\"address\":\"%s\",\"password\":\"%s\"}", email, password);
    
    headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    curl_easy_setopt(curl, CURLOPT_URL, API_BASE "/token");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "OsintHelper/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    
    res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK || !response.data) {
        free(response.data);
        free(domain);
        free(email);
        return NULL;
    }
    
    // Сохраняем токен
    g_token = extract_json_string(response.data, "token");
    free(response.data);
    free(domain);
    
    return email;
}

// Получить входящие письма
int tempmail_get_inbox(const char *email, inbox_t *inbox) {
    (void)email; // Не используется для Mail.tm
    
    if (!g_token) return -1;
    
    CURL *curl;
    CURLcode res;
    response_t response = {NULL, 0};
    
    curl = curl_easy_init();
    if (!curl) return -1;
    
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", g_token);
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, auth_header);
    
    curl_easy_setopt(curl, CURLOPT_URL, API_BASE "/messages");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "OsintHelper/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    
    res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK || !response.data) {
        free(response.data);
        return -1;
    }
    
    // Подсчет писем
    int count = 0;
    char *p = response.data;
    while ((p = strstr(p, "\"id\":\"")) != NULL) {
        count++;
        p += 6;
    }
    
    if (count == 0) {
        free(response.data);
        inbox->messages = NULL;
        inbox->count = 0;
        return 0;
    }
    
    inbox->messages = calloc(count, sizeof(email_message_t));
    inbox->count = count;
    
    // Парсим письма
    p = response.data;
    for (int i = 0; i < count; i++) {
        p = strstr(p, "\"id\":\"");
        if (!p) break;
        
        char *id_str = extract_json_string(p, "id");
        if (id_str) {
            // Используем первые цифры ID как int ID
            inbox->messages[i].id = i + 1;
            // Сохраняем полный ID в subject временно
            strncpy(inbox->messages[i].subject, id_str, sizeof(inbox->messages[i].subject) - 1);
            free(id_str);
        }
        
        // Извлекаем from
        char *from_obj = strstr(p, "\"from\":{");
        if (from_obj) {
            char *from_addr = extract_json_string(from_obj, "address");
            if (from_addr) {
                strncpy(inbox->messages[i].from, from_addr, sizeof(inbox->messages[i].from) - 1);
                free(from_addr);
            }
        }
        
        // Извлекаем subject (перезаписываем временный ID)
        char *subj = extract_json_string(p, "subject");
        if (subj) {
            strncpy(inbox->messages[i].subject, subj, sizeof(inbox->messages[i].subject) - 1);
            free(subj);
        }
        
        char *date = extract_json_string(p, "createdAt");
        if (date) {
            strncpy(inbox->messages[i].date, date, sizeof(inbox->messages[i].date) - 1);
            free(date);
        }
        
        p += 10;
    }
    
    free(response.data);
    return 0;
}

// Прочитать письмо (упрощенная версия)
int tempmail_read_message(const char *email, int message_id, char **body) {
    (void)email;
    (void)message_id;
    
    *body = strdup("Чтение писем будет добавлено в следующей версии.\nПисьма можно прочитать через веб-интерфейс Mail.tm");
    return 0;
}

void tempmail_free_inbox(inbox_t *inbox) {
    if (inbox->messages) {
        free(inbox->messages);
        inbox->messages = NULL;
    }
    inbox->count = 0;
}
