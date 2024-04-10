include ../common_definitions.mk

# File names
SOURCES = replay_raw.c replay_events.c
EXECS = $(SOURCES:.c=)

LIBS = -lzmq
LDFLAGS = -L/opt/local/lib/ $(LIBS)

.PHONY: all clean

all: $(EXECS)

%: %.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

# To remove generated files
clean:
	rm -f $(EXECS)
