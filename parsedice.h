#ifndef PARSEDICE_H
#define PARSEDICE_H

#include <stdbool.h>
#include <stddef.h>

#define PARSEDICE_DEFAULT_STACK_SIZE 4
#define PARSEDICE_EXPRESSION_DEFAULT_CAPACITY 2

#define PARSEDICE_ARRAY_SIZE(x) ((sizeof x) / (sizeof *x))

typedef struct {
  const char *start;
  size_t length;
} StringSlice;

typedef unsigned int DiceInt;

typedef struct {
  DiceInt amount;
  DiceInt faces;
} Dice;

// When adding a new operation, don't forget to:
// - create mapping on op_mappings
// - add an entry on the precedence_table
// - add a handler to the op_handlers
typedef enum {
  ParserOperationAdd,
  ParserOperationSub,
  ParserOperationMul,
  ParserOperationDiv,
} ParserOperation;

typedef enum {
  ParserDiceType,
  ParserOperationType,
  ParserConstNumType,
  ParserOpenParenthesisType,
  ParserCloseParenthesisType,
  ParserErrorType,
  ParserNullType,
} ParserTypes;

typedef float ParserConstNum;

// Please remember to add a string to error_str array
typedef enum {
  ParserErrorDidNotMatchPattern,
  ParserErrorNoMatches,
  ParserErrorExpectedInt,
} ParserErrorEnum;

typedef struct {
  ParserErrorEnum type;

  StringSlice stopped_at;
} ParserError;

typedef struct {
  ParserTypes type;

  union {
    Dice dice;
    ParserOperation operation;
    ParserError error;
    ParserConstNum number;
  };
} ParserItem;

typedef struct {
  ParserItem *items;
  size_t length;
  size_t capacity;
} ParseDiceExpression;

ParserConstNum parsedice_dice_roll(Dice d, ParserConstNum results[]);

ParseDiceExpression parsedice_parse_string(const char *string);
const char *parsedice_parse_error_to_string(ParserError error);

const char parsedice_operation_to_char(ParserOperation type);

void parsedice_parser_item_print(ParserItem i);

ParseDiceExpression parsedice_expression_create(void);
void parsedice_expression_destroy(ParseDiceExpression *e);
void parsedice_expression_append(ParseDiceExpression *e, ParserItem i);
bool parsedice_expression_is_balanced(ParseDiceExpression ex);
ParseDiceExpression parsedice_expression_to_postfix(ParseDiceExpression e);
ParserItem parsedice_expression_evaluate_postfix(ParseDiceExpression e);
void parsedice_expression_print_errors(const char *original_string,
                                       ParseDiceExpression e);
void parsedice_expression_print(ParseDiceExpression e);

#ifdef PARSEDICE_IMPLEMENTATION
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

ParseDiceExpression parsedice_expression_create(void) {
  return (ParseDiceExpression){
      .capacity = PARSEDICE_EXPRESSION_DEFAULT_CAPACITY,
      .length = 0,
      .items =
          malloc(sizeof(ParserItem) * PARSEDICE_EXPRESSION_DEFAULT_CAPACITY),
  };
}

void parsedice_expression_append(ParseDiceExpression *e, ParserItem i) {
  if (e->length + 1 > e->capacity) {
    size_t create_capacity = e->capacity * 2;

    e->items = realloc(e->items, sizeof(ParserItem) * create_capacity);
    e->capacity = create_capacity;
  }

  e->items[e->length] = i;
  e->length++;
}

void parsedice_expression_destroy(ParseDiceExpression *e) {
  free(e->items);
  e->items = NULL;
  e->capacity = 0;
  e->length = 0;
}

static inline StringSlice string_slice_from_c_str(const char *string) {
  return (StringSlice){
      .start = string,
      .length = strlen(string),
  };
}

static inline void string_slice_skip_characters(StringSlice *ps, int n) {
  if (ps->length < (size_t)n) {
    n = ps->length;
  }

  ps->start += n;
  ps->length -= n;
}

static inline void skip_whitespace(StringSlice *p) {
  while (p->start[0] == ' ' && p->length > 0)
    string_slice_skip_characters(p, 1);
}

static inline bool is_digit(char c) { return c >= '0' && c <= '9'; }

