#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <gpiod.h>
#include <string.h>
#include <time.h>

// GPIO 설정
#define CHIPNAME "/dev/gpiochip0"
#define CONSUMER "Door_System"

// 키패드 설정
#define ROWS 4
#define COLS 4
int rowPins[ROWS] = {2, 3, 4, 5};
int colPins[COLS] = {6, 7, 8, 9};
char hexaKeys[ROWS][COLS] = {
    { '0', '1', '2', '3' },
    { '4', '5', '6', '7' },
    { '8', '9', 'A', 'B' },
    { 'C', 'D', 'E', 'F' }
};

// 서보모터 설정
#define SERVO_LINE 10
#define FREQ 50

// 초음파 센서 설정
#define TRIG 17
#define ECHO 27
#define DISTANCE_THRESHOLD 4  // 4cm 이하일 때 문 열기

// 비밀번호 설정 (예: "1234")
#define PASSWORD "1234"
#define PASSWORD_LENGTH 4

// 전역 변수
struct gpiod_chip *chip;
struct gpiod_line *rowLines[ROWS];
struct gpiod_line *colLines[COLS];
struct gpiod_line *servo_line;
struct gpiod_line *trig_line, *echo_line;
int door_open = 0;
pthread_mutex_t door_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t keypad_thread, distance_thread;

// 함수 프로토타입 선언
void set_angle(int duration_ms, int duty_us);
void open_door(void);
void close_door(void);
char scan_keypad(void);
void* keypad_handler(void* arg);
void* distance_handler(void* arg);
long get_microseconds(void);
long pulseIn(struct gpiod_line *line, int value);
void sleep_microseconds(long microseconds);
int init_gpio(void);

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

// 서보모터 제어 함수
void set_angle(int duration_ms, int duty_us) {
    int period_us = 1000000 / FREQ;
    int high_time = duty_us;
    int low_time = period_us - high_time;
    int total_cycles = (duration_ms * 1000) / period_us;
    
    printf("서보모터 제어 - 각도: %d, 주기: %d, HIGH: %d, LOW: %d, 사이클: %d\n", 
           duty_us, period_us, high_time, low_time, total_cycles);
    
    for (int i = 0; i < total_cycles; i++) {
        gpiod_line_set_value(servo_line, 1);
        usleep(high_time);
        gpiod_line_set_value(servo_line, 0);
        usleep(low_time);
    }
}

// 문 열기 함수
void open_door() {
    pthread_mutex_lock(&door_mutex);
    if (!door_open) {
        printf("문 열기 시작 (90도 회전)\n");
        set_angle(1000, 2000);  // 180도 (문 열림)
        door_open = 1;
        printf("문이 열렸습니다.\n");
    }
    pthread_mutex_unlock(&door_mutex);
}

// 문 닫기 함수
void close_door() {
    pthread_mutex_lock(&door_mutex);
    if (door_open) {
        printf("문 닫기 시작 (90도 회전)\n");
        set_angle(1000, 1000);  // 0도 (문 닫힘)
        door_open = 0;
        printf("문이 닫혔습니다.\n");
    }
    pthread_mutex_unlock(&door_mutex);
}

// 키패드 스캔 함수
char scan_keypad() {
    for (int i = 0; i < ROWS; i++) {
        gpiod_line_set_value(rowLines[i], 0);
        
        for (int j = 0; j < COLS; j++) {
            int val = gpiod_line_get_value(colLines[j]);
            if (val == 0) {
                gpiod_line_set_value(rowLines[i], 1);
                return hexaKeys[i][j];
            }
        }
        gpiod_line_set_value(rowLines[i], 1);
    }
    return 0;
}

// 키패드 스레드 함수
void* keypad_handler(void* arg) {
    char input_buffer[PASSWORD_LENGTH + 1] = {0};
    int input_index = 0;
    char prev_key = 0;
    
    printf("키패드 모니터링 시작\n");
    
    while (1) {
        char key = scan_keypad();
        
        if (key != 0 && key != prev_key) {
            printf("키 입력: %c\n", key);
            
            if (input_index < PASSWORD_LENGTH) {
                input_buffer[input_index++] = key;
                printf("입력된 비밀번호: %s\n", input_buffer);
                
                // 비밀번호가 완성되면 확인
                if (input_index == PASSWORD_LENGTH) {
                    if (strcmp(input_buffer, PASSWORD) == 0) {
                        printf("올바른 비밀번호! 문을 엽니다.\n");
                        open_door();
                        
                        // 5초 후 자동으로 문 닫기
                        sleep(5);
                        close_door();
                    } else {
                        printf("잘못된 비밀번호입니다.\n");
                    }
                    
                    // 입력 버퍼 초기화
                    memset(input_buffer, 0, sizeof(input_buffer));
                    input_index = 0;
                }
            }
            prev_key = key;
        } else if (key == 0) {
            prev_key = 0;
        }
        
        usleep(10000);  // 10ms 대기
    }
    
    return NULL;
}

