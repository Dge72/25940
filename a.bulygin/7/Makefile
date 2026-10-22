CC      = cc
CFLAGS  = -O
LDFLAGS =
TARGET  = 7
SRC     = 7.c

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET) test-3.txt

clean:
	rm -f $(TARGET) core
