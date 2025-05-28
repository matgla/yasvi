CC ?= armv8m-tcc
CFLAGS = -Wall -Wextra -Werror -g 
LDFLAGS = -fsanitize=address
SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c, build/%.o, $(SRCS))

ifeq ($(CC), armv8m-tcc)
CFLAGS += -I../../rootfs/usr/include
LDFLAGS += -L../../rootfs/lib -lncurses
else
CFLAGS += -fsanitize=address -fno-omit-frame-pointer -I../../libs/yasos_curses/include
LDFLAGS += -L../../libs/yasos_curses/build -Wl,-rpath=../../libs/yasos_curses/build -lncurses
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