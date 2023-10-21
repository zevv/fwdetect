
CFLAGS += -Wall -Werror

LDFLAGS += -lm

fwdetect: main.c biquad.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
