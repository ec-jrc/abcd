include ../common_definitions.mk

# File names
EXEC = hivo

SOURCES = $(SRC_DIR)/actions.cpp $(SRC_DIR)/states.cpp $(SRC_DIR)/class_caen_hv.cpp
OBJECTS = $(SOURCES:.cpp=.o)

LIBS += -lzmq -ljansson

# We do need CAEN libraries!
CXXFLAGS += $(CAEN_CFLAGS)
LDFLAGS += $(CAEN_LDFLAGS)

.PHONY: all clean common_objects

all: $(EXEC)

# To obtain object files
%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

common_objects: $(COMMON_OBJECTS)

# Main target
$(EXEC): common_objects $(OBJECTS) $(EXEC).cpp
	$(CXX) $(CXXFLAGS) $(EXEC).cpp $(OBJECTS) $(COMMON_OBJECTS) $(LDFLAGS) -o $(EXEC)

# To remove generated files
clean:
	rm -f $(EXEC) $(OBJECTS) $(COMMON_OBJECTS)
