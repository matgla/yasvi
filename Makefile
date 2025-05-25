CC ?= armv8m-tcc
CFLAGS = -Wall -g -fsanitize=address -fno-omit-frame-pointer 
LDFLAGS = -lncurses -fsanitize=address
SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c, build/%.o, $(SRCS))

ifeq ($(CC), armv8m-tcc)
CFLAGS = $(CFLAGS) -I../../rootfs/usr/include -L../../rootfs/lib
endif

TARGET = build/vi

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
INCLUDEDIR ?= $(PREFIX)/include

# Rules
all: $(TARGET)

build/%.o: %.c
	mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

install: $(TARGET) 
	mkdir -p $(BINDIR)
	cp $(TARGET) $(BINDIR)

TEST_TARGET := tests

.PHONY: test

test: $(TEST_TARGET)
	$(MAKE) -C $(TEST_TARGET) run 

clean:
	rm -rf build