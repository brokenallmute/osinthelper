CC = gcc
CFLAGS = -O3 -march=native -flto -ffast-math -funroll-loops -pthread -Wall -Wextra
LDFLAGS = -lcurl
TARGET = osint-helper
SRC_DIR = src
SOURCES = $(SRC_DIR)/main.c $(SRC_DIR)/ui.c $(SRC_DIR)/search.c $(SRC_DIR)/ipinfo.c $(SRC_DIR)/emailvalidator.c $(SRC_DIR)/tempmail.c 
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
