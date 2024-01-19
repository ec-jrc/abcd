CC = gcc

CFLAGS = -std=gnu99 -O3 -W -Wall -pedantic -I../include

LIBS = -lc
LDFLAGS = $(LIBS)

SOURCES = sort_ade.c
EXECS = $(SOURCES:.c=)

.PHONY: all clean

all: $(EXECS)

%: %.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

# To remove generated files
clean:
	rm -f $(EXECS)
