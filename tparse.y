%{
/* tparse.y
 * 
 * generate parser via
 *     $ bison tparse.y
 *
 * template by
 * <https://en.wikipedia.org/w/index.php?title=GNU_bison&oldid=675382675>
 */

#include "tnode.h"
#include "tparse.h"
#include "tlex.h"

static int yyerror(struct tnode **expr, yyscan_t scanner, char max_id, struct src_array *sets, const char *msg)
{
	fprintf(stderr, "error: %s\n", msg);
	return 0;
}

static const struct fnode fnode_true = { FNODE_CONST, { { .cnst = 1 }, }, };

%}


%code requires {

#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
typedef void* yyscan_t;
#endif

}


%output "tparse.c"
%defines "tparse.h"

%define api.pure
%lex-param	{ yyscan_t scanner }
%parse-param	{ struct tnode **expr }
%parse-param	{ yyscan_t scanner }
%parse-param	{ char max_id }
%parse-param	{ struct src_array *sets }

%union {
	struct tnode *tnode;
	struct fnode *fnode;
	char cval;
	unsigned ival;
	char *sval;
	struct fnode_arr fnode_arr;
}

%left '-'
%left '|'
%left '&' '^'

%token <cval> TOKEN_ID
%token <ival> TOKEN_NUM

%token TOKEN_COLONEQ

%token <sval> TOKEN_LIT

%token <cval> TOKEN_VAR

%type <tnode> expr

%type <ival> field
%type <ival> field_list
%type <ival> fields
%type <ival> set_spec

%type <fnode> formula
%type <fnode> atomic_formula
%type <fnode> infix_predicate
%type <fnode> lit 

%type <fnode_arr> tuple
%type <fnode_arr> tuple_list
%type <fnode_arr> tuple_list_opt

%%

input
	: expr { *expr = $1; }
	;

/*stmt
	: TOKEN_ID TOKEN_COLONEQ expr ';'
	;*/

expr
	: expr '-' expr       { $$ = tnode_create(TNODE_DIFF, $1, $3); }
	| expr '|' expr       { $$ = tnode_create(TNODE_UNION, $1, $3); }
	| expr '&' expr       { $$ = tnode_create(TNODE_INTERS, $1, $3); }
	| expr '^' expr       { $$ = tnode_create(TNODE_SYMDIFF, $1, $3); }
	| '(' expr ')' fields { $$ = $2; $$->fields = $4; }
	| '{' set_spec '}' fields { $$ = tnode_create_id($2, $4); }
	| TOKEN_ID fields
	{
		if ($1 < MIN_ID || $1 > max_id ||
		    !($$ = tnode_create_id($1 - MIN_ID, $2))) {
			char buf[128];
			snprintf(buf, sizeof(buf),
			         "ID '%c' too large or wrong number of inputs",
			         $1);
			yyerror(expr,scanner,max_id,sets,buf);
			return -1;
		}
	}
	;

set_spec
	: tuple_list ':' formula { $$ = src_create_set(sets, $1, $3); fnode_tree_free($3); }
	| tuple_list_opt         { $$ = src_create_set(sets, $1, &fnode_true); }
	;

formula
	: atomic_formula
	| infix_predicate
	| formula '|' formula    { $$ = fnode_create2(FNODE_OR, $1, $3); }
	| formula '&' formula    { $$ = fnode_create2(FNODE_AND, $1, $3); }
	;

atomic_formula
	: '(' formula ')'        { $$ = $2; }
	| '!' atomic_formula     { $$ = fnode_create1(FNODE_NEG, $2); }
	| TOKEN_NUM              { $$ = fnode_create0($1); }
	| expr '(' TOKEN_VAR ')' { $$ = fnode_create_incl($1, $3); }
	;

infix_predicate
	: lit '<' lit            { $$ = fnode_create2(FNODE_LT, $1, $3); }
	| lit '>' lit            { $$ = fnode_create2(FNODE_GT, $1, $3); }
	| lit '=' lit            { $$ = fnode_create2(FNODE_EQ, $1, $3); }
	;

tuple_list_opt
	:                        { $$ = (struct fnode_arr)VARR_INIT; }
	| tuple_list
	;

tuple_list
	: lit                    { $$ = (struct fnode_arr)VARR_INIT; varr_append(&$$,&$1,1,1); }
	| tuple_list ',' lit     { $$ = $1; varr_append(&$$,&$3,1,1); }
	;

lit
	: TOKEN_LIT              { $$ = fnode_create_lit($1); }
	| TOKEN_VAR              { $$ = fnode_create_var($1); }
	| tuple                  { $$ = fnode_create_tuple($1); }
	;

tuple
	: '(' tuple_list_opt ')'
	{
		$$ = (struct fnode_arr)VARR_INIT;
		varr_append(&$$,&$2,1,1);
		varr_fini(&$2);
	}
	;

fields
	:                          { $$ = ~(fieldmap_t)0; }
	| field_list               { $$ = $1; }
	;

field_list
	: field                    { $$ = $1; }
	| field ',' field_list     { $$ = $1 | $3; }
	;

field
	: TOKEN_NUM {
		if ($1 > MAX_FIELD) {
			yyerror(expr,scanner,max_id,sets,"field must be between 0 and " XSTR(MAX_FIELD));
			return -2;
		}
		$$ = tnode_field($1, $1);
	  }
	| TOKEN_NUM ':' TOKEN_NUM {
		if ($1 > MAX_FIELD || $3 > MAX_FIELD) {
			yyerror(expr,scanner,max_id,sets,"field must be between 0 and " XSTR(MAX_FIELD));
			return -2;
		}
		$$ = $1 <= $3 ? tnode_field($1, $3) : tnode_field($3, $1);
	  }
	;

%%
