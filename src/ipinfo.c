#include "ipinfo.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#define IPINFO_TOKEN "8559f452d7522d"
#define IPINFO_URL "https://ipinfo.io/%s?token=" IPINFO_TOKEN

// Структура для накопления ответа
typedef struct {
    char *data;
    size_t size;
} response_t;

// Callback для curl
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

// Простой парсер JSON (без библиотек)
static char* extract_json_value(const char *json, const char *key) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\":", key);
    
    char *start = strstr(json, search);
    if (!start) return NULL;
    
    start += strlen(search);
    
    // Пропускаем пробелы
    while (*start == ' ' || *start == '\t') start++;
    
    // Если значение в кавычках
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
    
    // Если значение без кавычек (число, null и т.д.)
    char *end = start;
    while (*end && *end != ',' && *end != '}' && *end != '\n') end++;
    
    size_t len = end - start;
    char *value = malloc(len + 1);
    memcpy(value, start, len);
    value[len] = 0;
    
    // Убираем trailing пробелы
    for (int i = len - 1; i >= 0 && (value[i] == ' ' || value[i] == '\t'); i--) {
        value[i] = 0;
    }
    
    return value;
}

int ipinfo_lookup(const char *ip, ip_info_t *info) {
    CURL *curl;
    CURLcode res;
    response_t response = {NULL, 0};
    
    // Очищаем структуру
    memset(info, 0, sizeof(ip_info_t));
    
    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Ошибка инициализации CURL\n");
        return -1;
    }
    
    // Формируем URL
    char url[512];
    if (ip && strlen(ip) > 0) {
        snprintf(url, sizeof(url), IPINFO_URL, ip);
    } else {
        // Если IP не указан, проверяем свой
        snprintf(url, sizeof(url), "https://ipinfo.io/?token=" IPINFO_TOKEN);
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "OsintHelper/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        fprintf(stderr, "CURL ошибка: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        free(response.data);
        return -1;
    }
    
    curl_easy_cleanup(curl);
    
    if (!response.data) {
        return -1;
    }
    
    // Парсим JSON
    char *val;
    
    if ((val = extract_json_value(response.data, "ip"))) {
        strncpy(info->ip, val, sizeof(info->ip) - 1);
        free(val);
    }
    
    if ((val = extract_json_value(response.data, "hostname"))) {
        strncpy(info->hostname, val, sizeof(info->hostname) - 1);
        free(val);
    }
    
    if ((val = extract_json_value(response.data, "city"))) {
        strncpy(info->city, val, sizeof(info->city) - 1);
        free(val);
    }
    
    if ((val = extract_json_value(response.data, "region"))) {
        strncpy(info->region, val, sizeof(info->region) - 1);
        free(val);
    }
    
    if ((val = extract_json_value(response.data, "country"))) {
        strncpy(info->country, val, sizeof(info->country) - 1);
        free(val);
    }
    
    if ((val = extract_json_value(response.data, "loc"))) {
        strncpy(info->loc, val, sizeof(info->loc) - 1);
        free(val);
    }
    
    if ((val = extract_json_value(response.data, "org"))) {
        strncpy(info->org, val, sizeof(info->org) - 1);
        free(val);
    }
    
    if ((val = extract_json_value(response.data, "postal"))) {
        strncpy(info->postal, val, sizeof(info->postal) - 1);
        free(val);
    }
    
    if ((val = extract_json_value(response.data, "timezone"))) {
        strncpy(info->timezone, val, sizeof(info->timezone) - 1);
        free(val);
    }
    
    free(response.data);
    return 0;
}

void ipinfo_print(const ip_info_t *info) {
    printf("\n");
    printf(COLOR_BOLD "╔════════════════════════════════════════════════╗\n");
    printf("║           ИНФОРМАЦИЯ ОБ IP АДРЕСЕ            ║\n");
    printf("╚════════════════════════════════════════════════╝\n" COLOR_RESET);
    printf("\n");
    
    if (info->ip[0]) {
        printf(COLOR_CYAN "    IP адрес:      " COLOR_RESET "%s\n", info->ip);
    }
    
    if (info->hostname[0]) {
        printf(COLOR_CYAN "    Hostname:      " COLOR_RESET "%s\n", info->hostname);
    }
    
    if (info->country[0]) {
        printf(COLOR_CYAN "    Страна:        " COLOR_RESET "%s\n", info->country);
    }
    
    if (info->region[0]) {
        printf(COLOR_CYAN "    Регион:        " COLOR_RESET "%s\n", info->region);
    }
    
    if (info->city[0]) {
        printf(COLOR_CYAN "    Город:         " COLOR_RESET "%s\n", info->city);
    }
    
    if (info->postal[0]) {
        printf(COLOR_CYAN "    Индекс:        " COLOR_RESET "%s\n", info->postal);
    }
    
    if (info->loc[0]) {
        printf(COLOR_CYAN "     Координаты:    " COLOR_RESET "%s\n", info->loc);
        
        // Создаем ссылку на Google Maps
        printf(COLOR_DIM "     Google Maps:  https://www.google.com/maps?q=%s\n" COLOR_RESET, info->loc);
    }
    
    if (info->org[0]) {
        printf(COLOR_CYAN "    Провайдер:     " COLOR_RESET "%s\n", info->org);
    }
    
    if (info->timezone[0]) {
        printf(COLOR_CYAN "    Часовой пояс:  " COLOR_RESET "%s\n", info->timezone);
    }
}
