CC = gcc
CFLAGS = -Wall -Wextra -pthread
LIBS = -lgpiod -lm

# 타겟들
TARGETS = main key pir door

# 기본 타겟
all: $(TARGETS)

# 메인 프로그램
main: main.c
	$(CC) $(CFLAGS) -o main main.c $(LIBS)

# 개별 모듈들
key: key.c
	$(CC) $(CFLAGS) -o key key.c $(LIBS)

pir: pir.c
	$(CC) $(CFLAGS) -o pir pir.c $(LIBS)

door: door.c
	$(CC) $(CFLAGS) -o door door.c $(LIBS)

# 정리
clean:
	rm -f $(TARGETS)

# 실행
run: main
	./main

.PHONY: all clean run 