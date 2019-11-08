# Makefile for netty-tcnative native library code

CFLAGS = -fPIC -Wall -Wextra -O2 -g -I.
LDFLAGS = -shared -Wl,-soname,libnetty-tcnative.so.1 -lssl -lcrypto -lapr-1 -luuid -lrt -lcrypt -lpthread -ldl
RM = rm -f
TARGET_LIB = libnetty-tcnative.so.1.0

SRCS = bb.c  error.c  jnilib.c  native_constants.c  ssl.c  sslcontext.c  sslutils.c# source files
OBJS = $(SRCS:.c=.o)

.PHONY: all
all: ${TARGET_LIB}

$(TARGET_LIB): $(OBJS)
	$(CC) ${LDFLAGS} -o $@ $^

$(SRCS:.c=.d):%.d:%.c
	$(CC) $(CFLAGS) -MM $< >$@

include $(SRCS:.c=.d)

.PHONY: clean
clean:
	-${RM} ${TARGET_LIB} ${OBJS} $(SRCS:.c=.d)
