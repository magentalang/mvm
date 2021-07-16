CC=gcc
CFLAGS=-Isrc

build:
	@[ -d ".bin" ] || mkdir .bin
	@echo "==> building"
	$(CC) $(CFLAGS) src/util.c src/stack.c main.c -o .bin/mvm

test: build
	@echo "==> running tests"
	@node tests/test

run: build
	@echo "==> running"
	@./.bin/mvm
