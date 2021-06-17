include ../common_definitions.mk

# File names
EXEC = dummy_waveforms_publisher

SOURCES =
OBJECTS = $(SOURCES:.c=.o)

LIBS = -lc -lzmq -ljansson -lm
LDFLAGS = -L/opt/local/lib/ $(LIBS)

.PHONY: all clean

all: $(EXEC)

# To obtain object files
%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

# Main target
$(EXEC): $(OBJECTS) $(EXEC).c
	$(CC) $(CFLAGS) $(EXEC).c $(OBJECTS) $(LDFLAGS) -o $(EXEC)

# To remove generated files
clean:
	rm -f $(EXEC) $(OBJECTS)
