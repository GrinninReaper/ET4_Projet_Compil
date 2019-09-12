OBJ=tp.o tp_l.o tp_y.o
CC=gcc
CFLAGS=-Wall -ansi -I./ -g
LDFLAGS= -g -lfl
tp : $(OBJ)
	$(CC) -o tp $(OBJ) $(LDFLAGS)

test_lex: tp_y.h test_lex.o tp_l.o
	$(CC) -o test_lex  test_lex.o tp_l.o $(LDFLAGS)

# Si absent lance yacc et fait "mv y.tab.c tp.c" ce qui ecrase notre fichier.
tp.c :
	echo ''

tp.o: tp.c tp_y.h tp.h
	$(CC) $(CFLAGS) -c tp.c

tp_l.o: tp_l.c tp_y.h
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-implicit-function-declaration -c tp_l.c

tp_y.o : tp_y.c
	$(CC) $(CFLAGS) -c tp_y.c

tp_y.h tp_y.c : tp.y tp.h
	bison -v -d -o tp_y.c tp.y

test_lex.o : test_lex.c tp.h tp_y.h
	$(CC) $(CFLAGS) -c test_lex.c

.Phony: clean

clean:
	rm -f *~ tp.exe* ./tp *.o tp_l.o tp_y.* test_lex tp_y.output
