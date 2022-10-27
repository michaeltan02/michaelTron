CC=gcc
CFLAGS=-I.

michaelTron: michaelTron.o
	${CC} -o michaelTron michaelTron.o -Wall -Wextra -pedantic


