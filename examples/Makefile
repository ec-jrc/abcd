include ../common_definitions.mk

# File names
SOURCES = dummy_waveforms_publisher.c read_status.c
EXECS = $(SOURCES:.c=)

LIBS = -lc -lzmq -ljansson -lm
LDFLAGS = -L/opt/local/lib/ $(LIBS)

.PHONY: all clean

all: $(EXECS)

%: %.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

# To remove generated files
clean:
	rm -f $(EXECS)
