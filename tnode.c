
#include "tnode.h"

struct var {
	char id;
	fieldmap_t fields;
};

VARR_DECL(field_arr,struct field);
VARR_DECL(var_arr,struct var);

static void fnode_prep(
	struct array *s, struct field_arr *f, const struct fnode *g,
	struct var_arr *vars
) {
	struct field ff = { s->valid, 0 };
	const struct fnode_arr *a;
	struct fnode **h;
	struct var *v;
	fieldmap_t fld_mask;

	switch (g->type) {
	case FNODE_TUPLE:
		a = &g->ch[0].arr;
		if (!a->valid)
			varr_append(f,&ff,1,1);
		else
			varr_forall(h,a)
				fnode_prep(s,f,*h,vars);
		return;
	case FNODE_LIT:
		ff.len = strlen(g->ch[0].lit);
		array_append(s,g->ch[0].lit,ff.len,1);
		varr_append(f,&ff,1,1);
		return;
	case FNODE_VAR:
		fld_mask = (fieldmap_t)1 << f->valid;
		varr_append(f,&ff,1,1);
		varr_forall(v,vars)
			if (v->id == g->ch[0].var) {
				v->fields |= fld_mask;
				return;
			}
		varr_append(vars,(&(struct var){ g->ch[0].var, fld_mask }),1,1);
		return;
	default:
		DIE(1,"only VAR, LIT and TUPLE allowed inside a TUPLE, got %d\n",g->type);
	}
}

static int fnode_assign_vars(
	struct array *c, struct field_arr *f, const struct var_arr *v,
	const struct fnode *formula
) {
	return 0;
}

static void fnode_strs(struct str_array *r, struct fnode *g, const struct fnode *formula)
{
	struct field_arr f = VARR_INIT, f0;
	struct array c = ARRAY_INIT, c0;
	struct var_arr v = VARR_INIT;

	fnode_prep(&c, &f, g, &v);

	while (1) {
		struct field_arr f0 = VARR_INIT;
		struct array c0 = ARRAY_INIT;
		array_append_a(&c0,&c,0);
		varr_append_a(&f0,&f,0);
		if (1 /*fnode_assign_vars(&c0,&f0,&v,formula)*/) {
			varr_append(r,(&(struct str){c0.c, f0.v, f0.valid}),1,1);
			break;
		} else {
			varr_fini(&f0);
			array_fini(&c0);
			break;
		}
	}
	varr_fini(&f);
	array_fini(&c);
	varr_fini(&v);
}

int src_create_set(
	struct src_array *s, struct fnode_arr list, const struct fnode *formula
) {
	struct str_array r = VARR_INIT;
	struct fnode **f;
	varr_forall(f,&list) {
		fnode_strs(&r, *f, formula);
		fnode_tree_free(*f);
	}
	varr_fini(&list);
	int ret = s->valid;
	varr_append(s,&r,1,1);
	return ret;
}

void fnode_tree_dump(FILE *f, const struct fnode *r)
{
	struct fnode **s;
	fprintf(f, "(%c ", "&|<>=!vtlci"[r->type]);
	switch (r->type) {
	case FNODE_AND:
	case FNODE_OR:
	case FNODE_LT:
	case FNODE_GT:
	case FNODE_EQ:
		fnode_tree_dump(f, r->ch[0].fnode);
		fprintf(f, ",");
		fnode_tree_dump(f, r->ch[1].fnode);
		break;
	case FNODE_NEG: fnode_tree_dump(f, r->ch[0].fnode); break;
	case FNODE_VAR: fprintf(f, "%c", r->ch[0].var); break;
	case FNODE_TUPLE:
		fprintf(f, "[");
		varr_forall(s,&r->ch[0].arr) {
			fnode_tree_dump(f, *s);
			fprintf(f, ",");
		}
		fprintf(f, "]");
		break;
	case FNODE_LIT: fprintf(f, "'%s'", r->ch[0].lit); break;
	case FNODE_CONST: fprintf(f, "%d", r->ch[0].cnst); break;
	case FNODE_INCL: tnode_dump(f, r->ch[0].tnode); fprintf(f, "(%c)", r->ch[1].var); break;
	}
	fprintf(f, ")");
}

void fnode_tree_free(struct fnode *r)
{
	if (!r)
		return;
	struct fnode **s;
	switch (r->type) {
	case FNODE_AND:
	case FNODE_OR:
	case FNODE_LT:
	case FNODE_GT:
	case FNODE_EQ:
		fnode_tree_free(r->ch[1].fnode);
	case FNODE_NEG:
		fnode_tree_free(r->ch[0].fnode);
		break;
	case FNODE_VAR: break;
	case FNODE_TUPLE:
		varr_forall(s,&r->ch[0].arr)
			fnode_tree_free(*s);
		varr_fini(&r->ch[0].arr);
		break;
	case FNODE_LIT: free(r->ch[0].lit); break;
	case FNODE_CONST: break;
	case FNODE_INCL:
		tnode_tree_free(r->ch[0].tnode);
		break;
	}
	free(r);
}



void tnode_tree_free(struct tnode *t)
{
	if (!t)
		return;
	tnode_tree_free(t->ch[0]);
	tnode_tree_free(t->ch[1]);
	free(t);
}

void tnode_dump(FILE *f, const struct tnode *e)
{
	static const char ss[] = {
		[TNODE_ID]       = 'A',
		[TNODE_UNION]    = '|',
		[TNODE_INTERS]   = '&',
		[TNODE_DIFF]     = '-',
		[TNODE_SYMDIFF]  = '^',
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

static fieldmap_t sort_uniq_fields;

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

struct str_array tnode_eval(const struct tnode *e, const struct str_array *a)
{
	struct str_array u = VARR_INIT;
	if (e) {
		const struct str_array l = tnode_eval(e->ch[0], a);
		const struct str_array r = tnode_eval(e->ch[1], a);
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
		case TNODE_SYMDIFF:
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
		case TNODE_INTERS:
			varr_ensure_sz(&u,MIN(l.valid,r.valid),0);
			while (nl<l.valid && nr<r.valid) {
				int d = str_xcmp(pl, e->ch[0]->fields, pr, e->ch[1]->fields);
				if (!d)
					varr_append(&u,e->ch[0]->id < e->ch[1]->id ? pl : pr,1,1);
				if (d <= 0) nl++, pl++;
				if (d >= 0) nr++, pr++;
			}
			break;
		case TNODE_UNION:
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
		case TNODE_DIFF:
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
