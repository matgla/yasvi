# filepath: /home/mateusz/repos/yasvi/tests/Makefile

CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -I. -I..
LDFLAGS =  -Lbuild -static -lsut

SUT_SRCS = buffer.c
SUT_OBJS = $(patsubst %.c,build/sut/%.o,$(SUT_SRCS))

all: $(SUT_OBJS) run

build/sut/%.o: ../%.c
	mkdir -p build/sut
	$(CC) $(CFLAGS) -c $< -o $@

build/libsut.a: $(SUT_OBJS)
	ar rcs $@ $^

build/%.o: %.c acutest.h
	mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/buffer_tests: build/buffer_tests.o build/libsut.a
	$(CC) $(LDFLAGS) $^ -o $@

build/command_tests: build/command_tests.o build/libsut.a
	$(CC) $(LDFLAGS) $^ -o $@

run: build/buffer_tests build/command_tests
	./build/buffer_tests
	./build/command_tests

clean:
	rm -f $(OBJS) $(TARGET)
	rm -rf build

.PHONY: all clean