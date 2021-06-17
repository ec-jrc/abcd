include ../common_definitions.mk

# File names
EXEC = califo

SOURCES = $(SRC_DIR)/actions.cpp $(SRC_DIR)/states.cpp $(COMMON_SRC_DIR)/background.cpp
OBJECTS = $(SOURCES:.cpp=.o) 

C_SOURCES =
C_OBJECTS = $(C_SOURCES:.c=.o) 

LDFLAGS += -ljansson -lm -lgsl -lgslcblas

.PHONY: all clean common_objects test

all: $(EXEC)

# To obtain object files
%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@
%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

common_objects: $(COMMON_OBJECTS)

# Main target
$(EXEC): common_objects $(OBJECTS) $(C_OBJECTS) $(EXEC).cpp
	$(CXX) $(CXXFLAGS) $(EXEC).cpp $(OBJECTS) $(C_OBJECTS) $(LDFLAGS) -o $(EXEC)

test: test.c include/fit_functions.h ../include/histogram.h
	$(CC) $(CFLAGS) test.c $(LDFLAGS) -o test
	@echo "Running test..."
	@./test > test_result.txt || (echo "Test failed" && exit 1)
	@echo "Test successful!"

# To remove generated files
clean:
	rm -f $(EXEC) $(OBJECTS) test test_result.txt
