CXX = g++
CXXFLAGS = -std=c++17 -O2
TARGET = poolang
SRC = main.cpp
INSTALL_DIR = /usr/local/bin

# Runs when you just type "make"
all: $(TARGET)

# Compiles main.cpp into poolang
$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

# Copies poolang into your system PATH so it becomes a global command
install: $(TARGET)
	install -m 755 $(TARGET) $(INSTALL_DIR)/$(TARGET)

# Removes the compiled binary for a clean rebuild
clean:
	rm -f $(TARGET)
