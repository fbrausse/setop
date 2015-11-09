
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include "array.h"
#include "tnode.h"
#include "tparse.h"
#include "tlex.h"

#define BLANK	" \f\t\r\n"

#ifndef SETOP_DEF_ISEP
# define SETOP_DEF_ISEP		BLANK
# define SETOP_DEF_ISEP_DESC	"non-blank to blank transition"
#elif !defined(SETOP_DEF_ISEP_DESC)
# define SETOP_DEF_ISEP_DESC	XSTR(SETOP_DEF_ISEP)
#endif

#ifndef SETOP_DEF_OSEP
# define SETOP_DEF_OSEP		","
#endif
#ifndef SETOP_DEF_OSEP_DESC
# define SETOP_DEF_OSEP_DESC	XSTR(SETOP_DEF_OSEP)
#endif

#define USAGE	"usage: %s [-OPTS] EXPR [[-OPTS] A [[-OPTS] B [...]]]\n"

#define HELP	"\
Options [default]:\n\
  -d ISEP       use ISEP as input field delimiter(s) [" SETOP_DEF_ISEP_DESC "]\n\
  -D OSEP       use OSEP as output field separator [" SETOP_DEF_OSEP_DESC "]\n\
  -e            don't dismiss empty lines [dismiss]\n\
  -h            display this help message\n\
  -t            disable trimming blanks left and right of key [enable]\n\
  -v            print parse tree of EXPR to stderr\n\
\n\
A, B, ... are paths to filenames; optionally any of these can be '-' for stdin.\n\
Output are entries from the lowest numbered input if multiple match.\n\
EXPR is a math expression supporting parenthesis and these constants, both\n\
optionally followed by a FIELDS specification:\n\
\n\
  A             first input set\n\
  B             second input set\n\
  ...           ...\n\
  Z             max. supported input set\n\
\n\
FIELDS is a comma-separated list of integers or colon-separated integer pairs\n\
indicating a range. Fields are 0-based and delimited by SEP. Additionally the\n\
following binary operators are supported, in order of decreasing precedence:\n\
\n\
  ^              symmetric set difference\n\
  &              set intersection\n\
  |              set union\n\
  -              set difference\n\
\n\
Literal sets are supported via the following syntax:\n\
  { \"esc\\\"ape\\\"d\", 'un-esc\"ape\"d', ('a','tuple') }\n\
"
//  { x, (y,z), '#' : (A(x) | B2(x)) & C(y) & {'abc','def'}(z) & !0 }\n

struct iopts {
	char *isep;
	unsigned trim : 1;
	unsigned allow_empty : 1;
};

static int entry_extract(
	struct str *e, const char *line, size_t len, const struct iopts *o
) {
	VARR_DECL_ANON(struct field) f = VARR_INIT;
	e->s = strndup(line, len);

	unsigned field = 0;
	unsigned i = 0;
	while (i <= len) {
		if (o->trim)
			i += strspn(e->s + i, BLANK);
		unsigned fld_len = strcspn(e->s + i, o->isep);
		struct field g = { i, fld_len };
		if (o->trim)
			while (g.len && strchr(BLANK, e->s[i+g.len-1]))
				g.len--;
		varr_append(&f,&g,1,1);
#if DEBUG
		fprintf(stderr, "extracted field %d '%.*s'\n",
			(int)f.valid-1, (int)g.len, e->s + g.from);
#endif
		i += fld_len;
		unsigned sep_len = strchr(o->isep, e->s[i]) ? 1 : 0; // strspn(e->s + i, field_sep);
		i += sep_len;
		if (!sep_len)
			break;
	}
	e->f = f.v;
	e->n = f.valid;

	if (o->allow_empty || e->n)
		return 1;
	free(e->s);
	free(e->f);
	return 0;
}

