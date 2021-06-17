include ../common_definitions.mk

# File names
EXEC = abcd

SOURCES = $(SRC_DIR)/actions.cpp $(SRC_DIR)/states.cpp $(SRC_DIR)/class_caen_dgtz.cpp
OBJECTS = $(SOURCES:.cpp=.o) $(COMMON_SRC_DIR)/socket_functions.o

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
