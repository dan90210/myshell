run: myshell.c lex.yy.c
	gcc lex.yy.c myshell.c -o run -lfl -w

lex.yy.c: lex.c
	flex lex.c

