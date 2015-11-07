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

static int yyerror(struct tnode **expr, yyscan_t scanner, char max_id, const char *msg)
{
	fprintf(stderr, "error: %s\n", msg);
	return 0;
}

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

%union {
	struct tnode *tnode;
	char cval;
	unsigned ival;
}

%left '-'
%left '|'
%left '&' '^'

%token <cval> TOKEN_ID
%token <ival> TOKEN_NUM

%token TOKEN_COLONEQ
%token TOKEN_LIT_DQ
%token TOKEN_LIT_SQ
%token TOKEN_VAR

%type <tnode> expr
%type <ival> field
%type <ival> field_list
%type <ival> fields

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
	| '{' set_spec '}' fields
	| TOKEN_ID fields {
		if ($1 > max_id || !($$ = tnode_create_id($1, $2))) {
			char buf[128];
			snprintf(buf, sizeof(buf),
			         "ID '%c' too large or wrong number of inputs",
			         $1);
			yyerror(expr,scanner,max_id,buf);
			return -1;
		}
	  }
	;

clause_list
	: clause
	| clause_list ',' clause
	;

clause
	: infix_predicate
	| '!' predicate
	;

predicate
	: '(' infix_predicate ')'
	;

infix_predicate
	: lit '<' lit
	| lit '>' lit
	| lit '=' lit
	;

set_spec
	: %empty
	| lit ':' clause_list
	| tuple_list
	;

tuple_list
	: lit
	| tuple_list ',' lit
	;

lit
	: TOKEN_LIT_SQ
	| TOKEN_LIT_DQ
	| TOKEN_VAR
	| tuple
	;

tuple
	: '(' ')'
	| '(' tuple_list ')'
	;

fields
	: %empty                   { $$ = ~(fieldmap_t)0; }
	| field_list               { $$ = $1; }
	;

field_list
	: field                    { $$ = $1; }
	| field ',' field_list     { $$ = $1 | $3; }
	;

field
	: TOKEN_NUM {
		if ($1 > MAX_FIELD) {
			yyerror(expr,scanner,max_id,"field must be between 0 and " XSTR(MAX_FIELD));
			return -2;
		}
		$$ = tnode_field($1, $1);
	  }
	| TOKEN_NUM ':' TOKEN_NUM {
		if ($1 > MAX_FIELD || $3 > MAX_FIELD) {
			yyerror(expr,scanner,max_id,"field must be between 0 and " XSTR(MAX_FIELD));
			return -2;
		}
		$$ = $1 <= $3 ? tnode_field($1, $3) : tnode_field($3, $1);
	  }
	;

%%
