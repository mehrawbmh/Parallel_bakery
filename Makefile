CXX = g++
CXXFLAGS = -std=c++20 -lrt
TARGET = simulate
SRC = main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

phony:
	$(clean)
	