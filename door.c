#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>

#define CHIP "/dev/gpiochip0"
#define LINE 3
#define BTN_BASE 4
#define BTN_NUM 2

#define FREQ 50

void set_angle(struct gpiod_line *line, int duration_ms, int duty_us) {
	int period_us = 1000000 / FREQ;
	int high_time = duty_us;
	int low_time = period_us - high_time;
	int total_cycles = (duration_ms * 1000) / period_us;
	
	printf("PWM 설정 - 주기: %d, HIGH: %d, LOW: %d, 사이클: %d\n", 
		   period_us, high_time, low_time, total_cycles);
	
	for (int i=0;i<total_cycles;i++) {
		gpiod_line_set_value(line, 1);
		usleep(high_time);
		gpiod_line_set_value(line, 0);
		usleep(low_time);
	}
}

int main() {
	struct gpiod_chip *chip;
	struct gpiod_line *line;
	struct gpiod_line *btn[BTN_NUM];
	int duty_us;
	
	chip = gpiod_chip_open(CHIP);
	if (!chip) {
		perror("chip open failed");
		return 1;
	}
	printf("GPIO 칩 열기 성공\n");
	
	// 서보모터
	line = gpiod_chip_get_line(chip, LINE);
	if (!line) {
		perror("get line failed");
		gpiod_chip_close(chip);
		return 1;
	}
	printf("서보모터 라인 설정 성공\n");


	if (gpiod_line_request_output(line, "servo", 1) < 0) {
		perror("request output failed");
		gpiod_chip_close(chip);
		return 1;
	}
	printf("서보모터 출력 모드 설정 성공\n");
	
	int angle = 1500;
	
	printf("초기 세팅\n");
	set_angle(line, 1000, angle);	// 90도
	printf("초기 세팅 완료\n");

	while (1) {

		if (왼쪽) {
			angle -= 100;
			if (angle < 1000) {
				angle = 1000;
			}
			printf("angle %d로 왼쪽 회전 시작\n", angle);
			set_angle(line, 1000, angle);  // 0도로 회전
			printf("왼쪽 회전 완료\n");
		}
		if (오른쪽) {
			angle += 100;
			if (angle > 2000) {
				angle = 2000;
			}
			printf("angle %d로 오른쪽 회전 시작\n", angle);
			set_angle(line, 1000, angle);  // 180도로 회전
			printf("오른쪽 회전 완료\n");
		}

		usleep(200000);  // 0.2초 대기
	}
}
