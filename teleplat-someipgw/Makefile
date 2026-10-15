# Compiler and flags
CXX      ?= g++
CXXFLAGS ?= -O2 -pipe -std=c++17 -Wall -Wextra
CXXFLAGS += -I. -Wno-deprecated-declarations
LDFLAGS  ?=

# Target and sources
TARGET  := telePlatSomeipGWSvc
SOURCES := teleplat-someipgw-main.cpp \
           telePlatSomeipGWSvc.cpp \
           telePlatSomeipClntImpl.cpp \
           telePlatSomeipSvrImpl.cpp

HEADERS := telePlatSomeipGWSvc.hpp \
           telePlatSomeipClnt.hpp \
           telePlatSomeipSvr.hpp \
           interfaces.h \
           platformSvcIF.hpp \
           teleplat_framework.h

OBJECTS := $(SOURCES:.cpp=.o)

# Libraries
LIBS := -lvsomeip3 \
        -ljansson \
        -lpthread \
        -lrt \
        -lboost_thread \
        -lboost_regex \
        -lboost_filesystem \
        -lboost_chrono \
        -lboost_atomic \
        -lboost_system \
        -lboost_log_setup \
        -lboost_log

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJECTS)
