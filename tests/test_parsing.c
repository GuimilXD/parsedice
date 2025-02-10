#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>

#define PARSEDICE_IMPLEMENTATION
#include "parsedice.h"

static bool string_slice_compare(StringSlice s, const char *cstr) {
  return strncmp(s.start, cstr, s.length) == 0;
}

void test_simple_dice(void) {
  const char *input_str = "1d4";

  ParseDiceExpression e = parsedice_parse_string(input_str);

  parsedice_expression_print_errors(input_str, e);

  assert(e.length == 1);

  ParserItem item = e.items[0];
  assert(item.type == ParserDiceType);
  assert(item.dice.amount == 1);
  assert(item.dice.faces == 4);

  parsedice_expression_destroy(&e);
}

void test_dice_error_cases(void) {
  {
    const char *input_str = "1d-";

    ParseDiceExpression e = parsedice_parse_string(input_str);

    assert(e.length == 1);

    ParserItem item = e.items[0];
    assert(item.type == ParserErrorType);
    assert(item.error.type == ParserErrorExpectedInt);
    assert(string_slice_compare(item.error.stopped_at, "-"));

    parsedice_expression_destroy(&e);
  }
  {
    const char *input_str = "Xd2";

    ParseDiceExpression e = parsedice_parse_string(input_str);

    assert(e.length == 1);

    ParserItem item = e.items[0];
    assert(item.type == ParserErrorType);
    assert(item.error.type == ParserErrorNoMatches);
    assert(string_slice_compare(item.error.stopped_at, "Xd2"));

    parsedice_expression_destroy(&e);
  }
}

void test_ignore_spaces(void) {
  const char *input_str = "  3d8+   2d4  ";

  ParseDiceExpression e = parsedice_parse_string(input_str);

  parsedice_expression_print_errors(input_str, e);

  assert(e.length == 3);

  assert(e.items[0].type == ParserDiceType);
  assert(e.items[0].dice.faces == 8);
  assert(e.items[0].dice.amount == 3);

  assert(e.items[1].type == ParserOperationType);
  assert(e.items[1].operation == ParserOperationAdd);

  assert(e.items[2].type == ParserDiceType);
  assert(e.items[2].dice.faces == 4);
  assert(e.items[2].dice.amount == 2);

  parsedice_expression_destroy(&e);
}

void test_simple_const_num(void) {
  const char *input_str = " 32.3 +50 -120 ";

  ParseDiceExpression e = parsedice_parse_string(input_str);

  parsedice_expression_print_errors(input_str, e);

  assert(e.length == 5);
  assert(e.items[0].number == 32.3f);
  assert(e.items[1].type == ParserOperationType);
  assert(e.items[2].number == 50);
  assert(e.items[3].type == ParserOperationType);
  assert(e.items[4].number == 120);

  parsedice_expression_destroy(&e);
}

void test_complex_parsing(void) {
  const char *input_str = "2d4 + -2 * 3";

  ParseDiceExpression e = parsedice_parse_string(input_str);

  parsedice_expression_print_errors(input_str, e);

  assert(e.length == 6);

  assert(e.items[0].type == ParserDiceType);
  assert(e.items[1].type == ParserOperationType);
  assert(e.items[2].type == ParserOperationType);
  assert(e.items[3].type == ParserConstNumType);
  assert(e.items[4].type == ParserOperationType);
  assert(e.items[5].type == ParserConstNumType);

  assert(e.items[0].dice.amount == 2);
  assert(e.items[0].dice.faces == 4);

  assert(e.items[1].operation == ParserOperationAdd);

  assert(e.items[2].operation == ParserOperationSub);

  assert(e.items[3].number == 2);

  assert(e.items[4].operation == ParserOperationMul);

  assert(e.items[5].number == 3);

  parsedice_expression_destroy(&e);
}

void test_parethesis_parsing(void) {
  const char *input_str = "(1d4 + 2) * 2";

  ParseDiceExpression e = parsedice_parse_string(input_str);

  parsedice_expression_print_errors(input_str, e);

  assert(e.length == 7);

  assert(e.items[0].type == ParserOpenParenthesisType);
  assert(e.items[1].type == ParserDiceType);
  assert(e.items[2].type == ParserOperationType);
  assert(e.items[3].type == ParserConstNumType);
  assert(e.items[4].type == ParserCloseParenthesisType);
  assert(e.items[5].type == ParserOperationType);
  assert(e.items[6].type == ParserConstNumType);

  parsedice_expression_destroy(&e);
}

void test_parser_item_stack() {
  ParserItemStack *s = parser_item_stack_create();

  parser_item_stack_push(s,
                         (ParserItem){.type = ParserDiceType, .dice = {2, 3}});

  parser_item_stack_push(
      s, (ParserItem){.type = ParserConstNumType, .number = 2.0f});

  assert(s->length == 2);
  assert(parser_item_stack_pop(s).type == ParserConstNumType);
  assert(s->length == 1);
  assert(parser_item_stack_pop(s).type == ParserDiceType);
  assert(s->length == 0);

  assert(parser_item_stack_pop(s).type == ParserNullType);
  assert(s->length == 0);

  parser_item_stack_destroy(s);
}

int main(void) {
  test_simple_dice();
  test_dice_error_cases();
  test_ignore_spaces();
  test_simple_const_num();
  test_complex_parsing();
  test_parethesis_parsing();

  test_parser_item_stack();

  const char *input_str = "3d6 + 1d2 * 2";

  ParseDiceExpression e = parsedice_parse_string(input_str);

  for (size_t i = 0; i < e.length; i++) {
    ParserItem token = e.items[i];

    if (token.type == ParserDiceType) {
        ParserConstNum results[token.dice.amount];
        ParserConstNum roll =  parsedice_dice_roll(token.dice, results);

        printf("[");
        for (size_t i = 0; i < token.dice.amount; i++) {
            printf("%.0f", results[i]);

            if (i != token.dice.amount - 1) {
                printf(", ");
            }
        }
        printf("] ");

        e.items[i] = (ParserItem){
            .type = ParserConstNumType,
            .number = roll,
        };
    }

    parsedice_parser_item_print(token);
    printf(" ");
  }

  parsedice_expression_print_errors(input_str, e);

  ParseDiceExpression postfix = parsedice_expression_to_postfix(e);

  parsedice_expression_print_errors(input_str, postfix);

  ParserItem output = parsedice_expression_evaluate_postfix(postfix);

  printf("= ");
  parsedice_parser_item_print(output);
  printf("\n");

  parsedice_expression_destroy(&e);
  parsedice_expression_destroy(&postfix);
}
