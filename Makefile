CFLAGS=-std=gnu99 -Wall -Werror -O3

tap2tap: tap2tap.c
	$(CC) $(CCFLAGS) -o "$@" "$<"
