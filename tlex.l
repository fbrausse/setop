%{
/* tlex.l
 *
 * generate lexer via
 *     $ flex tlex.l
 *
 * template by
 * <https://en.wikipedia.org/w/index.php?title=GNU_bison&oldid=675382675>
 */

#include <stdio.h>
#include "tnode.h"
#include "tparse.h"

static char * dequote(const char *s)
{
	size_t len = strlen(s);
	if (len < 2 || s[0] != '"' || s[len-1] != '"')
		return NULL;
	char *r = malloc(len-1);
	size_t p = 0, q = 1;
	int escaped = 0;
	while (q < len-1) {
		char c = s[q++];
		if (escaped) {
			r[p++] = c;
			escaped = 0;
		} else if (c == '\\') {
			escaped = 1;
		} else {
			r[p++] = c;
		}
	}
	r[p] = '\0';
	return r;
}

%}

%option outfile="tlex.c" header-file="tlex.h"
%option warn nodefault
%option reentrant noyywrap never-interactive nounistd
%option bison-bridge

CUP		"\\cup"
CAP		"\\cap"
SETMINUS	"\\setminus"
OPLUS		"\\oplus"
WS		[ \r\n\t]

LIT_DQ		\"([^\\\"]*|\\.)*\"
LIT_SQ		\'[^\']*\'

NUM		[0-9]+

%%

{WS}+				{ /* skip blanks */ }
[A-Z]				{ yylval->cval = yytext[0]; return TOKEN_ID; }
%[a-z]				{ yylval->cval = yytext[0]; return TOKEN_VAR; }
{NUM}				{ yylval->ival = atoi(yytext); return TOKEN_NUM; }
"<="				{ return TOKEN_LEQ; }
">="				{ return TOKEN_GEQ; }
"!="|"<>"			{ return TOKEN_NEQ; }
[,:(){}<>=!]			{ return yytext[0]; }
"|"|{CUP}{WS}			{ return '|'; }
"&"|{CAP}{WS}			{ return '&'; }
"-"|{SETMINUS}{WS}		{ return '-'; }
"^"|{OPLUS}{WS}			{ return '^'; }
{LIT_DQ}			{ yylval->sval = dequote(yytext); return TOKEN_LIT; }
{LIT_SQ}			{ yylval->sval = strndup(yytext+1,strlen(yytext+1)-1); return TOKEN_LIT; }

%%

int yyerror(const char *msg)
{
	fprintf(stderr, "error: %s\n", msg);
	return 0;
}
