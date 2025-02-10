#include <assert.h>

#define PARSEDICE_IMPLEMENTATION
#include "parsedice.h"


void test_expression(void) {
  ParseDiceExpression e = parsedice_expression_create();

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

  parsedice_expression_destroy(&e);

  assert(e.length == 0);
  assert(e.capacity == 0);
  assert(e.items == NULL);
}

void test_expression_is_balanced(void) {
  {
    const char *input_str = "((3d8 + 2) - 2) * 2d4";

    ParseDiceExpression e = parsedice_parse_string(input_str);

    parsedice_expression_print_errors(input_str, e);

    assert(parsedice_expression_is_balanced(e) == true);

    parsedice_expression_destroy(&e);
  }

  {
    const char *input_str = "((3d8 + 2)) - 2) * 2d4)";

    ParseDiceExpression e = parsedice_parse_string(input_str);

    parsedice_expression_print_errors(input_str, e);

    assert(parsedice_expression_is_balanced(e) == false);

    parsedice_expression_destroy(&e);
  }

  {
    const char *input_str = "((((3d8 + 2) - ) 2) * 2d4";

    ParseDiceExpression e = parsedice_parse_string(input_str);

    parsedice_expression_print_errors(input_str, e);

    assert(parsedice_expression_is_balanced(e) == false);

    parsedice_expression_destroy(&e);
  }
}

void test_expression_to_postfix(void) {
  {
    const char *input_str = "3d6 - 2 * 10";
    // In postfix: "3d6 2 10 * -"

    ParseDiceExpression e = parsedice_parse_string(input_str);

    parsedice_expression_print_errors(input_str, e);

    ParseDiceExpression postfix = parsedice_expression_to_postfix(e);

    parsedice_expression_print_errors(input_str, postfix);

    assert(postfix.items[0].type == ParserDiceType);

    assert(postfix.items[1].type == ParserConstNumType);
    assert(postfix.items[1].number == 2);

    assert(postfix.items[2].type == ParserConstNumType);
    assert(postfix.items[2].number == 10);

    assert(postfix.items[3].type == ParserOperationType);
    assert(postfix.items[3].operation == ParserOperationMul);

    assert(postfix.items[4].type == ParserOperationType);
    assert(postfix.items[4].operation == ParserOperationSub);

    parsedice_expression_destroy(&e);
    parsedice_expression_destroy(&postfix);
  }
  {
    const char *input_str = "(3d6 - 2) * 10";
    // In postfix: "3d6 2 - 10 *"

    ParseDiceExpression e = parsedice_parse_string(input_str);

    parsedice_expression_print_errors(input_str, e);

    ParseDiceExpression postfix = parsedice_expression_to_postfix(e);

    parsedice_expression_print_errors(input_str, postfix);

    assert(postfix.items[0].type == ParserDiceType);

    assert(postfix.items[1].type == ParserConstNumType);
    assert(postfix.items[1].number == 2);

    assert(postfix.items[2].type == ParserOperationType);
    assert(postfix.items[2].operation == ParserOperationSub);

    assert(postfix.items[3].type == ParserConstNumType);
    assert(postfix.items[3].number == 10);

    assert(postfix.items[4].type == ParserOperationType);
    assert(postfix.items[4].operation == ParserOperationMul);

    parsedice_expression_destroy(&e);
    parsedice_expression_destroy(&postfix);
  }
}

void test_expression_evaluate_postfix() {
  const char *input_str = "20 10 * 2 2 + /";

  ParseDiceExpression e = parsedice_parse_string(input_str);

  parsedice_expression_print_errors(input_str, e);

  ParserItem output = parsedice_expression_evaluate_postfix(e);

  assert(output.type == ParserConstNumType);
  assert(output.number == 50);

  parsedice_expression_destroy(&e);
}

int main(void) {
  test_expression();
  test_expression_is_balanced();
  test_expression_to_postfix();
  test_expression_evaluate_postfix();
}
