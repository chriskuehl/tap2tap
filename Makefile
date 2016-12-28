CFLAGS := -std=gnu99 -Wall -Werror -O3 -g
C_SRCS := $(wildcard *.c)
C_OBJS := $(C_SRCS:.c=.o)


tap2tap: $(C_OBJS)
	$(CC) $(CFLAGS) $(C_OBJS) -o "$@" -lsodium

%.o: %.c
	$(CC) $(CFLAGS) -c "$<"

.PHONY: test
test: tap2tap
	tox

.PHONY: clean
clean:
	rm -f *.o
	rm -f tap2tap
