# define the C compiler to use
CC = gcc -g

# define any compile-time flags
# for newer compiler, the right hand side is not needed...
# ... for instance, remove the flags if using gnu/10.1.0 on Cheyenne.
CFLAGS = -fsanitize=address --coverage

INCLUDES = -I../include

LFLAGS =

LIBS = -lm

# define the C source files for main BMI test
SRCS = main_unit_test_bmi.c ../src/bmi_topmodel.c ../src/topmodel.c

# define the C object files 
OBJS = $(SRCS:.c=.o)

# define the executable files 
MAIN = run_bmi_unit_test
TEST_ARRAY_ALLOC = test_array_alloc
TEST_SHIFT_Q = test_shift_q
TEST_TOPMODEL_CALCS = test_topmodel_calcs
TEST_MULTI_STEP = test_multi_step
TEST_EDGE_CASES = test_edge_cases

ALL_TESTS = $(MAIN) $(TEST_ARRAY_ALLOC) $(TEST_SHIFT_Q) $(TEST_TOPMODEL_CALCS) $(TEST_MULTI_STEP) $(TEST_EDGE_CASES)

.PHONY: depend clean autotest

all:    $(ALL_TESTS)
	@echo  All unit tests compiled successfully

$(MAIN): main_unit_test_bmi.o ../src/bmi_topmodel.o ../src/topmodel.o
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) main_unit_test_bmi.o ../src/bmi_topmodel.o ../src/topmodel.o $(LFLAGS) $(LIBS)

$(TEST_ARRAY_ALLOC): test_array_alloc.o ../src/topmodel.o
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TEST_ARRAY_ALLOC) test_array_alloc.o ../src/topmodel.o $(LFLAGS) $(LIBS)

$(TEST_SHIFT_Q): test_shift_q.o ../src/topmodel.o
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TEST_SHIFT_Q) test_shift_q.o ../src/topmodel.o $(LFLAGS) $(LIBS)

$(TEST_TOPMODEL_CALCS): test_topmodel_calcs.o ../src/bmi_topmodel.o ../src/topmodel.o
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TEST_TOPMODEL_CALCS) test_topmodel_calcs.o ../src/bmi_topmodel.o ../src/topmodel.o $(LFLAGS) $(LIBS)

$(TEST_MULTI_STEP): test_multi_step.o ../src/bmi_topmodel.o ../src/topmodel.o
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TEST_MULTI_STEP) test_multi_step.o ../src/bmi_topmodel.o ../src/topmodel.o $(LFLAGS) $(LIBS)

$(TEST_EDGE_CASES): test_edge_cases.o ../src/bmi_topmodel.o ../src/topmodel.o
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TEST_EDGE_CASES) test_edge_cases.o ../src/bmi_topmodel.o ../src/topmodel.o $(LFLAGS) $(LIBS)

# this is a suffix replacement rule for building .o's from .c's
# it uses automatic variables $<: the name of the prerequisite of
# the rule(a .c file) and $@: the name of the target of the rule (a .o file) 
# (see the gnu make manual section about automatic variables)
.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) *.o *~ $(ALL_TESTS) *.gcda *.gcno *.gcov ../*.gcda ../*.gcno ../*.gcov ../src/*.gcda ../src/*.gcno ../src/*.gcov

depend: $(SRCS)
	makedepend $(INCLUDES) $^ test_array_alloc.c test_shift_q.c test_topmodel_calcs.c test_multi_step.c test_edge_cases.c

autotest: clean all
	@echo "Running all unit tests..."
	@exit_code=0; \
	for test in $(ALL_TESTS); do \
	  echo ""; \
	  echo "Running $$test..."; \
	  if ./$$test > /dev/null 2>&1; then \
	    echo "✓ $$test PASSED"; \
	  else \
	    test_exit=$$?; \
	    echo "✗ $$test FAILED (exit code: $$test_exit)" >&2; \
	    exit_code=$$test_exit; \
	  fi; \
	done; \
	if [ $$exit_code -eq 0 ]; then \
	  echo ""; \
	  echo "All tests passed!"; \
	fi; \
	exit $$exit_code
