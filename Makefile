CC = gcc
# Setting the feature macro _FILE_OFFSET_BITS to 64 allows to use large files (> 2GB) on 32-bit systems
CCFLAGS = -W -Wall -D_FILE_OFFSET_BITS=64

BINARY_NAME = http-file-sharing

all:
	$(CC) $(CCFLAGS) Main.c Network.c -o $(BINARY_NAME)

clean:
	rm -f $(BINARY_NAME) $(BINARY_NAME).exe

install:
	@if [ $(shell id -u) -ne 0 ]; then printf "\033[31mThis rule must be started as root.\033[0m\n"; false; fi
	cp $(BINARY_NAME) /usr/bin

uninstall:
	@if [ $(shell id -u) -ne 0 ]; then printf "\033[31mThis rule must be started as root.\033[0m\n"; false; fi
	rm -f /usr/bin/$(BINARY_NAME)