static void read_input(
	char *fname, char desc, const struct iopts *o,
	struct str_array *r, struct str_array **stdin_data
) {
	int is_stdin = !strcmp(fname, "-");
	FILE *f;

	*r = (struct str_array)VARR_INIT;
	if (is_stdin && *stdin_data) {
		struct str *s;
		varr_append_a(r,*stdin_data,0);
		varr_forall(s,r) {
			s->s = strdup(s->s);
			s->f = memdup(s->f, sizeof(*s->f) * s->n);
		}
		return;
	}

	if (!(f = is_stdin ? stdin : fopen(fname, "r")))
		DIE(1,"error opening '%s' for %c: %s\n",fname,desc,strerror(errno));

	/* read */
	int ret = 0;
	char *line = NULL;
	size_t sz = 0;
	ssize_t len;
	while (errno = 0, (len = getline(&line, &sz, f)) > 0) {
		if (line[len-1] == '\n')
			line[--len] = '\0';
		struct str e;
		if (entry_extract(&e, line, len, o))
			varr_append(r,&e,1,1);
	}
	ret = -errno;
	free(line);
	fclose(f);
	if (ret)
		DIE(1,"error reading '%s' for %c: %s\n",fname,desc,strerror(-ret));

	if (is_stdin)
		*stdin_data = r;
}

int yyparse(struct tnode **expr, yyscan_t scanner, char max_id, struct src_array *sets);

static struct tnode * tnode_parse(char *s, char max_id, struct src_array *sets)
{
	struct tnode *r;
	yyscan_t scanner;
	YY_BUFFER_STATE state;

	if (yylex_init(&scanner))
		DIE(1,"error initializing scanner: %s\n", strerror(errno));
	state = yy_scan_string(s, scanner);
	if (yyparse(&r, scanner, max_id, sets))
		DIE(1,"error parsing '%s'\n", s);
	yy_delete_buffer(state, scanner);
	yylex_destroy(scanner);
	return r;
}

int main(int argc, char **argv)
{
	struct src_array inputs = VARR_INIT;
	struct str_array *stdin_data = NULL;
#if YYDEBUG
	yydebug = 1;
#endif
	char *endptr;
	int   opt;
	int   n;
	int   verbosity = 0;
	char *expr = NULL;
	char *osep = SETOP_DEF_OSEP;
	struct iopts iopts = {
		SETOP_DEF_ISEP,
		1,
		0,
	};
	for (n=-1; optind < argc; n++) {
		while ((opt = getopt(argc, argv, ":d:D:ehtv")) != -1)
			switch (opt) {
			case 'd': iopts.isep = optarg; break;
			case 'D': osep = optarg; break;
			case 'e': iopts.allow_empty = 1; break;
			case 'h': DIE(0,USAGE "\n" HELP,argv[0]);
			case 't': iopts.trim = 0; break;
			case 'v': verbosity++; break;
			case '?': DIE(1,"error: unknown option '-%c'\n",optopt);
			case ':': DIE(1,"error: option '-%c' requires an argument\n",optopt);
			}
		if (optind < argc) {
			if (n < 0)
				expr = argv[optind++];
			else {
				varr_append(&inputs,(&(struct str_array)VARR_INIT),1,1);
				read_input(argv[optind++], MIN_ID+n, &iopts, inputs.v+n, &stdin_data);
			}
		}
	}
	if (!expr)
		DIE(1,USAGE,argv[0]);
	if (n > MAX_IDS)
		DIE(1,"error: max. %d inputs supported\n",MAX_IDS);

	struct tnode *e = tnode_parse(expr, MIN_ID + n - 1, &inputs);
	if (verbosity > 0) {
		tnode_dump(stderr, e);
		fprintf(stderr, "\n");
	}

	struct str_array u = tnode_eval(e, inputs.v);
	struct str *s;
	varr_forall(s,&u) {
		int first = 1;
		for (unsigned i=0; i<s->n; i++)
			if (e->fields & ((fieldmap_t)1 << i)) {
				printf("%s%.*s", first ? "" : osep,
				       (int)s->f[i].len, s->s + s->f[i].from);
				first = 0;
			}
		printf("\n");
	}
	varr_fini(&u);

	struct str_array *t;
	varr_forall(t,&inputs) {
		varr_forall(s,t) {
			free(s->s);
			free(s->f);
		}
		varr_fini(t);
	}
	varr_fini(&inputs);

	tnode_tree_free(e);

	return 0;
}
