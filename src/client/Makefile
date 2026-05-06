CFLAGS += -I../include/ -Wall

all: client

client: client.o
	gcc $^ -o $@ $(CFLAGS)

client.o: client.c
	gcc -c $< -o $@ $(CFLAGS)

clean:
	rm -rf *.o client