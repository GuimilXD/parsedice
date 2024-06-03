# Define the compiler
CC = gcc
INCLUDES = -I.

# Define the directory containing the test files
TEST_DIR = tests

# Define the directory to store the compiled executables
BUILD_DIR = build

# Get a list of all C files in the tests directory
TESTS = $(wildcard $(TEST_DIR)/*.c)

# Define the names of the executables by replacing .c with nothing
EXES = $(patsubst $(TEST_DIR)/%.c, $(BUILD_DIR)/%, $(TESTS))

# Default target to compile and run all tests
all: $(EXES) run-tests

# Compile each test file to an executable
$(BUILD_DIR)/%: $(TEST_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(INCLUDES) -o $@ $<

# Run each executable in the build directory
run-tests: $(EXES)
	@for exe in $(EXES); do \
		echo "Running $$exe"; \
		./$$exe; \
	done

# Clean up the build directory
clean:
	rm -rf $(BUILD_DIR)

# PHONY targets to prevent conflicts with files named 'all', 'clean', etc.
.PHONY: all run-tests clean
