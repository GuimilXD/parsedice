#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#define PARSEDICE_IMPLEMENTATION
#include "parsedice.h"

void check_error(const char *original_string, ParseDiceExpression e) {
    for (size_t i = 0; i < e.length; ++i) {
        ParserItem item = e.items[i];

        if (item.type != ParserErrorType)
            continue;

        printf("ERROR (%s): \"%s\"\n", parsedice_parse_error_to_string(item.error),
                original_string);
        printf("Stopped at: \"%s\"\n", item.error.stopped_at.start);

    }
}

void print_item(ParserItem i) {
    switch (i.type) {
        case ParserConstNumType:
            printf("%f", i.number);
            break;
        case ParserDiceType:
            printf("%dd%d", i.dice.amount, i.dice.faces);
            break;
        case ParserBinaryOperationType:
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
    }
}

void print_expression(ParseDiceExpression e) {
    for (size_t i = 0; i < e.length; i++) {
        print_item(e.items[i]);
        printf(" ");
    }
    printf("\n");
}

bool compare_string_slice(StringSlice s, const char* cstr) {
    return strncmp(s.start, cstr, s.len) == 0;
}

void test_simple_dice(void) {
  const char *input_str = "1d4";

  ParseDiceExpression e = parsedice_parse_string(input_str);

  check_error(input_str, e);

  assert(e.length == 1);

  ParserItem item = e.items[0];
  assert(item.type == ParserDiceType);
  assert(item.dice.amount == 1);
  assert(item.dice.faces == 4);

  parsedice_expression_free(&e);
}

void test_dice_error_cases(void) {
    // Expect integer after d
    {
        const char *input_str = "1d-";

        ParseDiceExpression e = parsedice_parse_string(input_str);

        assert(e.length == 1);

        ParserItem item = e.items[0];
        assert(item.type == ParserErrorType);
        assert(item.error.type == ParserErrorExpectedInt);
        assert(compare_string_slice(item.error.stopped_at, "-"));

        parsedice_expression_free(&e);
    }
}

void test_ignore_spaces(void) {
  const char *input_str = "  3d8   +   2d4  ";

  ParseDiceExpression e = parsedice_parse_string(input_str);

  check_error(input_str, e);

  assert(e.length == 3);

  assert(e.items[0].type == ParserDiceType);
  assert(e.items[0].dice.faces == 8);
  assert(e.items[0].dice.amount == 3);

  assert(e.items[1].type == ParserBinaryOperationType);
  assert(e.items[1].operation == BinaryOperationAdd);

  assert(e.items[2].type == ParserDiceType);
  assert(e.items[2].dice.faces == 4);
  assert(e.items[2].dice.amount == 2);

  parsedice_expression_free(&e);
}

void test_expression(void) {
  ParseDiceExpression e = parsedice_expression_new();

  parsedice_expression_append(&e, (ParserItem){
                                      .type = ParserDiceType,
                                      .dice = {2, 3},
                                  });

  assert(e.length == 1);
  assert(e.items[0].type == ParserDiceType);
  assert(e.items[0].dice.faces == 3);
  assert(e.items[0].dice.amount == 2);

  parsedice_expression_append(&e, (ParserItem){
                                      .type = ParserDiceType,
                                      .dice = {3, 3},
                                  });

  parsedice_expression_append(&e, (ParserItem){
                                      .type = ParserDiceType,
                                      .dice = {5, 3},
                                  });

  assert(e.length == 3);

  parsedice_expression_free(&e);

  assert(e.length == 0);
  assert(e.capacity == 0);
  assert(e.items == NULL);
}

void test_simple_const_num(void) {
  const char *input_str = "   32.3 +50 -120 ";

  ParseDiceExpression e = parsedice_parse_string(input_str);

  check_error(input_str, e);

  assert(e.length == 3);
  assert(e.items[0].number == 32.3f);
  assert(e.items[1].number == 50);
  assert(e.items[2].number == -120);

  parsedice_expression_free(&e);
}

void test_complex_parsing(void) {
  const char *input_str = "2d4 + -2 * 3";

  ParseDiceExpression e = parsedice_parse_string(input_str);

  check_error(input_str, e);

  assert(e.length == 5);

  assert(e.items[0].type == ParserDiceType);
  assert(e.items[1].type == ParserBinaryOperationType);
  assert(e.items[2].type == ParserConstNumType);
  assert(e.items[3].type == ParserBinaryOperationType);
  assert(e.items[4].type == ParserConstNumType);

  assert(e.items[0].dice.amount == 2);
  assert(e.items[0].dice.faces == 4);

  assert(e.items[1].operation == BinaryOperationAdd);

  assert(e.items[2].number == -2);

  assert(e.items[3].operation == BinaryOperationMul);

  assert(e.items[4].number == 3);

  parsedice_expression_free(&e);
}

void test_parethesis_parsing(void){
  const char *input_str = "(1d4 + 2) * 2";

  ParseDiceExpression e = parsedice_parse_string(input_str);

  check_error(input_str, e);

  assert(e.length == 7);

  assert(e.items[0].type == ParserOpenParenthesisType);
  assert(e.items[1].type == ParserDiceType);
  assert(e.items[2].type == ParserBinaryOperationType);
  assert(e.items[3].type == ParserConstNumType);
  assert(e.items[4].type == ParserCloseParenthesisType);
  assert(e.items[5].type == ParserBinaryOperationType);
  assert(e.items[6].type == ParserConstNumType);

  parsedice_expression_free(&e);
}

int main(void) {
  test_simple_dice();
  test_dice_error_cases();
  test_ignore_spaces();
  test_expression();
  test_simple_const_num();
  test_complex_parsing();
  test_parethesis_parsing();

  const char *input_str = "(1d4 + 2) * 2";

  ParseDiceExpression e = parsedice_parse_string(input_str);

  check_error(input_str, e);

  parsedice_expression_free(&e);
}
