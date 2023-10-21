
CFLAGS += -Wall -Werror

LDFLAGS += -lm

fwdetect: main.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
