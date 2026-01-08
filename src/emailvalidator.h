#ifndef EMAILVALIDATOR_H
#define EMAILVALIDATOR_H

#include <stdbool.h>

typedef struct {
    char email[256];
    bool valid;
    bool disposable;
    bool role_account;
    bool mx_found;
    bool smtp_valid;
    char suggestion[256];
    char reason[512];
    int score;
} email_validation_t;

int email_validate(const char *email, email_validation_t *result);
void email_print_result(const email_validation_t *result);

#endif
