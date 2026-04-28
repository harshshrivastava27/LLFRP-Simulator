# Makefile for LLFRP scheduler simulator
#
# Targets:
#   make            -> builds ./scheduler
#   make run        -> builds and runs with task.txt
#   make clean      -> removes objects and binary

CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -std=c99
LDFLAGS :=
LDLIBS  := -lm

# All source files
SRCS := main.c           \
        task.c           \
        job.c            \
        ready_queue.c    \
        deadline_queue.c \
        metrics.c        \
        energy.c         \
        utils.c          \
        scheduler_llfrp.c

OBJS := $(SRCS:.c=.o)
DEPS := $(SRCS:.c=.d)

BIN  := scheduler

.PHONY: all run clean

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

# Compile + emit dependency files
%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

run: $(BIN)
	./$(BIN) task.txt

clean:
	rm -f $(BIN) $(OBJS) $(DEPS)

-include $(DEPS)
