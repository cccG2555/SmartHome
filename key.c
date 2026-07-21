#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gpiod.h>

#define CHIPNAME "/dev/gpiochip0"
#define CONSUMER "Keypad"

#define ROWS 4
#define COLS 4

int rowPins[ROWS] = {2, 3, 4, 5};  // BCM 번호
int colPins[COLS] = {6, 7, 8, 9};  // BCM 번호

char hexaKeys[ROWS][COLS] = {
    { '0', '1', '2', '3' },
    { '4', '5', '6', '7' },
    { '8', '9', 'A', 'B' },
    { 'C', 'D', 'E', 'F' }
};

struct gpiod_line *rowLines[ROWS];
struct gpiod_line *colLines[COLS];
struct gpiod_chip *chip;

void init_keypad() {
    chip = gpiod_chip_open(CHIPNAME);
    if (!chip) {
        perror("Failed to open gpiochip");
        exit(1);
    }

    for (int i = 0; i < ROWS; i++) {
        rowLines[i] = gpiod_chip_get_line(chip, rowPins[i]);
        gpiod_line_request_output(rowLines[i], CONSUMER, 1); // 초기 HIGH
    }

    for (int i = 0; i < COLS; i++) {
        colLines[i] = gpiod_chip_get_line(chip, colPins[i]);
        gpiod_line_request_input_flags(colLines[i], CONSUMER, GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP);
    }
}

char scan_keypad() {
    for (int i = 0; i < ROWS; i++) {
        gpiod_line_set_value(rowLines[i], 0);  // 현재 행 LOW

        for (int j = 0; j < COLS; j++) {
            int val = gpiod_line_get_value(colLines[j]);
            if (val == 0) {
                gpiod_line_set_value(rowLines[i], 1);
                return hexaKeys[i][j];
            }
        }
        gpiod_line_set_value(rowLines[i], 1);  // 원상복귀 HIGH
    }
    return 0;
}

int main() {
    init_keypad();

    printf("Raspberry Pi Keypad 4x4 Started\n");

    char prev_key = 0;

    while (1) {
        char key = scan_keypad();

        if (key != 0 && key != prev_key) {
            printf("Key Pressed: %c\n", key);
            prev_key = key;
        } else if (key == 0) {
            prev_key = 0;
        }

        usleep(10000); // 10ms 주기 (적당한 스캔 속도)
    }

    gpiod_chip_close(chip);
    return 0;
}
