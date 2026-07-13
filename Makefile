CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Os -s
LDFLAGS  ?= -lreadline
BINARY    = whiterose

SRCS := main.cpp
SRCS += $(wildcard src/*.cpp)
SRCS += $(wildcard src/interceptors/*.cpp)
OBJS := $(SRCS:.cpp=.o)

.PHONY: all clean format install install-user install-mac install-completions install-man

all: $(BINARY)

$(BINARY): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(LDFLAGS)

format:
	clang-format -i $(SRCS) $(wildcard src/*.hpp) $(wildcard src/interceptors/*.hpp)

clean:
	rm -f $(OBJS) $(BINARY)

install: $(BINARY) install-man install-completions
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 $(BINARY) $(DESTDIR)/usr/local/bin/$(BINARY)

install-user: $(BINARY)
	install -d $(HOME)/.local/bin
	install -m 755 $(BINARY) $(HOME)/.local/bin/$(BINARY)

install-mac: $(BINARY)
	install -d /usr/local/bin
	install -m 755 $(BINARY) /usr/local/bin/$(BINARY)

install-completions:
	install -d $(DESTDIR)/usr/share/bash-completion/completions
	install -m 644 completions/whiterose.bash $(DESTDIR)/usr/share/bash-completion/completions/whiterose
	install -d $(DESTDIR)/usr/share/zsh/site-functions
	install -m 644 completions/whiterose.zsh $(DESTDIR)/usr/share/zsh/site-functions/_whiterose

install-man:
	install -d $(DESTDIR)/usr/share/man/man1
	install -m 644 whiterose.1 $(DESTDIR)/usr/share/man/man1/whiterose.1
