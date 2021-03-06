
#ifndef COMMON_H
#define COMMON_H

#if 0
#define _BSD_SOURCE			/* endianness conversions needed by "pb.h" */
#define _POSIX_C_SOURCE		200809L	/* getline(), strndup() */
#define _XOPEN_SOURCE			/* strptime() */
#endif

#ifndef alignof
# define alignof(type)		offsetof(struct { char c; type t; },t)
#endif

#if defined(__GNUC__) || defined(__clang__)
# define likely(x)			__builtin_expect(!!(x), 1)
# define unlikely(x)			__builtin_expect((x), 0)
# define func_deprecated		__attribute__((deprecated))
# define func_format(fmt_n,arg_n)	__attribute__((format(printf,fmt_n,arg_n)))
# define func_va_null_terminated	__attribute__((sentinel(0)))
# define func_non_null			__attribute__((nonnull))
# define LOG2(n)						\
	((unsigned)((CHAR_BIT * sizeof(n)) - 1 -		\
	 _Generic((n),						\
		unsigned: __builtin_clz(n),			\
		const unsigned: __builtin_clz(n),		\
		unsigned long: __builtin_clzl(n),		\
		const unsigned long: __builtin_clzl(n),		\
		unsigned long long: __builtin_clzll(n),		\
		const unsigned long long: __builtin_clzll(n))))
#else
# define likely(x)			!!(x)
# define unlikely(x)			(x)
# define func_deprecated
# define func_format(fmt_n,arg_n)
# define func_va_null_terminated
# define func_non_null
# define LOG2(n)								\
	((unsigned)_Generic((n),						\
		unsigned: LOG2_U((unsigned)(n)),				\
		const unsigned: LOG2_U((unsigned)(n)),				\
		unsigned long: LOG2_UL((unsigned long)(n)),			\
		const unsigned long: LOG2_UL((unsigned long)(n)),		\
		unsigned long long: LOG2_ULL((unsigned long long)(n)),		\
		const unsigned long long: LOG2_ULL((unsigned long long)(n))))
#endif

#if !defined(__STDC__) || (__STDC_VERSION__ - 0) < 199901L
# define inline
# define restrict
# define ARRAY_SIZE(arr)	(sizeof(arr)/sizeof(*(arr)))
#else
# define ARRAY_SIZE(...)	(sizeof(__VA_ARGS__)/sizeof(*(__VA_ARGS__)))
# ifdef LOG_DEBUG
#  define LOG(...)		(fprintf(stderr, "%9s:%-4d ",__FILE__,__LINE__)+\
				 fprintf(stderr, __VA_ARGS__))
#  define DBG(...)		LOG(...)
# else
#  define LOG(...)		fprintf(stderr, __VA_ARGS__)
#  define DBG(...)
# endif
# define FATAL(ret,...)		do { LOG(__VA_ARGS__); exit(ret); } while (0)
#endif

#define MIN(a,b)		((a) < (b) ? (a) : (b))
#define MAX(a,b)		((a) > (b) ? (a) : (b))
#define SGN2(a,b)		((a) < (b) ? -1 : (a) > (b) ? 1 : 0)
#define SGN(a)			SGN2(a,0)
#define ABS(a)			MAX(a,-(a))
#define STR(x)			#x
#define XSTR(x)			STR(x)

/* LOG2_k(n) returns floor(log2(n)) and is valid for values 0 <= n < 1 << k */
#define LOG2_2(n)	((n)&0x2               ? 1                 :0)
#define LOG2_4(n)	((n)&0xc               ? 2+LOG2_2 ((n)>>2 ):LOG2_2(n))
#define LOG2_8(n)	((n)&0xf0              ? 4+LOG2_4 ((n)>>4 ):LOG2_4(n))
#define LOG2_16(n)	((n)&0xff00            ? 8+LOG2_8 ((n)>>8 ):LOG2_8(n))
#define LOG2_32(n)	((n)&0xffff0000        ?16+LOG2_16((n)>>16):LOG2_16(n))
#define LOG2_64(n)	((n)&0xffffffff00000000?32+LOG2_32((n)>>32):LOG2_32(n))
//#define NBITS(n)	(!(n) ? 0 : 1+NBITS32(n))

#include <limits.h>

#if ULLONG_MAX/2 <= 1ULL << (32-1)
# define LOG2_ULL(n)	LOG2_32(n)
#elif ULLONG_MAX/2 <= 1ULL << (64-1)
# define LOG2_ULL(n)	LOG2_64(n)
#else
# error unable to def LOG2_ULL
#endif

