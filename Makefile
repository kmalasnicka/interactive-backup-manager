CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11
TARGET = backup-manager
SRC = main.c backup.c copy.c monitor.c restore.c utils.c
OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean