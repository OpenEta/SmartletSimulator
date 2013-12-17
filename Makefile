all:
	gcc mockdevice.c -o test

clean:
	rm -rf *.o test
