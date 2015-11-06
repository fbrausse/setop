
#define _POSIX_C_SOURCE	20151028

#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include "array.h"
#include "tnode.h"
#include "tparse.h"
#include "tlex.h"

#define BLANK	" \t\r\n"

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
  ^  \\oplus     symmetric set difference\n\
  &  \\cap       set intersection\n\
  |  \\cup       set union\n\
  -  \\setminus  set difference\n\
"

struct str {
	char *s;
	struct field { unsigned from, len; } *f; /* len == 0 terminates */
	unsigned n;
};

VARR_DECL(str_array,struct str);

static fieldmap_t  sort_uniq_fields;

static int entry_extract(
	struct str *e, const char *line, size_t len,
	const char *field_sep, int trim
) {
	VARR_DECL_ANON(struct field) f = VARR_INIT;
	e->s = strndup(line, len);

	unsigned field = 0;
	unsigned i = 0;
	while (i <= len) {
		if (trim)
			i += strspn(e->s + i, BLANK);
		unsigned fld_len = strcspn(e->s + i, field_sep);
		struct field g = { i, fld_len };
		if (trim)
			while (g.len && strchr(BLANK, e->s[i+g.len-1]))
				g.len--;
		varr_append(&f,&g,1,1);
#if DEBUG
		fprintf(stderr, "extracted field %d '%.*s'\n",
			(int)f.valid-1, (int)g.len, e->s + g.from);
#endif
		i += fld_len;
		unsigned sep_len = strchr(field_sep, e->s[i]) ? 1 : 0; // strspn(e->s + i, field_sep);
		i += sep_len;
		if (!sep_len)
			break;
	}
	e->f = f.v;
	e->n = f.valid;

	if (e->n)
		return 1;
	free(e->s);
	free(e->f);
	return 0;
}

