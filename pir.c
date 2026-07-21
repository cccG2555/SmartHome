#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gpiod.h>

#define PIR_IN 22    // BCM 22번 (Physical Pin 15)
#define LED 23       // BCM 23번 (Physical Pin 16)

#define CHIPNAME "/dev/gpiochip0"
#define CONSUMER "PIR_LED"

int main() {
    struct gpiod_chip *chip;
    struct gpiod_line *pir_line, *led_line;
    int pir_state;

    // GPIO 칩 열기
    chip = gpiod_chip_open(CHIPNAME);
    if (!chip) {
        perror("Failed to open gpiochip");
        return 1;
    }

    // 라인 요청
    pir_line = gpiod_chip_get_line(chip, PIR_IN);
    led_line = gpiod_chip_get_line(chip, LED);

    if (!pir_line || !led_line) {
        perror("Failed to get lines");
        gpiod_chip_close(chip);
        return 1;
    }

    // 입출력 설정
    if (gpiod_line_request_input(pir_line, CONSUMER) < 0 ||
        gpiod_line_request_output(led_line, CONSUMER, 0) < 0) {
        perror("Failed to request lines");
        gpiod_chip_close(chip);
        return 1;
    }

    while (1) {
        pir_state = gpiod_line_get_value(pir_line);
        printf("PIR State: %d\n", pir_state);

        if (pir_state == 1) {
            gpiod_line_set_value(led_line, 1);
        } else {
            gpiod_line_set_value(led_line, 0);
        }

        usleep(500000);  // 0.5초마다 확인
    }

    gpiod_chip_close(chip);
    return 0;
}
