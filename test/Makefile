TARGETS=$(patsubst %.c,%,$(shell ls *.c))
LDFLAGS+=-lpthread

all:$(TARGETS)

%:%.c

clean:
	$(RM) $(TARGETS)