static inline DiceInt parse_dice_int(StringSlice *p) {
  // This stops this function from parsing -XdX or +XdX as a dice.
  // For instance: "+ +3d6", should be parsed as [OperatorAdd, OperatorAdd,
  // Dice]. Not as [OperatorAdd, Dice].
  if (!is_digit(p->start[0])) {
    errno = EINVAL;
    return 0;
  }

  char *end_ptr;

  DiceInt res = strtoul(p->start, &end_ptr, 10);

  if ((end_ptr == p->start) && res == 0) {
    errno = EINVAL;

    return res;
  }

  errno = 0;
  *p = string_slice_from_c_str(end_ptr);

  return res;
}

static inline bool parse_character(StringSlice *ps, char c) {
  if (ps->length <= 0)
    return false;

  if (ps->start[0] != c)
    return false;

  string_slice_skip_characters(ps, 1);

  return true;
}

static ParserItem create_parser_error(StringSlice *p, ParserErrorEnum error) {
  return (ParserItem){
      .type = ParserErrorType,
      .error =
          (ParserError){
              .type = error,
              .stopped_at = *p,
          },
  };
}

static ParserItem parse_dice(StringSlice *p) {
  // We have to save the current state of the parser in case the first int
  // succeds but the rest fails.
  // Example: 1f 1 will be parsed fine, but f will fail.
  StringSlice tmp = *p;

  DiceInt amount = parse_dice_int(p);

  if (errno != 0 || !(parse_character(p, 'd'))) {
    *p = tmp;

    return create_parser_error(p, ParserErrorDidNotMatchPattern);
  }

  DiceInt faces = parse_dice_int(p);

  if (errno != 0)
    return create_parser_error(p, ParserErrorExpectedInt);

  return (ParserItem){
      .type = ParserDiceType,
      .dice =
          (Dice){
              .amount = amount,
              .faces = faces,
          },
  };
}

typedef struct {
  char character;
  ParserOperation type;
} OperatorMapping;

static OperatorMapping op_mappings[] = {
    {'+', ParserOperationAdd},
    {'-', ParserOperationSub},
    {'*', ParserOperationMul},
    {'/', ParserOperationDiv},
};

static ParserItem parse_operation(StringSlice *p) {
  for (size_t i = 0; i < PARSEDICE_ARRAY_SIZE(op_mappings); i++) {
    if (parse_character(p, op_mappings[i].character))
      return (ParserItem){
          .type = ParserOperationType,
          .operation = op_mappings[i].type,
      };
  }

  return create_parser_error(p, ParserErrorDidNotMatchPattern);
}

static inline ParserItem parse_const_num(StringSlice *p) {
  char *end_ptr;

  ParserConstNum res = strtof(p->start, &end_ptr);

  if ((end_ptr == p->start) && res == 0)
    return create_parser_error(p, ParserErrorDidNotMatchPattern);

  *p = string_slice_from_c_str(end_ptr);

  return (ParserItem){
      .type = ParserConstNumType,
      .number = res,
  };
}

static ParserItem parse_parenthesis(StringSlice *p) {
  if (parse_character(p, '('))
    return (ParserItem){.type = ParserOpenParenthesisType};

  if (parse_character(p, ')'))
    return (ParserItem){.type = ParserCloseParenthesisType};

  return create_parser_error(p, ParserErrorDidNotMatchPattern);
}

static ParserItem (*parsers[])(StringSlice *p) = {
    parse_parenthesis, parse_operation, parse_dice, parse_const_num};
static ParserItem parse_item(StringSlice *p) {
  for (size_t i = 0; i < sizeof(parsers) / sizeof(parsers[0]); ++i) {
    skip_whitespace(p);

    ParserItem item = parsers[i](p);

    skip_whitespace(p);

    if (item.type == ParserErrorType &&
        item.error.type == ParserErrorDidNotMatchPattern)
      continue;
    else
      return item;
  }

  return create_parser_error(p, ParserErrorNoMatches);
}

// Very fast random number generator, credits to Eskil Steenberg.
// (http://www.gamepipeline.org/forge.html) Range is between 0 and 1
static float randf(uint index) {
  index = (index << 13) ^ index;
  return (((index * (index * index * 15731 + 789221) + 1376312589) &
           0x7fffffff) /
          1073741824.0f) *
         0.5f;
}

static unsigned int generate_seed() {
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (unsigned int)(ts.tv_nsec ^ ts.tv_sec);
}

