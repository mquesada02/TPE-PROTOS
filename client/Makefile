CFLAGS:= -std=c11 -pedantic -lpthread -pedantic-errors -g -Wall -Werror -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-discarded-qualifiers -Wno-unused-const-variable -D_POSIX_C_SOURCE=200112L
CLIENT_CLI:= client
CLIENT_OBJS:= lib/args.o lib/selector.o lib/utils.o lib/connectionManager.o client.o

.PHONY: all clean

all:$(CLIENT_CLI)

utils.o: include/utils.h

args.o: include/args.h

selector.o: include/selector.h

connectionManager.o: include/connectionManager.h

$(CLIENT_CLI): $(CLIENT_OBJS)
	$(CC) $(CFLAGS) $(CLIENT_OBJS) -o $(CLIENT_CLI)

clean:
	- rm -rf $(CLIENT_CLI) $(CLIENT_OBJS)