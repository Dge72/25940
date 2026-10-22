CC      = cc
CFLAGS  = -O
LDFLAGS =
TARGET  = time_ex
SRC     = time_ex.c

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) core
