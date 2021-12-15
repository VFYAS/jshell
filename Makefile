C_MAIN_SOURCE=$(wildcard *.c)
LIBRARY=syntax.c executor.c error_handler.c
LIBRARY_OBJ=$(patsubst %.c, %.o, $(LIBRARY))
TESTS_MAIN=$(wildcard tests_main/tests/*)
TESTER_SRC=tests_main/tester.c
TESTER_EX=$(patsubst %.c, %, $(TESTER_SRC))
TEST_SCRIPT=test.sh

CFLAGS=-O2 -ftrapv -fsanitize=undefined -Wall -Werror \
       -Wformat-security -Wignored-qualifiers -Winit-self \
       -Wswitch-default -Wfloat-equal -Wshadow -Wpointer-arith \
       -Wtype-limits -Wempty-body -Wlogical-op -Wstrict-prototypes \
       -Wold-style-declaration -Wold-style-definition \
       -Wmissing-parameter-type -Wmissing-field-initializers \
       -Wnested-externs -Wno-pointer-sign -std=gnu11 -lm \
       -ggdb3 -Wno-unused-result -fsanitize=address -fsanitize=leak
CC=gcc
CVALFLAGS=-O2 -ftrapv -fsanitize=undefined -Wall -Werror \
	  -Wformat-security -Wignored-qualifiers -Winit-self \
	  -Wswitch-default -Wfloat-equal -Wshadow -Wpointer-arith \
	  -Wtype-limits -Wempty-body -Wlogical-op -Wstrict-prototypes \
	  -Wold-style-declaration -Wold-style-definition \
	  -Wmissing-parameter-type -Wmissing-field-initializers \
	  -Wnested-externs -Wno-pointer-sign -Wcast-qual -Wwrite-strings \
	  -std=gnu11 -lm
VALGR_FLAGS=--leak-check=full --show-leak-kinds=all \
	    --track-origins=yes --verbose
OBJS=$(patsubst %.c, %.o, $(C_MAIN_SOURCE))
PROGRAM=solution

.PHONY: all clean run valcheck valcomp test_main_comp test_main build_library

all: valcomp

%.o: %.c
	@$(CC) -c -o $@ $< $(CVALFLAGS)

$(PROGRAM): $(OBJS)
	@$(CC) -o $@ $^ $(CVALFLAGS) 

valcomp:
	@$(CC) -o $(PROGRAM) $(C_MAIN_SOURCE) $(CVALFLAGS)

comp_tester:
	@$(CC) -o $(TESTER_EX) $(TESTER_SRC) $(CFLAGS)

test_main: valcomp comp_tester
	@if [ ! -x $(TEST_SCRIPT) ]; then \
        chmod +x $(TEST_SCRIPT); \
    fi
	@./$(TEST_SCRIPT)

build_library: $(LIBRARY_OBJ)
	

build_library_val: $(LIBRARY)
	@$(CC) -c -o $(LIBRARY_OBJ) $< $(CVALFLAGS)

run: $(PROGRAM)
	./$<

valcheck: valcomp 
	valgrind $(VALGR_FLAGS) ./$(PROGRAM)

clean:
	@rm -f *.o $(PROGRAM) tests_main/*.o $(TESTER_EX)
