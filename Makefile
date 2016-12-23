CFLAGS=-std=gnu99 -Wall -Werror -O3 -g

C_SRCS := $(wildcard *.c)
C_OBJS := $(C_SRCS:.c=.o)


tap2tap: $(C_OBJS) Makefile
	$(CC) $(CCFLAGS) $(C_OBJS) -o "$@"

define OBJECT_DEPENDS_ON_CORRESPONDING_HEADER
    $(1) : ${1:.o=.h}
endef

$(foreach object_file,$(C_OBJS),$(eval $(call OBJECT_DEPENDS_ON_CORRESPONDING_HEADER,$(object_file))))
