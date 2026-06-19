#define _GNU_SOURCE
#include "menu.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

static int getch(void) {
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}

int menu_choose_pin_length(void) {
    const char *options[] = {"4 Digit", "5 Digit", "6 Digit", "7 Digit", "8 Digit", "9 Digit", "10 Digit", "Custom PIN"};
    const int values[] = {4, 5, 6, 7, 8, 9, 10, 99};
    int n = 8;
    int sel = 2;
    int ch;

    printf("\n" BOLD "  Select PIN Length" RESET "\n");
    printf("  " CYAN "Use ↑/↓ arrows to navigate, Enter to select" RESET "\n\n");

    while (1) {
        printf("\033[%dA", n);
        for (int i = 0; i < n; i++) {
            if (i == sel)
                printf("  " CYAN "▶" RESET " %s " CYAN "◀" RESET "\n", options[i]);
            else
                printf("    %s\n", options[i]);
        }
        fflush(stdout);

        ch = getch();
        if (ch == 27) {
            ch = getch();
            if (ch == 91) {
                ch = getch();
                if (ch == 65) { sel = (sel - 1 + n) % n; }
                else if (ch == 66) { sel = (sel + 1) % n; }
            }
        } else if (ch == 10 || ch == 13) {
            break;
        }
    }

    printf("\n");
    if (values[sel] == 99) {
        printf("[*] Custom PIN: ");
        fflush(stdout);
        char buf[32];
        if (fgets(buf, sizeof(buf), stdin)) {
            int v = atoi(buf);
            if (v >= 1 && v <= 10) { printf("[+] Selected: %d-digit\n", v); return v; }
        }
        printf("[*] Invalid, defaulting to 6\n");
        return 6;
    }
    printf("[+] Selected: %s\n", options[sel]);
    return values[sel];
}
