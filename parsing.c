#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

#include <editline/readline.h>
#include <editline/history.h>

/*** macros ***/

#define LASSERT(args, cond, err) \
  if (!cond) { lval_free(args); return lval_err(err); }

/* lval can be a number or an error */
typedef struct lval {
  int type;
  long num;
  char *err;
  char *sym;
  /* Count and pointer to a list of lval* */
  int count;
  struct lval **cell;
} lval;

enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };

/*** method declarations ***/
void lval_print(lval *v);
void lval_del(lval *v);
lval *lval_add(lval *v, lval *x);
lval* lval_eval(lval *v);
lval* lval_join(lval *x, lval *y);

/* Create a pointer to number type lval */
lval* lval_num(long x) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

/* Create a pointer to error type lval */
lval* lval_err(char *msg) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  v->err = malloc(strlen(msg) + 1);
  strcpy(v->err, msg);
  return v;
}

/* Create pointer to a Symbol lval */
lval* lval_sym(char* sym) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(sym) + 1);
  strcpy(v->sym, sym);
  return v;
}

/* Create pointer to a Sexpr lval */
lval* lval_sexpr(void) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

/* Create pointer to a Qexpr lval */
lval* lval_qexpr(void) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

/* Free lval memory */
void lval_free(lval *v) {
  switch (v->type) {
  case LVAL_NUM: break;

    /* For Err or Sym, free the string data */
  case LVAL_ERR: free(v->err); break;
  case LVAL_SYM: free(v->sym); break;

    /* If Sexpr or Qexpr, delete all the elements inside */
  case LVAL_QEXPR:
  case LVAL_SEXPR:
    for (int i = 0; i < v->count; i++) {
      lval_free(v->cell[i]);
    }
    free(v->cell);
    break;
  }

  free(v);
}

lval* lval_read_num(mpc_ast_t *t) {
  errno = 0;
  long x = strtol(t->contents, NULL, 10);
  if (errno != ERANGE) {
    return lval_num(x);
  } else {
    return lval_err("Invalid number");
  }
}

lval* lval_read(mpc_ast_t *t) {

  /* If symbol or Number, return conversion to that type */
  if (strstr(t->tag, "number")) return lval_read_num(t);
  if (strstr(t->tag, "symbol")) return lval_sym(t->contents);

  lval *x = NULL;
  if (strcmp(t->tag, ">") == 0) x = lval_sexpr();
  if (strstr(t->tag, "sexpr")) x = lval_sexpr();
  if (strstr(t->tag, "qexpr")) x = lval_qexpr();

  /* Fill lval sexpr list with any valid expression contained withing */
  for (int i = 0; i < t->children_num; i++) {
    if (strcmp(t->children[i]->contents, "(") == 0) continue;
    if (strcmp(t->children[i]->contents, ")") == 0) continue;
    if (strcmp(t->children[i]->contents, "{") == 0) continue;
    if (strcmp(t->children[i]->contents, "}") == 0) continue;
    if (strcmp(t->children[i]->tag, "regex") == 0) continue;
    x = lval_add(x, lval_read(t->children[i]));
  }
  return x;
}

lval *lval_add(lval *v, lval *x) {
  v->count++;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count - 1] = x;
  return v;
}

lval *lval_pop(lval *v, int i) {
  /* Find item at i */
  lval *x = v->cell[i];

  /* Shift memory after the item ati over the top */
  memmove(&v->cell[i], &v->cell[i + 1], sizeof(lval*) * (v->count - i - 1));

  /* Decrease number of items in list */
  v->count--;

  /* Reallocate memory used */
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  return x;
}

lval *lval_take(lval *v, int i) {
  lval *x = lval_pop(v, i);
  lval_free(v);
  return x;
}

void lval_expr_print(lval *v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; i++) {
    lval_print(v->cell[i]);
    if (i != (v->count - 1)) {
      putchar(' ');
    }
  }
  putchar(close);
}

void lval_print(lval *v) {
  switch(v->type) {
  case LVAL_NUM: printf("%li", v->num); break;
  case LVAL_ERR: printf("Error: %s", v->err); break;
  case LVAL_SYM: printf("%s", v->sym); break;
  case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
  case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
  }
}

void lval_println(lval *v) { lval_print(v); putchar('\n'); }

