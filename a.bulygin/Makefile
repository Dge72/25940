CC      = cc
CFLAGS  = -O
LDFLAGS =
TARGET  = 5
SRC     = 5.c

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET) test.txt

clean:
	rm -f $(TARGET) core
