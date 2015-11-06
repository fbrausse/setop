
#ifndef ARRAY_H
#define ARRAY_H

#include "common.h"

#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>

struct array {
	char   *c;
	size_t  sz;    /* in chars */
	size_t  valid; /* in chars */
};

#define ARRAY_INIT		{ NULL, 0, 0, }

static inline int array_init(struct array *a, size_t sz)
{
	void *p = malloc(sz);
	if (sz && !p)
		return -errno;
	a->c = p;
	a->sz = sz;
	a->valid = 0;
	return 0;
}

static inline int array_resize(struct array *a, size_t sz)
{
	void *p = realloc(a->c, sz);
	if (sz && !p)
		return -errno;
	a->c = p;
	a->sz = sz;
	if (sz < a->valid)
		a->valid = sz;
	return 0;
}

static inline void array_fini(struct array *a)
{
	free(a->c);
	a->c = NULL;
}

static inline int array_enlarge_by(struct array *a, size_t sz)
{
	return array_resize(a, a->sz + sz);
}

static inline int array_ensure_sz(struct array *a, size_t sz, int double_sz)
{
	if (a->sz < sz)
		return array_resize(a, double_sz ? 2 * sz : sz);
	return 0;
}

static inline int array_cstr_compat(struct array *a)
{
	int r = array_ensure_sz(a, a->valid+1, 0);
	if (r)
		return r;
	a->c[a->valid] = '\0';
	return 0;
}

/* v must not overlap with the range of a->v that is to be written */
static inline int array_append(struct array *a, const void *v, size_t sz, int double_sz)
{
	int r = array_ensure_sz(a, a->valid + sz, double_sz);
	if (r)
		return r;
	memcpy(a->c + a->valid, v, sz);
	a->valid += sz;
	return 0;
}

/*#define array_append_v(a, v) array_append((a), &(v), sizeof((v)), 1)*/

static inline int array_append_a(struct array *a, struct array *b, int double_sz)
{
	return array_append(a, b->c, b->valid, double_sz);
}

static inline int array_vprintf(struct array *a, size_t at, const char *fmt, va_list ap)
{
	va_list ap2;
	va_copy(ap2, ap);
	int n = vsnprintf(NULL, 0, fmt, ap);

	int r = array_ensure_sz(a, at + n+1, 0);
	if (r < 0)
		return r;

	r = vsnprintf(a->c + at, n+1, fmt, ap2);
	if (r >= 0 && at + r > a->valid)
		a->valid = at + r;

	return r;
}

func_format(3,4)
static inline int array_printf(struct array *a, size_t at, const char *fmt, ...)
{
	va_list ap;
	int n;
	va_start(ap,fmt);
	n = array_vprintf(a, at, fmt, ap);
	va_end(ap);
	return n;
}

static inline int array_vappendf(struct array *a, const char *fmt, va_list ap)
{
	return array_vprintf(a, a->valid, fmt, ap);
}

func_format(2,3)
static inline int array_appendf(struct array *a, const char *fmt, ...)
{
	va_list ap;
	int n;
	va_start(ap,fmt);
	n = array_vappendf(a, fmt, ap);
	va_end(ap);
	return n;
}

#define VARR_DECL0(id,...)			\
	struct id {				\
		__VA_ARGS__;			\
		size_t n;	/* in type */	\
		size_t valid;	/* in type */	\
	}
#define VARR_DECL(id,...)		VARR_DECL0(id,__VA_ARGS__ *v)
#define VARR_DECL_ANON(...)		VARR_DECL(,__VA_ARGS__)
#define VARR_INIT			{ NULL, 0, 0 }
#define VARR_ESZ(a)			((char *)((a)->v+1)-(char *)(a)->v)
#define varr_init(a,n)			_varr_init((struct array *)(a), (n),VARR_ESZ(a))
#define varr_fini(a)			array_fini((struct array *)(a))
#define varr_resize(a,n)		_varr_resize((struct array *)(a), (n),VARR_ESZ(a))
#define varr_enlarge_by(a,n)		_varr_enlarge_by((struct array *)(a),(n),VARR_ESZ(a))
#define varr_ensure_sz(a,n,double_sz)	_varr_ensure_sz((struct array *)(a),(n),VARR_ESZ(a),(double_sz))
#define varr_append(a,v,n,double_sz)	_varr_append((struct array *)(a),(v),(n),VARR_ESZ(a),(double_sz))
#define varr_append_a(a,b,double_sz)	_varr_append_a((struct array *)(a),VARR_ESZ(a),(struct array *)(b),VARR_ESZ(b),(double_sz))
#define varr_insert(a,at,v,n,double_sz)	_varr_insert((struct array *)(a),(at),(v),(n),VARR_ESZ(a),(double_sz))

#define varr_forall(vvar,a)		for (vvar = (a)->v; (vvar - (a)->v) < (a)->valid; vvar++)
#define varr_qsort(a,cmp)		qsort((a)->v, (a)->valid, VARR_ESZ(a), (cmp))
#define varr_ck_bsearch(key,a,cmp)	ck_bsearch(key, (a)->v, (a)->valid, VARR_ESZ(a), (cmp))
#define varr_bsearch(key,a,cmp)		bsearch(key, (a)->v, (a)->valid, VARR_ESZ(a), (cmp))

static inline int _varr_init(struct array *a, size_t n, size_t esz)
{
	void *p = calloc(n, esz);
	if (n && esz && !p)
		return -errno;
	a->c = p;
	a->sz = n;
	a->valid = 0;
	return 0;
}

static inline int _varr_resize(struct array *a, size_t n, size_t esz)
{
	void *p = realloc(a->c, n * esz);
	if (n && esz && !p)
		return -errno;
	if (a->sz < n)
		memset((char *)p + a->sz * esz, 0, esz * (n - a->sz));
	a->c = p;
	a->sz = n;
	if (n < a->valid)
		a->valid = n;
	return 0;
}

static inline int _varr_enlarge_by(struct array *a, size_t n, size_t esz)
{
	return _varr_resize(a, a->sz + n, esz);
}

static inline int _varr_ensure_sz(struct array *a, size_t n, size_t esz, int double_sz)
{
	if (a->sz < n)
		return _varr_resize(a, double_sz ? 2 * n : n, esz);
	return 0;
}

static inline ptrdiff_t _varr_append(struct array *a, const void *v, size_t n, size_t esz, int double_sz)
{
	int r = _varr_ensure_sz(a, a->valid + n, esz, double_sz);
	if (r)
		return r;
	ptrdiff_t d = a->valid;
	memcpy(a->c + a->valid * esz, v, n * esz);
	a->valid += n;
	return d;
}

static inline int _varr_append_a(struct array *a, size_t asz, struct array *b, size_t bsz, int double_sz)
{
	if (asz != bsz)
		return -EINVAL;
	return _varr_append(a, b->c, b->valid, asz, double_sz);
}

static inline int _varr_insert(struct array *a, size_t at, const void *v, size_t n, size_t esz, int double_sz)
{
	int r = _varr_ensure_sz(a, a->valid + n, esz, double_sz);
	if (r)
		return r;
	memmove(a->c + (at + n) * esz, a->c + at * esz, (a->valid - at) * esz);
	memcpy(a->c + at * esz, v, n * esz);
	a->valid += n;
	return 0;
}

#endif
