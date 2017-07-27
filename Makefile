WUSELESS	=	-Wno-implicit-function-declaration 
OBJ		= 	main.c compile.c exec.c vbuf.c

all:
	gcc $(WUSELESS) -ggdb3 -o sed $(OBJ)
