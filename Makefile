CC      = gcc
CFLAGS  = -Wall -Wextra -g -O2

TARGET  = memory_hierarchy
SRC     = memory_hierarchy.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
