NAME		=	sed
CC		=	gcc
CFLAGS		=	-ggdb3
WNO		=	-Wno-implicit-function-declaration 
SRC		= 	main.c compile.c exec.c vbuf.c
OBJDIR		=	obj
OBJ		=	$(SRC:%.c=$(OBJDIR)/%.o)

all:	$(NAME)

$(NAME): $(OBJ) | $(OBJDIR)
	$(CC) -o $@ $(CFLAGS) $(WNO) $^

$(OBJ): $(OBJDIR)/%.o : %.c data.h
	  $(CC) -o $@ $(CFLAGS) $(WNO) -c $<

$(OBJDIR):
	-mkdir $@

clean:
	rm -rf $(OBJDIR) $(NAME)
