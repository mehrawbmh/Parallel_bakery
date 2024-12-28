CXX = g++
CXXFLAGS = -std=c++20 -lrt
TARGET = single_baker.out
SRC = single_baker.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

phony:
	$(clean)
	