include ../common_definitions.mk

# File names
EXEC = absp

SOURCES = $(SRC_DIR)/actions.cpp $(SRC_DIR)/states.cpp $(SRC_DIR)/ADQ_descriptions.cpp $(SRC_DIR)/ADQ412.cpp $(SRC_DIR)/ADQ14_FWDAQ.cpp $(SRC_DIR)/ADQ14_FWPD.cpp $(SRC_DIR)/ADQ214.cpp
OBJECTS = $(SOURCES:.cpp=.o)

CXXFLAGS += -std=c++11
LDFLAGS += -L /usr/lib -ljansson -ladq

.PHONY: all clean

all: $(EXEC)

# To obtain object files
%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

# Main target
$(EXEC): $(OBJECTS) $(EXEC).cpp
	$(CXX) $(CXXFLAGS) $(EXEC).cpp $(OBJECTS) $(LDFLAGS) -o $(EXEC)

# To remove generated files
clean:
	rm -f $(EXEC) $(OBJECTS)