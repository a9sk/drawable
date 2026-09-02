CC = gcc
CFLAGS = -Wall -Wextra -std=c99
LDFLAGS = $(shell pkg-config --libs raylib)

TARGET = drawtable
SRC = main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)
