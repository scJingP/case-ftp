all:
	gcc -g ftpserver.c -o server -L. -Wl,-Bdynamic -lpthread -lsqlite3 -Wl,-Bstatic -lseqlist -Wl,-Bdynamic
	gcc -g ftpclient.c -o client -lpthread
	
	mv server ../test
	cp client ../test2
	mv client ../test3
clean:
	rm ftpserver.o ftpclient.o