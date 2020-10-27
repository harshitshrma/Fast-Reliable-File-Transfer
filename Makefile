all: server.c client.c
	gcc -o server server.c -lpthread
	gcc -o client client.c -lpthread

# server: server
#	@./server 51717

# client: client
#	@./client cheerios 51717

data:
	dd if=/dev/urandom of=data.bin bs=1M count=1024 iflag=fullblock

clean:
	rm -f *.o client server 
ifneq ("", "$(wildcard ./data.bin)")
	rm data.bin
endif

zipcopy:
	tar cvzf lab3.tar.gz Makefile *.c* README.md
