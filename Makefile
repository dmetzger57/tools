# Makefile for file_tracker

CC = gcc
CFLAGS = -Wall -Wextra -pedantic -I/opt/homebrew/opt/openssl/include
LDFLAGS = -L/opt/homebrew/opt/openssl/lib
LIBS = -lcrypto -lsqlite3

TARGET = file_tracker
SRC = file_tracker.c
OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ)

install:
	mv file_tracker $(HOME)/bin
