include ../common_definitions.mk

# File names
EXEC = tofcalc

SOURCES = $(SRC_DIR)/actions.cpp $(SRC_DIR)/states.cpp
OBJECTS = $(SOURCES:.cpp=.o)

C_SOURCES =
C_OBJECTS = $(C_SOURCES:.c=.o)

CFLAGS += 
CXXFLAGS += 

LDFLAGS += -ljansson -lm

.PHONY: all clean common_objects test

all: $(EXEC)

# To obtain object files
%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@
%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

common_objects: $(COMMON_OBJECTS)

# Main target
$(EXEC): $(OBJECTS) $(C_OBJECTS) $(EXEC).cpp
	$(CXX) $(CXXFLAGS) $(EXEC).cpp $(OBJECTS) $(C_OBJECTS) $(LDFLAGS) -o $(EXEC)

# To remove generated files
clean:
	rm -f $(EXEC) $(C_OBJECTS) $(OBJECTS)
