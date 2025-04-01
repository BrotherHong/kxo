#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "game.h"

#define XO_STATUS_FILE "/sys/module/kxo/initstate"
#define XO_DEVICE_FILE "/dev/kxo"
#define XO_DEVICE_ATTR_FILE "/sys/class/kxo/kxo/kxo_state"

#define MEASURE_MODE 1
#if MEASURE_MODE
#define MEASURE_COUNT 1000
#define MEASURED_FILE "/tmp/measured_kernel.txt"
#endif

static bool status_check(void)
{
    FILE *fp = fopen(XO_STATUS_FILE, "r");
    if (!fp) {
        printf("kxo status : not loaded\n");
        return false;
    }

    char read_buf[20];
    fgets(read_buf, 20, fp);
    read_buf[strcspn(read_buf, "\n")] = 0;
    if (strcmp("live", read_buf)) {
        printf("kxo status : %s\n", read_buf);
        fclose(fp);
        return false;
    }
    fclose(fp);
    return true;
}

static struct termios orig_termios;

static void raw_mode_disable(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void raw_mode_enable(void)
{
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(raw_mode_disable);
    struct termios raw = orig_termios;
    raw.c_iflag &= ~IXON;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static bool read_attr, end_attr;

static void listen_keyboard_handler(void)
{
    int attr_fd = open(XO_DEVICE_ATTR_FILE, O_RDWR);
    char input;

    if (read(STDIN_FILENO, &input, 1) == 1) {
        char buf[20];
        switch (input) {
        case 16: /* Ctrl-P */
            read(attr_fd, buf, 6);
            buf[0] = (buf[0] - '0') ? '0' : '1';
            read_attr ^= 1;
            write(attr_fd, buf, 6);
            if (!read_attr)
                printf("Stopping to display the chess board...\n");
            break;
        case 17: /* Ctrl-Q */
            read(attr_fd, buf, 6);
            buf[4] = '1';
            read_attr = false;
            end_attr = true;
            write(attr_fd, buf, 6);
            printf("Stopping the kernel space tic-tac-toe game...\n");
            break;
        }
    }
    close(attr_fd);
}

#if MEASURE_MODE
long measured_time[MEASURE_COUNT];
#endif

int main(int argc, char *argv[])
{
    if (!status_check())
        exit(1);

    raw_mode_enable();
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    char display_buf[DRAWBUFFER_SIZE];

    fd_set readset;
    int device_fd = open(XO_DEVICE_FILE, O_RDONLY);
    int max_fd = device_fd > STDIN_FILENO ? device_fd : STDIN_FILENO;
    read_attr = true;
    end_attr = false;

#if MEASURE_MODE
    int measure_idx = 0;
    struct timespec start, end;
    printf("Measuring the time taken to read the display buffer...\n");
#endif

    while (!end_attr) {
        FD_ZERO(&readset);
        FD_SET(STDIN_FILENO, &readset);
        FD_SET(device_fd, &readset);

        int result = select(max_fd + 1, &readset, NULL, NULL, NULL);
        if (result < 0) {
            printf("Error with select system call\n");
            exit(1);
        }

        if (FD_ISSET(STDIN_FILENO, &readset)) {
            FD_CLR(STDIN_FILENO, &readset);
            listen_keyboard_handler();
        } else if (read_attr && FD_ISSET(device_fd, &readset)) {
            FD_CLR(device_fd, &readset);
            printf("\033[H\033[J"); /* ASCII escape code to clear the screen */
#if MEASURE_MODE
            int len;
            long elapsed_time = 0;

            clock_gettime(CLOCK_MONOTONIC, &start);
            len = read(device_fd, display_buf, DRAWBUFFER_SIZE);
            clock_gettime(CLOCK_MONOTONIC, &end);

            if (len > 0) {
                elapsed_time = (end.tv_sec - start.tv_sec) * 1000000 +
                               (end.tv_nsec - start.tv_nsec) / 1000;

                // only record the time that is greater than 0.1 ms
                if (elapsed_time >= 100)
                    measured_time[measure_idx++] = elapsed_time;

                if (measure_idx >= MEASURE_COUNT)
                    end_attr = true;
            }

            printf("%s\n", display_buf);
            printf("Current measured time: %ld us\n", elapsed_time);
            printf("Remaining measurements: %d\n", MEASURE_COUNT - measure_idx);
#else
            read(device_fd, display_buf, DRAWBUFFER_SIZE);
            printf("%s", display_buf);
#endif
        }
    }

#if MEASURE_MODE
    FILE *fp = fopen(MEASURED_FILE, "w");
    if (!fp) {
        printf("Error opening file %s\n", MEASURED_FILE);
        exit(1);
    }
    for (int i = 0; i < MEASURE_COUNT; i++) {
        fprintf(fp, "%ld\n", measured_time[i]);
    }
#endif

    raw_mode_disable();
    fcntl(STDIN_FILENO, F_SETFL, flags);

    close(device_fd);

    return 0;
}
