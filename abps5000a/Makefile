include ../common_definitions.mk

# File names
EXEC = abps5000a

SOURCES = $(SRC_DIR)/actions.cpp $(SRC_DIR)/states.cpp
OBJECTS = $(SOURCES:.cpp=.o)

CXXFLAGS += -I /opt/picoscope/include
LDFLAGS += -L /usr/lib -ljansson -L /opt/picoscope/lib
PICO_LIBS = -lps5000a -lpthread -lm -Wunknown-pragmas

.PHONY: all clean

all: $(EXEC)

# To obtain object files
%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

# Main target
$(EXEC): $(OBJECTS) $(EXEC).cpp
	$(CXX) $(CXXFLAGS) $(EXEC).cpp $(OBJECTS) $(LDFLAGS) $(PICO_LIBS) -o $(EXEC)

# To remove generated files
clean:
	rm -f $(EXEC) $(OBJECTS)
