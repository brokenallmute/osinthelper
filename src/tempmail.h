#ifndef TEMPMAIL_H
#define TEMPMAIL_H

typedef struct {
    int id;
    char from[256];
    char subject[512];
    char date[64];
} email_message_t;

typedef struct {
    email_message_t *messages;
    int count;
} inbox_t;

// Генерация временного email
char* tempmail_generate(void);

// Получить список писем
int tempmail_get_inbox(const char *email, inbox_t *inbox);

// Прочитать письмо
int tempmail_read_message(const char *email, int message_id, char **body);

// Освобождение памяти
void tempmail_free_inbox(inbox_t *inbox);

#endif
