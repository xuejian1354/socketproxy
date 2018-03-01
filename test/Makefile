TARGETS=$(patsubst %.c,%,$(shell ls *.c))
CFLAGS+=-I../ -DPTHREAD_SELECT_NUM=1
LDFLAGS+=-lpthread

proxy_server:proxy_server.o corecomm.o globals.o netlist.o

all:$(TARGETS)

corecomm.o:../corecomm.c
	$(CC) $(CFLAGS) -c -w -o $@ $<

globals.o:../globals.c
	$(CC) $(CFLAGS) -c -o $@ $<

netlist.o:../netlist.c
	$(CC) $(CFLAGS) -c -o $@ $<

%:%.c

clean:
	$(RM) *.o $(TARGETS)
