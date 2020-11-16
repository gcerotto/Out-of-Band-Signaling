CC=gcc
CFLAGS= -Wall -pedantic
FLAG_S= -pthread
OBJN= net.o
OBJC= client.o
OBJS= server.o supervisor.o
DEPS= header.h net.c net.h
SERVERFILE= serverout
CLIENTFILE= clientout

.PHONY: clean test all

all: supervisor client

client: $(OBJC) $(OBJN)
	$(CC) $^ -o $@

supervisor: $(OBJS) $(OBJN)
	$(CC) $(FLAG_S) $^ -o $@

client.o: client.c $(DEPS)

supervisor.o: supervisor.c $(DEPS)

server.o: server.c $(DEPS)

net.o: net.c net.h

clean: 
	-rm client supervisor $(OBJC) $(OBJS) $(CLIENTFILE) $(SERVERFILE)

test: all
	./supervisor 8 > $(SERVERFILE) & \
	PIDSERVER=$$! ;\
	sleep 2 ;\
	> $(CLIENTFILE) ;\
	for i in $$(seq 1 10); do \
		(./client 5 8 20 >> $(CLIENTFILE) &); \
		(./client 5 8 20 >> $(CLIENTFILE) &); \
		sleep 1; \
	done ;\
	for i in $$(seq 1 6); \
		do sleep 10; \
		kill -s 2 $$PIDSERVER; \
	done; \
	sleep 0.5; \
	kill -s 2 $$PIDSERVER ; \
	./misura $(CLIENTFILE) $(SERVERFILE)