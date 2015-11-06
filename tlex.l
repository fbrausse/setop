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


NUM		[0-9]+

%%

{WS}+				{ /* skip blanks */ }
[A-Z]				{ yylval->cval = yytext[0]; return TOKEN_ID; }
{NUM}				{ yylval->ival = atoi(yytext); return TOKEN_NUM; }
[,:()]				{ return yytext[0]; }
"|"|{CUP}{WS}			{ return '|'; }
"&"|{CAP}{WS}			{ return '&'; }
"-"|{SETMINUS}{WS}		{ return '-'; }
"^"|{OPLUS}{WS}			{ return '^'; }

%%

int yyerror(const char *msg)
{
	fprintf(stderr, "error: %s\n", msg);
	return 0;
}
