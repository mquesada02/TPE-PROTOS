CFLAGS:= -std=c11 -pedantic -lpthread -pedantic-errors -g -Wall -Werror -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-discarded-qualifiers -Wno-unused-const-variable -D_POSIX_C_SOURCE=200112L
TRACKER_CLI:= tracker
TRACKER_OBJS:= lib/args.o lib/selector.o lib/utils.o tracker.o

.PHONY: all clean

all:$(TRACKER_CLI)

utils.o: include/utils.h

args.o: include/args.h

selector.o: include/selector.h

$(TRACKER_CLI): $(TRACKER_OBJS)
	$(CC) $(CFLAGS) $(TRACKER_OBJS) -o $(TRACKER_CLI)

clean:
	- rm -rf $(TRACKER_CLI) $(TRACKER_OBJS) 