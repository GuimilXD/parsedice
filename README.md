# ParseDice 🎲

**ParseDice** is a lightweight, efficient, **STB-style single-header** C library for parsing and evaluating dice expressions. Whether you're developing a tabletop RPG tool, a dice roller, or a game system that requires dice mechanics, **ParseDice** provides a powerful yet simple API to handle dice expressions with mathematical operations.

## Features 🚀

- ✅ **Single-header, STB-style** (just include `parsedice.h`)
- ✅ **Parse standard dice notation** (e.g., `3d6`, `1d20 + 5`, `(2d4 + 3) * 2`)
- ✅ **Supports math operations** (`+`, `-`, `*`, `/`)
- ✅ **Evaluates expressions correctly**
- ✅ **Error handling for invalid expressions**
- ✅ **Minimal dependencies, easy to integrate**

---

## Installation 📦

Since **ParseDice** is a **single-header library**, you only need `parsedice.h`.

To use it in your project, **define `PARSEDICE_IMPLEMENTATION` in exactly one `.c` file** (most likely your main.c file) before including the header:

```c
#define PARSEDICE_IMPLEMENTATION
#include "parsedice.h"
```

## Usage 📝
### Parsing and Evaluating a Dice Expression

```c
#include <stdio.h>
#define PARSEDICE_IMPLEMENTATION
#include "parsedice.h"

int main(void) {
  const char *input_str = "3d6 + 1";

  ParseDiceExpression e = parsedice_parse_string(input_str);
  // Print errors if any
  parsedice_expression_print_errors(input_str, e);

  // Evaluate expression
  ParserItem result = parsedice_expression_evaluate(e);

  printf("Result: ");
  parsedice_parser_item_print(result);
  printf("\n");

  // Cleanup
  parsedice_expression_destroy(&e);

  return 0;
}
```

### Example Output
```
Result: 14
```

# Testing
The project includes a suite of unit tests to validate the core functionality, including expression parsing, postfix conversion, and evaluation. You can run the tests by just running make:
```
make
```

# License 📜

ParseDice is public domain / MIT licensed—do whatever you want with it! If you use it in a project, a shout-out is always appreciated. 🎲✨
