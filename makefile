run: myshell.c lex.yy.c
	gcc lex.yy.c myshell.c -o run -lfl -w

clean:
	rm lex.yy.c
	rm run

lex.yy.c: lex.c
	flex lex.c

