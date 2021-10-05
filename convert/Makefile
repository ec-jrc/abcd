CC = gcc

CFLAGS = -std=c99 -O3 -W -Wall -pedantic 

LIBS = -lc
LDFLAGS = $(LIBS)

SOURCES = ade2ascii.c adr2ade.c adw2ascii.c
EXECS = $(SOURCES:.c=)

.PHONY: all clean

all: $(EXECS)

%: %.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

# To remove generated files
clean:
	rm -f $(EXECS)
