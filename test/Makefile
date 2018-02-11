TARGETS=$(patsubst %.c,%,$(shell ls *.c))
CFLAGS+=-I../
LDFLAGS+=-lpthread

all:$(TARGETS)

proxy_server:proxy_server.o ../corecomm.o ../globals.o ../netlist.o

%:%.c

clean:
	$(RM) $(TARGETS)
