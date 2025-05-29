# Protocoale de comunicatii
# Laborator 7 - TCP
# Echo Server
# Makefile

CFLAGS = -Wall -g -Werror -Wno-error=unused-variable

# Portul pe care asculta serverul
PORT = 12345


all: server subscriber

utils.o: utils.c

common.o: common.c

# Compileaza server.c
server: server.c common.o utils.o

# Compileaza client.c
subscriber: subscriber.c common.o

.PHONY: clean run_server run_subscriber

# Ruleaza serverul
run_server:
	./server ${PORT}

# Ruleaza clientul 	
run_subscriber:
	./subscriber ${PORT}

clean:
	rm -rf server subscriber *.o *.dSYM