// 거리 측정 스레드 함수
void* distance_handler(void* arg) {
    printf("거리 측정 모니터링 시작\n");
    
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

        printf("거리: %ld cm\n", distance);

        // 거리가 4cm 이하이고 문이 닫혀있으면 문 열기
        if (distance <= DISTANCE_THRESHOLD && !door_open) {
            printf("거리 %ld cm 감지! 자동으로 문을 엽니다.\n", distance);
            open_door();
            
            // 5초 후 자동으로 문 닫기
            sleep(5);
            close_door();
        }

        sleep(1); // 1초 대기
    }
    
    return NULL;
}

// GPIO 초기화
int init_gpio() {
    chip = gpiod_chip_open(CHIPNAME);
    if (!chip) {
        perror("GPIO 칩 열기 실패");
        return -1;
    }
    
    // 키패드 라인 설정
    for (int i = 0; i < ROWS; i++) {
        rowLines[i] = gpiod_chip_get_line(chip, rowPins[i]);
        if (!rowLines[i] || gpiod_line_request_output(rowLines[i], CONSUMER, 1) < 0) {
            perror("키패드 행 라인 설정 실패");
            return -1;
        }
    }
    
    for (int i = 0; i < COLS; i++) {
        colLines[i] = gpiod_chip_get_line(chip, colPins[i]);
        if (!colLines[i] || gpiod_line_request_input_flags(colLines[i], CONSUMER, GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP) < 0) {
            perror("키패드 열 라인 설정 실패");
            return -1;
        }
    }
    
    // 서보모터 라인 설정
    servo_line = gpiod_chip_get_line(chip, SERVO_LINE);
    if (!servo_line) {
        perror("서보모터 라인 가져오기 실패");
        return -1;
    }
    
    if (gpiod_line_request_output(servo_line, CONSUMER, 0) < 0) {
        perror("서보모터 라인 설정 실패");
        return -1;
    }
    
    // 초음파 센서 라인 설정
    trig_line = gpiod_chip_get_line(chip, TRIG);
    echo_line = gpiod_chip_get_line(chip, ECHO);
    if (!trig_line || !echo_line) {
        perror("초음파 센서 라인 가져오기 실패");
        return -1;
    }
    
    if (gpiod_line_request_output(trig_line, CONSUMER, 0) < 0 ||
        gpiod_line_request_input(echo_line, CONSUMER) < 0) {
        perror("초음파 센서 라인 설정 실패");
        return -1;
    }
    
    printf("GPIO 초기화 완료\n");
    return 0;
}

int main() {
    printf("도어 시스템 시작\n");
    printf("비밀번호: %s\n", PASSWORD);
    printf("거리 임계값: %d cm\n", DISTANCE_THRESHOLD);
    
    // GPIO 초기화
    if (init_gpio() < 0) {
        printf("GPIO 초기화 실패\n");
        return 1;
    }
    
    // 서보모터 초기 위치 설정 (문 닫힘 상태)
    printf("서보모터 초기 위치 설정\n");
    set_angle(1000, 1000);
    
    // 스레드 생성
    if (pthread_create(&keypad_thread, NULL, keypad_handler, NULL) != 0) {
        perror("키패드 스레드 생성 실패");
        return 1;
    }
    
    if (pthread_create(&distance_thread, NULL, distance_handler, NULL) != 0) {
        perror("거리 측정 스레드 생성 실패");
        return 1;
    }
    
    printf("모든 스레드가 시작되었습니다.\n");
    printf("키패드에서 비밀번호를 입력하거나 거리가 %d cm 이하일 때 문이 제어됩니다.\n", DISTANCE_THRESHOLD);
    
    // 메인 스레드 대기
    pthread_join(keypad_thread, NULL);
    pthread_join(distance_thread, NULL);
    
    // 정리
    gpiod_chip_close(chip);
    return 0;
} 
