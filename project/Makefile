CC 						= gcc
CFLAGS 				= -std=c89 -pedantic -g -D_GNU_SOURCE
INCLUDES 			= src/headers/*.h
COMMON_DEPS 	= $(INCLUDES) Makefile

build/%.o: src/%.c $(COMMON_DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

all: bin/block_chain

bin/block_chain: build/master.o bin/node bin/user build/header.o $(COMMON_DEPS)
	$(CC) $(CFLAGS) -o bin/block_chain build/master.o build/header.o
	
bin/user: build/user.o build/header.o $(COMMON_DEPS)
	$(CC) $(CFLAGS) -o bin/user build/user.o build/header.o

bin/node: build/node.o build/header.o $(COMMON_DEPS)
	$(CC) $(CFLAGS) -o bin/node build/node.o build/header.o

run:
	bin/block_chain

clean:
	rm -f build/* bin/*