lval* builtin_op(lval *a, char *op) {

  /* Ensure all arguments are numbers */
  for (int i = 0; i < a->count; i++) {
    if (a->cell[i]->type != LVAL_NUM) {
      lval_free(a);
      return lval_err("Cannot operate on non-number");
    }
  }

  lval *x = lval_pop(a, 0);

  if ((strcmp(op, "-") == 0) && a->count == 0)
    x->num = -x->num;

  while (a->count > 0) {
    lval *y = lval_pop(a, 0);
  if (strcmp(op, "+") == 0) x->num += y->num;
  if (strcmp(op, "-") == 0) x->num -= y->num;
  if (strcmp(op, "*") == 0) x->num *= y->num;
  if (strcmp(op, "/") == 0) {
    if (y->num == 0) {
      lval_free(x);
      lval_free(y);
      x = lval_err("Division by zero");
      break;
    }
    x->num /= y->num;
  }

  lval_free(y);
  }
  lval_free(a);
  return x;
}

lval* builtin_head(lval *a) {
  LASSERT(a, a->count == 1,
	  "Function 'head' passed too many arguments!");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
	  "Function 'head' passed incorrect types!");
  LASSERT(a, a->cell[0]->count != 0,
	  "Function 'head' passed {}!");

  lval *v = lval_take(a, 0);
  while (v->count > 1) { lval_free(lval_pop(v, 1)); }
  return v;
}

lval* builtin_tail(lval *a) {
  LASSERT(a, a->count == 1,
	  "Function 'tail'passed too many arguments!");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
	  "Function 'tail' passed incorrect types!");
  LASSERT(a, a->cell[0]->count != 0,
	  "Function 'tail' passed {}!");

  lval *v = lval_take(a, 0);
  lval_free(lval_pop(v, 0));
  return v;
}

lval* builtin_list(lval *a) {
  a->type = LVAL_QEXPR;
  return a;
}

lval* builtin_eval(lval *a) {
  LASSERT(a, a->count == 1,
	  "Function 'eval' passed too many arguments!");
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
	  "Function 'eval' passed incorrect types!");

  lval *x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(x);
}

lval* builtin_join(lval *a) {

  for (int i = 0; i < a->count; i++) {
    LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
	    "Function 'join' passed incorrect type!");
  }

  lval *x = lval_pop(a, 0);
  while (a->count) {
    x = lval_join(x, lval_pop(a, 0));
  }

  lval_free(a);
  return x;
}

lval* builtin(lval *a, char* func) {
  if (strcmp("list", func) == 0) return builtin_list(a);
  if (strcmp("head", func) == 0) return builtin_head(a);
  if (strcmp("tail", func) == 0) return builtin_tail(a);
  if (strcmp("join", func) == 0) return builtin_join(a);
  if (strcmp("eval", func) == 0) return builtin_eval(a);
  if (strstr("+-/*", func)) return builtin_op(a, func);
  lval_free(a);
  return lval_err("Unknown function");
}
lval* lval_join(lval *x, lval *y) {
  /* FOr each cell i 'y', add it to 'x' */
  while (y->count) {
    x = lval_add(x, lval_pop(y, 0));
  }

  lval_free(y);
  return x;
}

lval* lval_eval_sexpr(lval *v) {

  for (int i = 0; i < v->count; i++) {
    v->cell[i] = lval_eval(v->cell[i]);
  }

  for (int i = 0; i < v->count; i++) {
    if(v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
  }

  if (v->count == 0) return v;

  if (v->count == 1) return lval_take(v, 0);

  lval *f = lval_pop(v, 0);
  if (f->type != LVAL_SYM) {
    lval_free(f);
    lval_free(v);
    return lval_err("S-expression does not start with symbol.");
  }

  lval *result = builtin(v, f->sym);
  lval_free(f);
  return result;
}

lval *lval_eval(lval *v) {
  if (v->type == LVAL_SEXPR) return lval_eval_sexpr(v);
  else return v;
}

int main() {

  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Symbol = mpc_new("symbol");
  mpc_parser_t *Sexpr = mpc_new("sexpr");
  mpc_parser_t *Qexpr = mpc_new("qexpr");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Lispy = mpc_new("lispy");

  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                    \
      number : /-?[0-9]+/;                               \
      symbol : \"list\" | \"head\" | \"tail\"            \
             | \"join\" | \"eval\" | '+' | '-' | '*' | '/' ; \
      sexpr  : '(' <expr>* ')' ;                         \
      qexpr  : '{' <expr>* '}' ;                         \
      expr   : <number> | <symbol> | <sexpr> | <qexpr> ; \
      lispy  : /^/ <expr>* /$/;                          \
    ",
    Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

  puts("Lispy version 0.0.0.0.2");
  puts("Press Ctrl-C to exit\n");

  while (1) {

    char *input = readline("lispy> ");
    add_history(input);
    
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      lval *x = lval_eval(lval_read(r.output));
      lval_println(x);
      lval_free(x);
      mpc_ast_delete(r.output);
    } else {
      /* Otherwise print and delete the error */
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    free(input);
  }

  /* Undefine and delete our parsers */
  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

  return 0;
}