static void read_input(
	char *fname, char desc, const char *field_sep, int trim,
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
		if (entry_extract(&e, line, len, field_sep, trim))
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

int yyparse(struct tnode **expr, yyscan_t scanner, char max_id);

static struct tnode * tnode_parse(char *s, char max_id)
{
	struct tnode *r;
	yyscan_t scanner;
	YY_BUFFER_STATE state;

	if (yylex_init(&scanner))
		DIE(1,"error initializing scanner: %s\n", strerror(errno));
	state = yy_scan_string(s, scanner);
	if (yyparse(&r, scanner, max_id))
		DIE(1,"error parsing '%s'\n", s);
	yy_delete_buffer(state, scanner);
	yylex_destroy(scanner);
	return r;
}

typedef void tnode_walk_f(struct tnode *t, void *data);

static void tnode_walk(struct tnode *t, tnode_walk_f *pre, tnode_walk_f *mid, tnode_walk_f *post, void *data)
{
	if (!t) return;
	if (pre) pre(t, data);
	tnode_walk(t->ch[0], pre, mid, post, data);
	if (mid) mid(t, data);
	tnode_walk(t->ch[1], pre, mid, post, data);
	if (post) post(t, data);
}

static void tnode_dump(FILE *f, const struct tnode *e)
{
	static const char ss[] = {
		[TNODE_ID]       = 'A',
		[TNODE_CUP]      = '|',
		[TNODE_CAP]      = '&',
		[TNODE_SETMINUS] = '-',
		[TNODE_XOR]      = '^',
	};
	if (!e)
		return;
	if (e->type == TNODE_ID)
		fprintf(f, "%c", MIN_ID + e->id);
	else {
		fprintf(f, "%c(", ss[e->type]);
		tnode_dump(f, e->ch[0]);
		fprintf(f, ",");
		tnode_dump(f, e->ch[1]);
		fprintf(f, ")");
	}
	fprintf(f, "[0x%08x]", e->fields);
}

static int str_fcmp(
	const struct str *pa, unsigned fia,
	const struct str *pb, unsigned fib
) {
	struct field fa = pa->f[fia], fb = pb->f[fib];
	int d = memcmp(pa->s + fa.from, pb->s + fb.from, MIN(fa.len, fb.len));
#if DEBUG
	fprintf(stderr, "cmp %d '%.*s' vs %d '%.*s' -> %d\n",
		fia, (int)MIN(fa.len, fb.len), pa->s + fa.from,
		fib, (int)MIN(fa.len, fb.len), pb->s + fb.from,
		d);
#endif
	return d ? d : fa.len - fb.len;
}

static int str_ycmp(
	const struct str *pa, const struct str *pb, fieldmap_t fmap
) {
	unsigned a = 0;
	fieldmap_t fm = fmap & ~(~(fieldmap_t)0 << MIN(pa->n, pb->n));
	while (fm) {
		while (!(fm & 1))
			fm >>= 1, a++;
		int d = str_fcmp(pa, a, pb, a);
		if (d)
			return d;
		fm >>= 1, a++;
	}
	return fmap & (~(fieldmap_t)0 << MIN(pa->n,pb->n)) ? pa->n - pb->n : 0;
}

static int str_xcmp(
	const struct str *pa, fieldmap_t fmap,
	const struct str *pb, fieldmap_t fmbp
) {
	if (fmap == fmbp)
		return str_ycmp(pa, pb, fmap);
	unsigned a = 0, b = 0;
	fieldmap_t fma = fmap & ~(~(fieldmap_t)0 << pa->n);
	fieldmap_t fmb = fmbp & ~(~(fieldmap_t)0 << pb->n);
	unsigned m = 0;
	while (fma && fmb) {
		while (!(fma & 1)) fma >>= 1, a++;
		while (!(fmb & 1)) fmb >>= 1, b++;
		m++;
		int d = str_fcmp(pa, a, pb, b);
		if (d)
			return d;
		fma >>= 1, a++;
		fmb >>= 1, b++;
	}
	return fma ? -1 : fmb ? +1 : 0;
}

static int str_qcmp(const void *a, const void *b)
{
	const struct str *pa = a, *pb = b;
	return str_ycmp(pa, pb, sort_uniq_fields);
}

static void sort_uniq(struct str_array *a, fieldmap_t fmap)
{
	sort_uniq_fields = fmap;
	varr_qsort(a,str_qcmp);
	unsigned i = 0;
	while (i<a->valid && !(fmap & ~(~(fieldmap_t)0 << a->v[i].n)))
		memmove(a->v+i, a->v+i+1, (--a->valid-i)*sizeof(*a->v));
	for (i=1; i<a->valid; i++)
		if (!(fmap & ~(~(fieldmap_t)0 << a->v[i].n)) || !str_qcmp(a->v+i-1, a->v+i)) {
#if DEBUG
			fprintf(stderr, "removing duplicate '%s' = '%s' wrt. 0x%08x\n", a->v[i-1].s, a->v[i].s, fmap);
#endif
			memmove(a->v+i, a->v+i+1, (--a->valid-i)*sizeof(*a->v));
			i--;
		}
	while (i<a->valid && !(fmap & ~(~(fieldmap_t)0 << a->v[i].n)))
		memmove(a->v+i, a->v+i+1, (--a->valid-i)*sizeof(*a->v));
}

static struct str_array eval(const struct tnode *e, const struct str_array *a)
{
	struct str_array u = VARR_INIT;
	if (e) {
		const struct str_array l = eval(e->ch[0], a);
		const struct str_array r = eval(e->ch[1], a);
		const struct str *pl = l.v, *pr = r.v;
#if DEBUG
		for (unsigned i=0; i<l.valid; i++) {
			tnode_dump(stderr, e);
			fprintf(stderr, " l: '%s'\n", pl[i].s);
		}
		for (unsigned i=0; i<r.valid; i++) {
			tnode_dump(stderr, e);
			fprintf(stderr, " r: '%s'\n", pr[i].s);
		}
#endif
		unsigned nl = 0, nr = 0;
		switch (e->type) {
		case TNODE_ID:
#if DEBUG
			for (unsigned i=0; i<a[e->id].valid; i++) {
				tnode_dump(stderr, e);
				fprintf(stderr, " a[%d]: '%s'\n", e->id, a[e->id].v[i].s);
			}
#endif
			varr_append_a(&u,a+e->id,0);
			break;
		case TNODE_XOR:
			varr_ensure_sz(&u,l.valid + r.valid,0);
			while (nl<l.valid || nr<r.valid) {
				int d = nl>=l.valid ? +1
				      : nr>=r.valid ? -1
				      : str_xcmp(pl, e->ch[0]->fields, pr, e->ch[1]->fields);
				if (d)
					varr_append(&u,d<0?pl:pr,1,1);
				if (d <= 0) nl++, pl++;
				if (d >= 0) nr++, pr++;
			}
			break;
		case TNODE_CAP:
			varr_ensure_sz(&u,MIN(l.valid,r.valid),0);
			while (nl<l.valid && nr<r.valid) {
				int d = str_xcmp(pl, e->ch[0]->fields, pr, e->ch[1]->fields);
				if (!d)
					varr_append(&u,e->ch[0]->id < e->ch[1]->id ? pl : pr,1,1);
				if (d <= 0) nl++, pl++;
				if (d >= 0) nr++, pr++;
			}
			break;
		case TNODE_CUP:
			varr_ensure_sz(&u,l.valid + r.valid,0);
			while (nl<l.valid || nr<r.valid) {
				int d = nl>=l.valid ? +1
				      : nr>=r.valid ? -1
				      : str_xcmp(pl, e->ch[0]->fields, pr, e->ch[1]->fields);
				varr_append(&u,(d < 0 || (!d && e->ch[0]->id < e->ch[1]->id))?pl:pr,1,1);
				if (d <= 0) nl++, pl++;
				if (d >= 0) nr++, pr++;
			}
			break;
		case TNODE_SETMINUS:
			varr_ensure_sz(&u,l.valid,0);
			while (nl<l.valid) {
				int d = nr>=r.valid ? -1
				      : str_xcmp(pl, e->ch[0]->fields, pr, e->ch[1]->fields);
				if (d < 0)
					varr_append(&u,pl,1,1);
				if (d <= 0) nl++, pl++;
				if (d >= 0) nr++, pr++;
			}
			break;
		}
		varr_fini(&l);
		varr_fini(&r);
#if DEBUG
		for (unsigned i=0; i<u.valid; i++) {
			tnode_dump(stderr, e);
			fprintf(stderr, " u: '%s'\n", u.v[i].s);
		}
#endif
		sort_uniq(&u,e->fields);
	}
	return u;
}

static void tnode_free(struct tnode *t, void *data) { free(t); }

int main(int argc, char **argv)
{
	VARR_DECL_ANON(struct str_array) inputs = VARR_INIT;
	struct str_array *stdin_data = NULL;

	char *endptr;
	int   opt;
	int   n;
	int   verbosity = 0;
	char *expr = NULL;
	char *isep = SETOP_DEF_ISEP;
	int   trim = 1;
	char *osep = SETOP_DEF_OSEP;
	for (n=-1; optind < argc; n++) {
		while ((opt = getopt(argc, argv, ":d:D:htv")) != -1)
			switch (opt) {
			case 'd': isep = optarg; break;
			case 'D': osep = optarg; break;
			case 'h': DIE(0,USAGE "\n" HELP,argv[0]);
			case 't': trim = 0; break;
			case 'v': verbosity++; break;
			case '?': DIE(1,"error: unknown option '-%c'\n",optopt);
			case ':': DIE(1,"error: option '-%c' requires an argument\n",optopt);
			}
		if (optind < argc) {
			if (n < 0)
				expr = argv[optind++];
			else {
				varr_append(&inputs,(&(struct str_array)VARR_INIT),1,1);
				read_input(argv[optind++], MIN_ID+n, isep, trim, inputs.v+n, &stdin_data);
			}
		}
	}
	if (!n || !expr)
		DIE(1,USAGE,argv[0]);
	if (n > MAX_IDS)
		DIE(1,"error: max. %d inputs supported\n",MAX_IDS);

	struct tnode *e = tnode_parse(expr, MIN_ID + n - 1);
	if (verbosity > 0) {
		tnode_dump(stderr, e);
		fprintf(stderr, "\n");
	}

	struct str_array u = eval(e, inputs.v);
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

	for (unsigned i=0; i<n; i++) {
		varr_forall(s,inputs.v+i) {
			free(s->s);
			free(s->f);
		}
		varr_fini(inputs.v+i);
	}
	varr_fini(&inputs);

	tnode_walk(e, NULL, NULL, tnode_free, NULL);

	return 0;
}
