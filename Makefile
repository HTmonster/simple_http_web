CC = gcc
CFLAGS = -Wall -g
LIBS = -lpthread
TARGET = shttpd
#RM = rm -f 
OBJS = shttpd_parameter.o shttpd.o  shttpd_worker.o shttpd_error.o  shttpd_request.o shttpd_method.o shttpd_mine.o
# shttpd_cgi.o shttpd_uri.o 
all:$(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LIBS)
clean:
	$(RM) $(TARGET) $(OBJS)