#if ULONG_MAX/2 <= 1UL << (32-1)
# define LOG2_UL(n)	LOG2_32(n)
#elif ULONG_MAX/2 <= 1UL << (64-1)
# define LOG2_UL(n)	LOG2_64(n)
#else
# error unable to def LOG2_UL
#endif

#if UINT_MAX/2 <= 1U << (32-1)
# define LOG2_U(n)	LOG2_32(n)
#elif UINT_MAX/2 <= 1U << (64-1)
# define LOG2_U(n)	LOG2_64(n)
#else
# error unable to def LOG2_U
#endif

typedef unsigned long bitset_t;

/* BIT_* macros require predefined macros 'macro_pref ## _{BITS,SHIFT}' */
#if (__STDC_VERSION__ - 0) < 201112L || \
    (!defined(__clang__) && /* GCC doesn't support _Generic as of 4.8 ... */ \
     defined(__GNUC__) && (__GNUC__ < 4 || __GNUC__ == 4 && __GNUC_MINOR__ <= 8))
/* warning: BIT_{GET,ADD} evaluate their arguments multiple times */
# define BIT_MASKT(type,macro_pref)	BIT_MASK((type)0,macro_pref)
# define BIT_MASK(val,macro_pref)	(~(~((val) & 0) << (macro_pref##_BITS)))
#else
# define BIT_MASKT(type,macro_pref)	(~(~(type)0 << (macro_pref##_BITS)))
# define BIT_MASK(val,macro_pref)	\
	_Generic((val),								\
		unsigned long long: BIT_MASKT(unsigned long long,macro_pref),	\
		unsigned long     : BIT_MASKT(unsigned long,macro_pref),	\
		unsigned          : BIT_MASKT(unsigned,macro_pref))
#endif
#define BIT_GET(bits,macro_pref)	\
	(((bits) >> (macro_pref##_SHIFT)) & BIT_MASK((bits),macro_pref))
#define BIT_ADD(val,macro_pref)		\
	(((val) & BIT_MASK((val),macro_pref)) << (macro_pref##_SHIFT))

#define DIE(exitcode, ...)	\
	do { fprintf(stderr, __VA_ARGS__); exit(exitcode); } while (0)

#include <sys/types.h>		/* ssize_t */
#include <stdlib.h>		/* malloc(), etc. */
#include <stdio.h>		/* *printf() */
#include <string.h>		/* strerror() */
#include <errno.h>		/* errno */

struct cstr {
	const char *s;
	size_t l;
};

#define CSTR(x)			{ (x), sizeof(x)-1, }

typedef char utf8_t;

static unsigned bsearch_depth;

static func_non_null ssize_t ck_bsearch(
	const void *key, const void *base, size_t nmemb, size_t esz,
	int (*compar)(const void *, const void *)
) {
	ssize_t l = 0, r = (ssize_t)nmemb - 1, m;
	void *i = NULL;
	int c;
	bsearch_depth = 0;
	while (l <= r) {
		bsearch_depth++;
		m = l + (r-l)/2;
		c = compar(key, (const char *)base + m * esz);
		if (c < 0)
			r = m - 1;
		else if (c > 0)
			l = m + 1;
		else
			return m;
	}
	return ~l;
}

inline static void * ck_malloc(size_t size)
{
	void *p = malloc(size);
	if (size && unlikely(!p))
		FATAL(-1, "malloc: %s", strerror(errno));
	return p;
}

inline static void * ck_calloc(size_t nmemb, size_t size)
{
	void *p = calloc(nmemb, size);
	if (nmemb && size && unlikely(!p))
		FATAL(-1, "calloc: %s", strerror(errno));
	return p;
}

inline static void * ck_realloc(void *old_p, size_t size)
{
	void *p = realloc(old_p, size);
	if (size && unlikely(!p)) {
		free(old_p);
		FATAL(-1, "realloc: %s", strerror(errno));
	}
	return p;
}

inline static func_non_null void * ck_memcpy(void *restrict dest, const void *restrict src, size_t n)
{
	return (char *)memcpy(dest, src, n) + n;
}

inline static func_non_null void * ck_memmove(void *dest, const void *src, size_t n)
{
	return (char *)memmove(dest, src, n) + n;
}

inline static func_non_null char * ck_strddup(const char *from, const char *to)
{
	size_t len = to - from;
	char *c = ck_malloc(len + 1);
	*(char *)ck_memcpy(c, from, len) = '\0';
	return c;
}

static inline func_non_null void * memdup(const void *src, size_t n)
{
	return memcpy(ck_malloc(n), src, n);
}

#endif
