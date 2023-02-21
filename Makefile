build:
	gcc -g -o muros2 ./src/main.c

install:
	sudo cp ./muros2 /usr/bin/
