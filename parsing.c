#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

#include <editline/readline.h>
#include <editline/history.h>

/* lval can be a number or an error */
typedef struct {
  int type;
  long num;
  int err;
} lval;

/* Enumeration of possible lval types */
enum { LVAL_ERR, LVAL_NUM };

/* Enumerate possible error types */
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

/* Create number type lval */
lval lval_num(long x) {
  lval v;
  v.type = LVAL_NUM;
  v.num = x;
  return v;
}

/* Create a error type lval */
lval lval_err(int e) {
  lval v;
  v.type = LVAL_ERR;
  v.err = e;
  return v;
}

void lval_print(lval v) {
  switch (v.type) {
  case LVAL_NUM: printf("%li", v.num); break;
  case LVAL_ERR:
    {
      switch (v.err) {
      case LERR_DIV_ZERO: printf("Error: Division by zero"); break;
      case LERR_BAD_OP: printf("Error: Invalid operator"); break;
      case LERR_BAD_NUM: printf("Error: Invalid number"); break;
      }
    }
  }
}

void lval_println(lval v) { lval_print(v); putchar('\n'); }

lval eval_op_single(char *op, lval x) {
  if (x.type == LVAL_ERR) { return x; }

  if (strcmp(op, "-") == 0) return lval_num(x.num * -1);
  return lval_err(LERR_BAD_OP);
}

lval eval_op(char *op, lval x, lval y) {
  if (x.type == LVAL_ERR) { return x; }
  if (y.type == LVAL_ERR) { return y; }

  if (strcmp(op, "+") == 0) return lval_num(x.num + y.num);
  if (strcmp(op, "-") == 0) return lval_num(x.num - y.num);
  if (strcmp(op, "*") == 0) return lval_num(x.num * y.num);
  if (strcmp(op, "/") == 0) {
    return y.num == 0
      ? lval_err(LERR_DIV_ZERO)
      : lval_num(x.num / y.num);
  }
  if (strcmp(op, "%") == 0) return lval_num(x.num % y.num);
  if (strcmp(op, "^") == 0) {
    long acc = y.num;
    for (int i = 1; i < x.num; i++) {
      acc *= y.num;
    }
    return lval_num(acc);
  }
  if (strcmp(op, "min") == 0) {
    if (x.num < y.num) {
      return x;
    } else {
      return y;
    }
  }
  if (strcmp(op, "max") == 0) {
    if (x.num > y.num) {
      return x;
    } else {
      return y;
    }
  }
  return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t *t) {

  /* If tagged as a number, return it */
  if (strstr(t->tag, "number")) {
    errno = 0;
    long n = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(n) : lval_err(LERR_BAD_NUM);
  }

  /* The operator is always the second child */
  char *op = t->children[1]->contents;

  /* We store the third child in `x` */
  lval x = eval(t->children[2]);

  /* eval_op with only one argument */
  if (t->children_num < 5) {
    x = eval_op_single(op, x);
  } else {
    /* Iterate the remaining children and combine */
    int i = 3;
    while (strstr(t->children[i]->tag, "expr")) {
      x = eval_op(op, x, eval(t->children[i]));
      i++;
    }
  }

  return x;
}

int main() {

  /* Create some parsers */
  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Operator = mpc_new("operator");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Lispy = mpc_new("lispy");

  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                    \
      number    : /-?[0-9]+/;                            \
      operator  : '+' | '-' | '*' | '/' | '%' | '^' | \"min\" | \"max\"; \
      expr      : <number> | '(' <operator> <expr>+ ')'; \
      lispy     : /^/ <operator> <expr>+ /$/;            \
    ",
    Number, Operator, Expr, Lispy);

  puts("Lispy version 0.0.0.0.2");
  puts("Press Ctrl-C to exit\n");

  while (1) {

    char *input = readline("lispy> ");
    add_history(input);
    
    /* Attempt to parse the user input */
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      lval result = eval(r.output);
      lval_println(result);
      mpc_ast_delete(r.output);
    } else {
      /* Otherwise print and delete the error */
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    free(input);
  }

  /* Undefine and delete our parsers */
  mpc_cleanup(4, Number, Operator, Expr, Lispy);

  return 0;
}