ParserConstNum parsedice_dice_roll(Dice d, ParserConstNum results[]) {
  static bool seeded = false;

  if (!seeded) {
    srand(generate_seed());
    seeded = true;
  }

  ParserConstNum res = 0;

  for (size_t i = 0; i < d.amount; i++) {
    ParserConstNum r = (uint)(rand() % d.faces + 1);

    assert(r >= 0);

    if (results != NULL)
      results[i] = r;
    res += (r);
  }

  return res;
}

ParseDiceExpression parsedice_parse_string(const char *string) {
  StringSlice p = string_slice_from_c_str(string);

  ParseDiceExpression e = parsedice_expression_create();

  ParserItem item;

  do {
    item = parse_item(&p);

    if (item.type == ParserErrorType &&
        item.error.type == ParserErrorDidNotMatchPattern)
      return e;

    parsedice_expression_append(&e, item);
  } while (p.length > 0 && item.type != ParserErrorType);

  return e;
}

static const char *const error_str[] = {
    [ParserErrorExpectedInt] = "Expected Int",
    [ParserErrorDidNotMatchPattern] =
        "This error should never be logged, internal error",
    [ParserErrorNoMatches] = "No types have matched, please check your input",
};

const char *parsedice_parse_error_to_string(ParserError error) {
  return error_str[error.type];
}

const char parsedice_operation_to_char(ParserOperation type) {
  for (size_t i = 0; i < PARSEDICE_ARRAY_SIZE(op_mappings); i++) {
    if (type == op_mappings[i].type)
      return op_mappings[i].character;
  }

  return '?';
}

typedef struct {
  size_t length;
  size_t capacity;
  ParserItem items[];
} ParserItemStack;

static ParserItemStack *parser_item_stack_create() {
  ParserItemStack *s =
      malloc(sizeof(ParserItemStack) +
             PARSEDICE_DEFAULT_STACK_SIZE * sizeof(ParserItem));
  s->length = 0;
  s->capacity = PARSEDICE_DEFAULT_STACK_SIZE;

  return s;
}

static void parser_item_stack_destroy(ParserItemStack *s) {
  free(s);

  s = NULL;
}

static void parser_item_stack_push(ParserItemStack *s, ParserItem i) {
  if (s == NULL) {
    printf(
        "ERROR: Attempt to push to NULL pointer. Possible use after free bug.");

    return;
  }

  if (s->length + 1 > s->capacity) {
    s->capacity *= 2;
    s = realloc(s, sizeof(ParserItemStack) + sizeof(ParserItem) * s->capacity);
  }

  s->items[s->length] = i;
  s->length++;
}

static ParserItem parser_item_stack_pop(ParserItemStack *s) {
  if (s->length <= 0)
    return (ParserItem){.type = ParserNullType};

  return s->items[--s->length];
}

static ParserItem parser_item_stack_peek(ParserItemStack *s) {
  if (s->length <= 0)
    return (ParserItem){.type = ParserNullType};

  return s->items[s->length - 1];
}

bool parsedice_expression_is_balanced(ParseDiceExpression ex) {
  ParserItemStack *s = parser_item_stack_create();

  bool is_balanced = true;

  ParserItem item;
  for (size_t i = 0; i < ex.length; ++i) {
    item = ex.items[i];

    if (item.type == ParserOpenParenthesisType) {
      parser_item_stack_push(s, item);
    }

    if (item.type == ParserCloseParenthesisType) {
      if (parser_item_stack_pop(s).type == ParserNullType) {
        is_balanced = false;
        goto end;
      }
    }
  }

  if (s->length != 0)
    is_balanced = false;

end:
  parser_item_stack_destroy(s);
  return is_balanced;
}

static const uint precedence_table[] = {
    [ParserOperationAdd] = 1,
    [ParserOperationSub] = 1,
    [ParserOperationMul] = 2,
    [ParserOperationDiv] = 2,
};

