include ../common_definitions.mk

# File names
EXEC = waps

SOURCES =
OBJECTS = $(SOURCES:.c=.o)

LIBS = -lc -lzmq -ljansson -lm -ldl
LDFLAGS = -L/opt/local/lib/ $(LIBS)

.PHONY: all clean

all: $(EXEC)

# To obtain object files
%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@
%.so: %.c
	$(CC) -shared -fPIC $(CFLAGS) $< -o $@

# Main target
$(EXEC): $(OBJECTS) $(EXEC).c
	$(CC) $(CFLAGS) $(EXEC).c $(OBJECTS) $(LDFLAGS) -o $(EXEC)

# To remove generated files
clean:
	rm -f $(EXEC) $(OBJECTS)
