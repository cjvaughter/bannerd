NAME ?= bannerd
ROOTFSDIR ?= _install

OBJS = animation.o bmp.o fb.o image.o main.o png.o
LDLIBS = -lpng
CFLAGS += -DSRV_NAME=\"$(NAME)\"

.PHONY: all clean install no_png

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(LDFLAGS) $(LDLIBS) -o $(NAME) $(OBJS)

no_png: CFLAGS += -DNO_PNG
no_png: $(NAME)

clean:
	rm -fr *.o *~ $(NAME)

install:
	install $(NAME) $(ROOTFSDIR)/bin

