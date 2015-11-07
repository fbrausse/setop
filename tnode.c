
#include "tnode.h"

static void fnode_tuple_flatten(struct fnode_arr *t)
{
	struct fnode **n = t->v;
	struct fnode_arr a;
	for (size_t i=0; i<t->valid; i++, n++) {
		switch ((*n)->type) {
		case FNODE_VAR:
		case FNODE_LIT:
			break;
		case FNODE_TUPLE:
			a = (*n)->ch->arr;
			if (a.valid >= 1) {
				*n = a.v[--a.valid];
				varr_insert(t,i,a.v,a.valid,1);
			} else {
				memmove(t->v+i, t->v+i+1, sizeof(*t->v) * (--t->valid - i));
			}
			varr_fini(&a);
			i--;
			n--;
			break;
		default:
			DIE(1,"only VAR, LIT and TUPLE allowed inside a TUPLE\n");
		}
	}
}

static void fnode_eval(struct str_array *r, const struct fnode_arr *t, const struct fnode *f)
{
	
}

int src_create_set(
	struct src_array *s, struct fnode_arr list, const struct fnode *formula
) {
	struct str_array r = VARR_INIT;
	fnode_tuple_flatten(&list);
	fnode_eval(&r, &list, formula);
	int ret = s->valid;
	varr_append(s,&r,1,1);
	return ret;
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
