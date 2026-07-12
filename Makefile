CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Os -s
LDFLAGS  ?= -lreadline
BINARY    = whiterose

.PHONY: all clean install install-user install-mac uninstall

all: $(BINARY)

$(BINARY): main.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(BINARY)

install: $(BINARY)
	install -d $(DESTDIR)/usr/local/bin
	install -m 755 $(BINARY) $(DESTDIR)/usr/local/bin/$(BINARY)

install-user: $(BINARY)
	install -d $(HOME)/.local/bin
	install -m 755 $(BINARY) $(HOME)/.local/bin/$(BINARY)
	@echo "Installed to $$HOME/.local/bin/$(BINARY)."
	@echo "Make sure $$HOME/.local/bin is on your PATH."

install-mac: $(BINARY)
	install -d /usr/local/bin
	install -m 755 $(BINARY) /usr/local/bin/$(BINARY)

uninstall:
	rm -f /usr/local/bin/$(BINARY) $(HOME)/.local/bin/$(BINARY)
