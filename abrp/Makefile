include ../common_definitions.mk

# File names
EXEC = abrp

SOURCES = $(SRC_DIR)/actions.cpp $(SRC_DIR)/states.cpp
OBJECTS = $(SOURCES:.cpp=.o)

CXXFLAGS += -I../../api/include -I /opt/redpitaya/include
LDFLAGS += -L /usr/lib -ljansson -L /opt/redpitaya/lib -lrp

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
