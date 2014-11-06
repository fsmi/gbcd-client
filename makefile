CFLAGS=-g -Wall

all: gbcd-client

barcoded-cli: gbcd-client.c

clean:
	rm gbcd-client
