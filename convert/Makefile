include ../common_definitions.mk

SOURCES = ade2ascii.c adr2ade.c adw2ascii.c
EXECS = $(SOURCES:.c=)

.PHONY: all clean

all: $(EXECS)

%: %.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

# To remove generated files
clean:
	rm -f $(EXECS)
