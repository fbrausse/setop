
#ifndef TNODE_H
#define TNODE_H

#include <stdlib.h>
#include <stddef.h>
#include "common.h"
#include "array.h"

#define MIN_ID		'A'
#define MAX_ID		'Z'
#define MAX_IDS		(MAX_ID - MIN_ID + 1)
#define MAX_FIELD	31

typedef unsigned fieldmap_t;

struct str {
	char *s;
	struct field { unsigned from, len; } *f;
	unsigned n;
};

VARR_DECL(str_array,struct str);
VARR_DECL(src_array,struct str_array);

struct fnode;
VARR_DECL(fnode_arr,struct fnode *);

enum tnode_type {
	TNODE_ID, TNODE_UNION, TNODE_INTERS, TNODE_DIFF, TNODE_SYMDIFF,
};

struct tnode {
	enum tnode_type type;
	struct tnode *ch[2];
	int id;
	fieldmap_t fields;
};

enum fnode_type {
	FNODE_AND, FNODE_OR, FNODE_LT, FNODE_GT, FNODE_EQ, /* ch[0:1].fnode */
	FNODE_NEG,   /* ch[0].fnode */
	FNODE_VAR,   /* ch[0].var */
	FNODE_TUPLE, /* ch[0].arr */
	FNODE_LIT,   /* ch[0].lit */
	FNODE_CONST, /* ch[0].cnst */
	FNODE_INCL,  /* ch[0].tnode, ch[1].var */
};

struct fnode {
	enum fnode_type type;
	union fnode_ch {
		struct fnode *fnode;
		struct tnode *tnode;
		char *lit;
		char var;
		struct fnode_arr arr;
		int cnst;
	} ch[];
};

void tnode_tree_free(struct tnode *t);
void fnode_tree_free(struct fnode *r);

static inline struct fnode * fnode_create(enum fnode_type type, unsigned nch)
{
	struct fnode *r = malloc(offsetof(struct fnode,ch) + sizeof(union fnode_ch) * nch);
	r->type = type;
	return r;
}

static inline struct fnode * fnode_create0(int cnst)
{
	struct fnode *r = fnode_create(FNODE_CONST, 1);
	r->ch[0].cnst = cnst;
	return r;
}

static inline struct fnode * fnode_create1(
	enum fnode_type type, struct fnode *ch0
) {
	struct fnode *r = fnode_create(type, 1);
	r->ch[0].fnode = ch0;
	return r;
}

static inline struct fnode * fnode_create2(
	enum fnode_type type, struct fnode *ch0, struct fnode *ch1
) {
	struct fnode *r = fnode_create(type, 2);
	r->ch[0].fnode = ch0;
	r->ch[1].fnode = ch1;
	return r;
}

static inline struct fnode * fnode_create_incl(struct tnode *t, char var)
{
	struct fnode *r = fnode_create(FNODE_INCL, 2);
	r->ch[0].tnode = t;
	r->ch[1].var = var;
	return r;
}

static inline struct fnode * fnode_create_lit(char *s)
{
	struct fnode *r = fnode_create(FNODE_LIT, 1);
	r->ch[0].lit = s;
	return r;
}

static inline struct fnode * fnode_create_var(char v)
{
	struct fnode *r = fnode_create(FNODE_VAR, 1);
	r->ch[0].var = v;
	return r;
}

static inline struct fnode * fnode_create_tuple(struct fnode_arr a)
{
	struct fnode *r = fnode_create(FNODE_TUPLE, 1);
	r->ch[0].arr = a;
	return r;
}



static inline struct tnode * tnode_create(
	enum tnode_type type, struct tnode *ch0, struct tnode *ch1
) {
	struct tnode *r = malloc(sizeof(struct tnode));
	r->type = type;
	r->ch[0] = ch0;
	r->ch[1] = ch1;
	r->fields = ~(fieldmap_t)0;
	return r;
}

static inline struct tnode * tnode_create_id(int id, fieldmap_t fields)
{
	struct tnode *r = tnode_create(TNODE_ID, NULL, NULL);
	r->id = id;
	r->fields = fields;
	return r;
}

int src_create_set(
	struct src_array *s, struct fnode_arr list, const struct fnode *formula
);

struct str_array tnode_eval(const struct tnode *e, const struct str_array *a);

static inline fieldmap_t tnode_field(int from, int to)
{
	fieldmap_t mask  = ~(fieldmap_t)0;
	fieldmap_t fto   = ~(mask << (to+1));
	fieldmap_t ffrom = ~(mask << from);
	return fto & ~ffrom;
	return ~(~(fieldmap_t)0 << (to+1)) & ~(~(fieldmap_t)0 << from);
}

#endif
