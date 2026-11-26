CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2
TARGET = poolang
SRC = main.cpp
PREFIX ?= /usr/local
DESTDIR ?=

.PHONY: all install uninstall clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

install: $(TARGET)
	mkdir -p "$(DESTDIR)$(PREFIX)/bin"
	install -m 755 $(TARGET) "$(DESTDIR)$(PREFIX)/bin/$(TARGET)"

uninstall:
	rm -f "$(DESTDIR)$(PREFIX)/bin/$(TARGET)"

clean:
	rm -f $(TARGET)
