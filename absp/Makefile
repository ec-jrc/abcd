include ../common_definitions.mk

# File names
EXEC = absp

SOURCES = $(SRC_DIR)/ADQ_descriptions.cpp
SOURCES += $(SRC_DIR)/ADQ214.cpp $(SRC_DIR)/ADQ412.cpp $(SRC_DIR)/ADQ14_FWDAQ.cpp $(SRC_DIR)/ADQ14_FWPD.cpp $(SRC_DIR)/ADQ36_FWDAQ.cpp
SOURCES += $(SRC_DIR)/Digitizers_wrap.cpp
SOURCES += $(SRC_DIR)/actions.cpp $(SRC_DIR)/states.cpp

OBJECTS = $(SOURCES:.cpp=.o)

CXXFLAGS += -I/usr/include/lua5.4
LDFLAGS += -L /usr/lib -ljansson -ladq -llua5.4

.PHONY: all clean

all: $(EXEC)

# To obtain object files
%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(SRC_DIR)/Digitizers_wrap.cpp: $(INCLUDE_DIR)/Digitizers.i
	swig -D__linux__ -c++ -lua -I/usr/include/ -I$(COMMON_INCLUDE_DIR) -o $@ $<

$(INCLUDE_DIR)/swigluarun.h: $(INCLUDE_DIR)/Digitizers.i
	swig -D__linux__ -c++ -lua -external-runtime $(INCLUDE_DIR)/swigluarun.h

$(SRC_DIR)/Digitizers_wrap.o: $(SRC_DIR)/Digitizers_wrap.cpp $(INCLUDE_DIR)/swigluarun.h
	$(CXX) -I/usr/include/lua5.4/ -c -std=c++17 -O3 -I$(COMMON_INCLUDE_DIR) -I$(INCLUDE_DIR) -I/usr/include/lua5.4 $< -o $@

# Main target
$(EXEC): $(INCLUDE_DIR)/LuaManager.hpp $(OBJECTS) $(EXEC).cpp
	$(CXX) $(CXXFLAGS) $(EXEC).cpp $(OBJECTS) $(LDFLAGS) -o $(EXEC)

clean-swig:
	rm -f $(INCLUDE_DIR)/swigluarun.h
	rm -f $(SRC_DIR)/Digitizers_wrap.cpp

# To remove generated files
clean:
	rm -f $(EXEC) $(OBJECTS)
