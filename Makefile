# Makefile for threaded_file_tracker

CC      = gcc
CFLAGS  = -Wall -Wextra -O2
TARGET  = threaded_file_tracker
SRC     = threaded_file_tracker.c

# Try to get OpenSSL/SQLite flags from pkg-config if available
PKG_CONFIG ?= pkg-config
SSL_CFLAGS  := $(shell $(PKG_CONFIG) --cflags openssl 2>/dev/null)
SSL_LIBS    := $(shell $(PKG_CONFIG) --libs openssl 2>/dev/null)
SQLITE_CFLAGS := $(shell $(PKG_CONFIG) --cflags sqlite3 2>/dev/null)
SQLITE_LIBS   := $(shell $(PKG_CONFIG) --libs sqlite3 2>/dev/null)

# Fallback if pkg-config not available (macOS Homebrew typical paths)
ifeq ($(SSL_CFLAGS),)
SSL_CFLAGS = -I/opt/homebrew/opt/openssl@3/include -I/usr/local/opt/openssl@3/include
SSL_LIBS   = -L/opt/homebrew/opt/openssl@3/lib -L/usr/local/opt/openssl@3/lib -lssl -lcrypto
endif

ifeq ($(SQLITE_LIBS),)
SQLITE_LIBS = -lsqlite3
endif

# pthread is always needed
LIBS    = -lpthread $(SQLITE_LIBS) $(SSL_LIBS)
CFLAGS += $(SSL_CFLAGS) $(SQLITE_CFLAGS)

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f $(TARGET) *.o
