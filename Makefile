NAME		=	sed
CFLAGS		=	-ggdb3
WUSELESS	=	-Wno-implicit-function-declaration 
SRC		= 	main.c compile.c exec.c vbuf.c
OBJDIR		=	obj
OBJ		=	$(SRC:%.c=$(OBJDIR)/%.o)

all:	$(NAME)

$(NAME): $(OBJ) | $(OBJDIR)
	gcc -o $@ $(CFLAGS) $(WUSELESS) $^

$(OBJ): obj/%.o : %.c data.h
	  gcc -o $@ $(CFLAGS) $(WUSELESS) -c $<

$(OBJDIR):
	-mkdir $@

clean:
	rm -rf $(OBJDIR) $(NAME)
