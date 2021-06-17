include ../common_definitions.mk

# File names
EXEC = unzad

SOURCES =
OBJECTS = $(SOURCES:.c=.o)

LIBS = -lc -lzmq -lz -lbz2
LDFLAGS = -L/opt/local/lib/ $(LIBS)

.PHONY: all clean

all: $(EXEC)

# To obtain object files
%.o: %.c
	$(CXX) -c $(CXXFLAGS) $< -o $@

# Main target
$(EXEC): $(OBJECTS) $(EXEC).c
	$(CC) $(CFLAGS) $(EXEC).c $(OBJECTS) $(LDFLAGS) -o $(EXEC)

# To remove generated files
clean:
	rm -f $(EXEC) $(OBJECTS)
