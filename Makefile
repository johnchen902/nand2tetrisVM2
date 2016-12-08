.PHONY : all clean

all : sbt emulib.o

sbt : sbt.cpp
	g++ -o sbt sbt.cpp -Wall -Wextra

emulib.o : emulib.c
	gcc -o emulib.o -c emulib.c -Wall -Wextra -O3

program : program.c emulib.o
	gcc -o program program.c emulib.o -Wall -Wextra -O1 -lX11

clean :
	$(RM) sbt emulib.o program.c program
