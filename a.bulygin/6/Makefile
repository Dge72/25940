CC      = cc
CFLAGS  = -O
LDFLAGS =
TARGET  = 6
SRC     = 6.c

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET) test-2.txt

clean:
	rm -f $(TARGET) core
