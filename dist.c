#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <gpiod.h>

// 사용할 GPIO 번호 (라즈베리파이5의 실제 BCM GPIO 번호로 수정!)
#define TRIG 17
#define ECHO 27

#define CHIPNAME "/dev/gpiochip0"
#define CONSUMER "HC-SR04"

// 마이크로초 단위로 sleep
void sleep_microseconds(long microseconds) {
    struct timespec ts;
    ts.tv_sec = microseconds / 1000000;
    ts.tv_nsec = (microseconds % 1000000) * 1000;
    nanosleep(&ts, NULL);
}

// 현재 시간 마이크로초 단위로 얻기
long get_microseconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

// pulseIn 구현 (HIGH 또는 LOW 될 때까지 시간 측정)
long pulseIn(struct gpiod_line *line, int value) {
    long start_time = get_microseconds();
    long timeout = 30000; // 30ms 타임아웃

    // 원하는 신호가 나올 때까지 대기
    while (gpiod_line_get_value(line) != value) {
        if (get_microseconds() - start_time > timeout) return 0;
    }

    long pulse_start = get_microseconds();

    // 신호가 바뀔 때까지 측정
    while (gpiod_line_get_value(line) == value) {
        if (get_microseconds() - pulse_start > timeout) return 0;
    }

    long pulse_end = get_microseconds();

    return pulse_end - pulse_start;
}

int main() {
    struct gpiod_chip *chip;
    struct gpiod_line *trig_line, *echo_line;

    chip = gpiod_chip_open(CHIPNAME);
    if (!chip) {
        perror("Failed to open gpiochip");
        return 1;
    }

    trig_line = gpiod_chip_get_line(chip, TRIG);
    echo_line = gpiod_chip_get_line(chip, ECHO);
    if (!trig_line || !echo_line) {
        perror("Failed to get lines");
        gpiod_chip_close(chip);
        return 1;
    }

    if (gpiod_line_request_output(trig_line, CONSUMER, 0) < 0 ||
        gpiod_line_request_input(echo_line, CONSUMER) < 0) {
        perror("Failed to request lines");
        gpiod_chip_close(chip);
        return 1;
    }

    while (1) {
        long duration, distance;

        // 초음파 발사
        gpiod_line_set_value(trig_line, 0);
        sleep_microseconds(2);
        gpiod_line_set_value(trig_line, 1);
        sleep_microseconds(10);
        gpiod_line_set_value(trig_line, 0);

        // 반사신호 측정
        duration = pulseIn(echo_line, 1);

        // 거리 계산 (cm)
        distance = duration * 17 / 1000;

        printf("Duration: %ld us\n", duration);
        printf("Distance: %ld cm\n\n", distance);

        sleep(1); // 1초 대기
    }

    gpiod_chip_close(chip);
    return 0;
}
