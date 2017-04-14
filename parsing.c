#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

#include <editline/readline.h>
#include <editline/history.h>

int main(int argc, char **argv) {

  /* Create some parsers */
  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Operator = mpc_new("operator");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Lispy = mpc_new("lispy");

  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                    \
      number    : /-?[0-9]+/;                            \
      operator  :'+' | '-' | '*' | '/';                  \
      expr      : <number> | '(' <operator> <expr>+ ')'; \
      lispy     : /^/ <operator> <expr>+ /$/;            \
    ",
    Number, Operator, Expr, Lispy);

  puts("Lispy version 0.0.0.0.2");
  puts("Press Ctrl-C to exit\n");

  while (1) {

    char *input = readline("listpy> ");
    add_history(input);
    
    /* Attempt to parse the user input */
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      /* On success, print and delete the AST */
      mpc_ast_print(r.output);
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
