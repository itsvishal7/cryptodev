CC = gcc
CFLAGS = -g

LIB=lib
SRC=src
TEST_SRC=test-cases
INC=include
LIB_OBJ=lib
OBJ=obj


# src = $(wildcard Examples/*.c)

# obj = $(src:.c=.o)

HDRS=$(shell ls $(INC)/*.h)

all: test1 # library #examples

test1: $(OBJ)/test1.o $(LIB)/libcrypter.so
	$(CC) -o $@ -I$(INC) $< -L$(LIB) -lcrypter

$(OBJ)/%.o: $(TEST_SRC)/%.c $(HDRS)
	mkdir -p obj
	$(CC) -c -I$(INC) $< -o $@

library: $(LIB)/libcrypter.so

$(LIB)/libcrypter.so: $(LIB_OBJ)/crypter.o
	gcc -shared -o $@ $^

$(LIB_OBJ)/%.o: $(SRC)/%.c $(HDRS)
	mkdir -p lib
	$(CC) -fPIC -c -I$(INC) $< -o $@

%-pa-cs730.tar.gz:	clean
	tar cf - `find . -type f | grep -v '^\.*$$' | grep -v '/CVS/' | grep -v '/\.svn/' | grep -v '/\.git/' | grep -v '[0-9].*\.tar\.gz' | grep -v '/submit.token$$'` | gzip > $@

.PHONY: prepare-submit
prepare-submit: $(RNO)-pa-cs730.tar.gz

.PHONY: clean
clean:
	rm -rf test1 $(OBJ) $(LIB)
