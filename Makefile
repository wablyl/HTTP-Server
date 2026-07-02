CC = clang
CFLAGS = -Wall -Wextra -Werror -pedantic -g
LDFLAGS = -pthread

# Default target
all: httpserver

# Link the object files and the helper library to create the executable
httpserver: httpserver.o rwlock.o queue.o
	$(CC) $(LDFLAGS) -o httpserver httpserver.o rwlock.o queue.o asgn4_helper_funcs.a

# Compile each C file into an object file
httpserver.o: httpserver.c
	$(CC) $(CFLAGS) -c httpserver.c

rwlock.o: rwlock.c rwlock.h
	$(CC) $(CFLAGS) -c rwlock.c

queue.o: queue.c queue.h
	$(CC) $(CFLAGS) -c queue.c

# Clean rule to remove object files and the binary
clean:
	rm -f *.o httpserver

# Default tar

# Link the object files and the helper library to create the exe
