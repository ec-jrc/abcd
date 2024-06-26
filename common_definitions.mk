################################################################################
# Common definitions                                                           #
################################################################################

# This can be set to "clang" or "gcc"
ABCD_COMPILER = gcc

COMMON_INCLUDE_DIR = ../include
COMMON_SRC_DIR = ../src

SRC_DIR = src
INCLUDE_DIR = include/

# Flags
ifeq ($(ABCD_COMPILER),clang)
	CXX = clang++
	CC = clang

	CXXFLAGS += -std=c++17 -stdlib=libc++
else
	CXX = g++
	CC = gcc

	CXXFLAGS += -std=c++17
endif

CXXFLAGS += -O3 -W -Wall -pedantic -I$(COMMON_INCLUDE_DIR) -I$(INCLUDE_DIR) -I/usr/include/ -I/usr/include/jsoncpp/ -I /opt/local/include/ -I /usr/local/include/ -I /usr/local/include/jsoncpp/
CFLAGS += -std=c99 -O3 -W -Wall -pedantic -I$(COMMON_INCLUDE_DIR) -I$(INCLUDE_DIR) -I/usr/include/ -I /opt/local/include/ -I /usr/local/include/
LIBS += -lzmq -ljsoncpp
LDFLAGS += -L/usr/lib/ -L/usr/lib64/ -L/usr/lib/x86_64-linux-gnu/ -L/opt/local/lib/ -L/usr/local/lib/ $(LIBS)

# In case we need CAEN libraries...
CAEN_CFLAGS =
CAEN_LDFLAGS = -lCAENComm -lCAENVME -lCAENDigitizer

COMMON_SOURCES = $(COMMON_SRC_DIR)/utilities_functions.cpp
COMMON_OBJECTS = $(COMMON_SOURCES:.cpp=.o)
