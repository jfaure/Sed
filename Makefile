WUSELESS	=	-Wno-implicit-function-declaration 
OBJ		= 	main.c compile.c exec.c textbuf.c

all:
	gcc $(WUSELESS) -ggdb3 -o sed $(OBJ)