// https://compileralchemy.substack.com/p/step-by-step-parsing-of-mathematical
ParseDiceExpression parsedice_expression_to_postfix(ParseDiceExpression e) {
  ParserItemStack *operator_stack = parser_item_stack_create();

  ParseDiceExpression output = parsedice_expression_create();

  for (size_t i = 0; i < e.length; i++) {
    ParserItem token = e.items[i];

    switch (token.type) {
    case ParserDiceType:
    case ParserConstNumType:
      parsedice_expression_append(&output, token);
      break;
    case ParserOperationType:
      if (operator_stack->length > 0) {
        ParserItem on_top = parser_item_stack_peek(operator_stack);

        if (on_top.type == ParserOpenParenthesisType) {
          parser_item_stack_push(operator_stack, token);
        } else if (precedence_table[on_top.operation] >=
                   precedence_table[token.operation]) {
          parsedice_expression_append(&output,
                                      parser_item_stack_pop(operator_stack));

          parser_item_stack_push(operator_stack, token);
        } else {
          parser_item_stack_push(operator_stack, token);
        }
      } else {
        parser_item_stack_push(operator_stack, token);
      }
      break;
    case ParserOpenParenthesisType:
      parser_item_stack_push(operator_stack, token);
      break;
    case ParserCloseParenthesisType:
      while (operator_stack->length > 0) {
        ParserItem on_top = parser_item_stack_peek(operator_stack);

        if (on_top.type == ParserOpenParenthesisType) {
          parser_item_stack_pop(operator_stack);
          break;
        } else {
          parsedice_expression_append(&output,
                                      parser_item_stack_pop(operator_stack));
        }
      }
      break;
    default:
      parser_item_stack_push(operator_stack, token);
      break;
    }
  }

  while (operator_stack->length > 0) {
    parsedice_expression_append(&output, parser_item_stack_pop(operator_stack));
  }

  parser_item_stack_destroy(operator_stack);

  return output;
}

static ParserConstNum handle_add(ParserConstNum left, ParserConstNum right) {
  return left + right;
}

static ParserConstNum handle_sub(ParserConstNum left, ParserConstNum right) {
  return left - right;
}

static ParserConstNum handle_mul(ParserConstNum left, ParserConstNum right) {
  return left * right;
}

static ParserConstNum handle_div(ParserConstNum left, ParserConstNum right) {
  // TODO: Handle divide by zero
  return left / right;
}

static const ParserConstNum (*op_handlers[])(ParserConstNum, ParserConstNum) = {
    [ParserOperationAdd] = handle_add,
    [ParserOperationSub] = handle_sub,
    [ParserOperationMul] = handle_mul,
    [ParserOperationDiv] = handle_div,
};

static ParserItem handle_operation(ParseDiceExpression e, size_t idx, ParserItemStack *s, ParserOperation op) {
  ParserItem right = parser_item_stack_pop(s);
  ParserItem left = parser_item_stack_pop(s);

  return (ParserItem){.type = ParserConstNumType,
                      .number = op_handlers[op](left.number, right.number)};
}

ParserItem parsedice_expression_evaluate_postfix(ParseDiceExpression e) {
  ParserItemStack *s = parser_item_stack_create();

  for (size_t i = 0; i < e.length; ++i) {
    ParserItem token = e.items[i];

    switch (token.type) {
    case ParserConstNumType:
      parser_item_stack_push(s, token);
      break;
    case ParserDiceType:
      parser_item_stack_push(
          s, (ParserItem){.type = ParserConstNumType,
                          .number = parsedice_dice_roll(token.dice, NULL)});
      break;
    case ParserOperationType:
      parser_item_stack_push(s, handle_operation(e, i, s, token.operation));
      break;
    }
  }

  // assert(s->length == 1);

  ParserItem res = parser_item_stack_pop(s);
  parser_item_stack_destroy(s);

  return res;
}

// TODO: implement better error printing
void parsedice_expression_print_errors(const char *original_string,
                                       ParseDiceExpression e) {
  for (size_t i = 0; i < e.length; ++i) {
    ParserItem item = e.items[i];

    if (item.type != ParserErrorType)
      continue;

    printf("ERROR (%s): \"%s\"\n", parsedice_parse_error_to_string(item.error),
           original_string);
    printf("Stopped at: \"%s\"\n", item.error.stopped_at.start);
  }
}

void parsedice_parser_item_print(ParserItem i) {
  switch (i.type) {
  case ParserConstNumType:
    printf("%.0f", i.number);
    break;
  case ParserDiceType:
    printf("%dd%d", i.dice.amount, i.dice.faces);
    break;
  case ParserOperationType:
    putchar(parsedice_operation_to_char(i.operation));
    break;
  case ParserOpenParenthesisType:
    printf("(");
    break;
  case ParserCloseParenthesisType:
    printf(")");
    break;
  case ParserErrorType:
    printf("ERROR");
    break;
  case ParserNullType:
    printf("NULL");
    break;
  }
}

void parsedice_expression_print(ParseDiceExpression e) {
  for (size_t i = 0; i < e.length; i++) {
    parsedice_parser_item_print(e.items[i]);
    printf(" ");
  }
  printf("\n");
}

#endif

#endif
