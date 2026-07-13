CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Os -s
LDFLAGS  ?= -lreadline
BINARY    = whiterose

SRCS := main.cpp
SRCS += $(wildcard src/*.cpp)
SRCS += $(wildcard src/interceptors/*.cpp)
OBJS := $(SRCS:.cpp=.o)

.PHONY: all clean install install-user install-mac

all: $(BINARY)

$(BINARY): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(LDFLAGS)

clean:
	rm -f $(OBJS) $(BINARY)

install: $(BINARY)
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 $(BINARY) $(DESTDIR)/usr/local/bin/$(BINARY)

install-user: $(BINARY)
	install -d $(HOME)/.local/bin
	install -m 755 $(BINARY) $(HOME)/.local/bin/$(BINARY)

install-mac: $(BINARY)
	install -d /usr/local/bin
	install -m 755 $(BINARY) /usr/local/bin/$(BINARY)
