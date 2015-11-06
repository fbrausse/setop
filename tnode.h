
#ifndef TNODE_H
#define TNODE_H

#include <stdlib.h>
#include "common.h"

#define MIN_ID		'A'
#define MAX_ID		'Z'
#define MAX_IDS		(MAX_ID - MIN_ID + 1)
#define MAX_FIELD	31

typedef unsigned fieldmap_t;

enum tnode_type {
	TNODE_ID, TNODE_CUP, TNODE_CAP, TNODE_SETMINUS, TNODE_XOR,
};

struct tnode {
	enum tnode_type type;
	struct tnode *ch[2];
	int id;
	fieldmap_t fields;
};

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

static inline struct tnode * tnode_create_id(char c, fieldmap_t fields)
{
	if (c < MIN_ID || c > MAX_ID)
		return NULL;
	struct tnode *r = tnode_create(TNODE_ID, NULL, NULL);
	r->id = c - MIN_ID;
	r->fields = fields;
	return r;
}

static inline fieldmap_t tnode_field(int from, int to)
{
	fieldmap_t mask  = ~(fieldmap_t)0;
	fieldmap_t fto   = ~(mask << (to+1));
	fieldmap_t ffrom = ~(mask << from);
	return fto & ~ffrom;
	return ~(~(fieldmap_t)0 << (to+1)) & ~(~(fieldmap_t)0 << from);
}

#endif
