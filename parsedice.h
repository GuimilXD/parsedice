#ifndef PARSEDICE_H
#define PARSEDICE_H

#include <stddef.h>

typedef struct {
  const char *start;
  size_t len;
} StringSlice;

typedef unsigned int DiceInt;

typedef struct {
  DiceInt amount;
  DiceInt faces;
} Dice;

typedef enum {
  BinaryOperationAdd,
  BinaryOperationSub,
  BinaryOperationMul,
  BinaryOperationDiv,
} BinaryOperationType;

typedef enum {
  ParserDiceType,
  ParserBinaryOperationType,
  ParserConstNumType,
  ParserOpenParenthesisType,
  ParserCloseParenthesisType,
  ParserErrorType,
} ParserTypes;

typedef float ParserConstNum;

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
    BinaryOperationType operation;
    ParserError error;
    ParserConstNum number;
  };
} ParserItem;

typedef struct {
  ParserItem *items;
  size_t length;
  size_t capacity;
} ParseDiceExpression;

ParseDiceExpression parsedice_parse_string(const char *string);
const char *parsedice_parse_error_to_string(ParserError error);

const char parsedice_operation_to_char(BinaryOperationType os);

ParseDiceExpression parsedice_expression_new(void);
void parsedice_expression_append(ParseDiceExpression *e, ParserItem i);
void parsedice_expression_free(ParseDiceExpression *e);

#ifdef PARSEDICE_IMPLEMENTATION
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define PARSEDICE_EXPRESSION_DEFAULT_CAPACITY 2

#define PARSEDICE_ARRAY_SIZE(x) ((sizeof x) / (sizeof *x))

ParseDiceExpression parsedice_expression_new(void) {
  return (ParseDiceExpression){
      .capacity = PARSEDICE_EXPRESSION_DEFAULT_CAPACITY,
      .length = 0,
      .items =
          malloc(sizeof(ParserItem) * PARSEDICE_EXPRESSION_DEFAULT_CAPACITY),
  };
}

void parsedice_expression_append(ParseDiceExpression *e, ParserItem i) {
  if (e->length + 1 > e->capacity) {
    size_t new_capacity = e->capacity * 2;

    e->items = realloc(e->items, sizeof(ParserItem) * new_capacity);
    e->capacity = new_capacity;
  }

  e->items[e->length] = i;
  e->length++;
}

void parsedice_expression_free(ParseDiceExpression *e) {
  free(e->items);
  e->items = NULL;
  e->capacity = 0;
  e->length = 0;
}

static inline StringSlice string_slice_from_c_str(const char *string) {
  return (StringSlice){
      .start = string,
      .len = strlen(string),
  };
}

static inline void string_slice_skip_characters(StringSlice *ps, int n) {
  if (ps->len < (size_t)n) {
    n = ps->len;
  }

  ps->start += n;
  ps->len -= n;
}

static inline void skip_whitespace(StringSlice *p) {
  while (p->start[0] == ' ' && p->len > 0)
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
  if (ps->len <= 0)
    return false;

  if (ps->start[0] != c)
    return false;

  string_slice_skip_characters(ps, 1);

  return true;
}

static ParserItem new_parser_error(StringSlice *p, ParserErrorEnum error) {
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

    return new_parser_error(p, ParserErrorDidNotMatchPattern);
  }

  DiceInt faces = parse_dice_int(p);

  if (errno != 0)
    return new_parser_error(p, ParserErrorExpectedInt);

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
  BinaryOperationType type;
} OperatorMapping;

static OperatorMapping op_mappings[] = {
    {'+', BinaryOperationAdd},
    {'-', BinaryOperationSub},
    {'*', BinaryOperationMul},
    {'/', BinaryOperationDiv},
};

static ParserItem parse_operation(StringSlice *p) {
  for (size_t i = 0; i < PARSEDICE_ARRAY_SIZE(op_mappings); i++) {
    if (parse_character(p, op_mappings[i].character))
      return (ParserItem){
          .type = ParserBinaryOperationType,
          .operation = op_mappings[i].type,
      };
  }

  return new_parser_error(p, ParserErrorDidNotMatchPattern);
}

static inline ParserItem parse_const_num(StringSlice *p) {
  char *end_ptr;

  ParserConstNum res = strtof(p->start, &end_ptr);

  if ((end_ptr == p->start) && res == 0)
    return new_parser_error(p, ParserErrorDidNotMatchPattern);

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

  return new_parser_error(p, ParserErrorDidNotMatchPattern);
}

static ParserItem (*parsers[])(StringSlice *p) = {
    parse_parenthesis, parse_dice, parse_const_num, parse_operation};
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

  return new_parser_error(p, ParserErrorNoMatches);
}

ParseDiceExpression parsedice_parse_string(const char *string) {
  StringSlice p = string_slice_from_c_str(string);

  ParseDiceExpression e = parsedice_expression_new();

  ParserItem item;

  do {
    item = parse_item(&p);

    if (item.type == ParserErrorType &&
        item.error.type == ParserErrorDidNotMatchPattern)
      return e;

    parsedice_expression_append(&e, item);
  } while (p.len > 0 && item.type != ParserErrorType);

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

const char parsedice_operation_to_char(BinaryOperationType type) {
  for (size_t i = 0; i < PARSEDICE_ARRAY_SIZE(op_mappings); i++) {
    if (type == op_mappings[i].type)
      return op_mappings[i].character;
  }

  return '?';
}
#endif

#endif
