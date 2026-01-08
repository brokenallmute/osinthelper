#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void clear_screen(void) {
    printf("\033[2J\033[H");
    fflush(stdout);
}

void show_logo(void) {
    printf(COLOR_CYAN COLOR_BOLD);
    printf("  ___  ____  ___ _   _ _____   _   _      _                 \n");
    printf(" / _ \\/ ___||_ _| \\ | |_   _| | | | | ___| |_ __   ___ _ __ \n");
    printf("| | | \\___ \\ | ||  \\| | | |   | |_| |/ _ \\ | '_ \\ / _ \\ '__|\n");
    printf("| |_| |___) || || |\\  | | |   |  _  |  __/ | |_) |  __/ |   \n");
    printf(" \\___/|____/|___|_| \\_| |_|   |_| |_|\\___|_| .__/ \\___|_|   \n");
    printf("                                            |_|              \n");
    printf(COLOR_RESET);
    printf(COLOR_DIM "        Мощный инструмент для работы с базами данных\n" COLOR_RESET);
    printf("\n");
}

int show_menu(void) {
    printf(COLOR_BOLD "╔════════════════════════════════════════════════╗\n");
    printf("║              ГЛАВНОЕ МЕНЮ                      ║\n");
    printf("╚════════════════════════════════════════════════╝\n" COLOR_RESET);
    printf("\n");
    
    printf(COLOR_DIM"  [1]" COLOR_RESET " Поиск в базах данных\n");
    printf(COLOR_DIM"  [2]" COLOR_RESET " Пробив IP адреса\n");
    printf(COLOR_DIM"  [3]" COLOR_RESET " Проверка Email\n");
    printf(COLOR_DIM"  [4]" COLOR_RESET " Временная почта\n");  
    printf(COLOR_DIM"  [5]" COLOR_RESET " Список баз\n");
    printf(COLOR_DIM"  [6]" COLOR_RESET " Статистика\n");
    printf(COLOR_DIM"  [0]" COLOR_RESET " Выход\n");
    printf("\n");
    
    printf(COLOR_BOLD "Выберите действие: " COLOR_RESET);
    
    int choice;
    if (scanf("%d", &choice) != 1) {
        while (getchar() != '\n');
        return -1;
    }
    
    while (getchar() != '\n');
    
    return choice;
}

void press_enter(void) {
    printf("\n" COLOR_DIM "Нажмите Enter для продолжения..." COLOR_RESET);
    char c;
    while ((c = getchar()) != '\n' && c != EOF);
}

void show_loading(const char *text) {
    printf(COLOR_CYAN " %s" COLOR_RESET, text);
    fflush(stdout);
}
