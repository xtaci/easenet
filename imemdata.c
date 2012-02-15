/**********************************************************************
 *
 * imemdata.c - basic data structures and algorithms
 *
 * ISTREAM - definition of the basic stream interface
 *
 * VALUE OPERATION
 * type independance value classes
 *
 * DICTIONARY OPERATION
 *
 * feature: 2.1-2.2 times faster than std::map (string or integer key)
 * feature: 1.3-1.5 times faster than stdext::hash_map 
 *
 * for more information, please see the readme file
 *
 **********************************************************************/

#include "imemdata.h"

#include <ctype.h>
#include <assert.h>


/**********************************************************************
 * VALUE OPERATION
 **********************************************************************/


/**********************************************************************
 * DICTIONARY OPERATION
 **********************************************************************/


/*-------------------------------------------------------------------*/
/* dictionary basic interface                                        */
/*-------------------------------------------------------------------*/

/* create dictionary */
idict_t *idict_create(void)
{
	idict_t *dict;
	ilong i;
	dict = (idict_t*)ikmem_malloc(sizeof(idict_t));
	if (dict == NULL) return NULL;

	imnode_init(&dict->nodes, sizeof(struct IDICTENTRY), &ikmem_allocator);
	iv_init(&dict->vect, &ikmem_allocator);

	dict->shift = 6;
	dict->length = (1 << dict->shift);
	dict->mask = dict->length - 1;
	dict->size = 0;
	dict->nodes.grow_limit = 8192;

	if (iv_resize(&dict->vect, sizeof(struct IDICTBUCKET) * dict->length)) {
		ikmem_free(dict);
		return NULL;
	}

	dict->table = (struct IDICTBUCKET*)dict->vect.data;
	for (i = 0; i < dict->length; i++) {
		iqueue_init(&dict->table[i].head);
		dict->table[i].count = 0;
	}

	for (i = 0; i < (ilong)IDICT_LRUSIZE; i++) 
		dict->lru[i] = NULL;

	dict->inc = 0;
	return dict;
}

/* delete dictionary */
void idict_delete(idict_t *dict)
{
	struct IDICTENTRY *entry;
	ilong index;

	assert(dict);
	index = imnode_head(&dict->nodes);
	for (; index >= 0; ) {
		entry = (struct IDICTENTRY*)IMNODE_DATA(&dict->nodes, index);
		iqueue_del(&entry->queue);
		it_destroy(&entry->key);
		it_destroy(&entry->val);
		index = imnode_next(&dict->nodes, index);
	}
	iv_destroy(&dict->vect);
	imnode_destroy(&dict->nodes);
	ikmem_free(dict);
}

/* search pair in dictionary */
static inline idictentry_t *_idict_search(idict_t *dict, const ivalue_t *key)
{
	idictentry_t *recent, *entry;
	struct IDICTBUCKET *bucket;
	iqueue_head *head, *p;
	iulong hash1;
	iulong hash2;

	hash1 = key->hash;
	hash2 = ((hash1 & 0xffff) + (hash1 >> 16)) & (IDICT_LRUSIZE - 1);
	recent = dict->lru[hash2];

	if (recent) {
		if (recent->key.hash == hash1) {
			if (it_cmp(&recent->key, key) == 0) {
				return recent;
			}
		}
	}

	bucket = &dict->table[hash1 & dict->mask];
	head = &bucket->head;

	for (p = head->next; p != head; p = p->next) {
		entry = iqueue_entry(p, idictentry_t, queue);
		if (entry->key.hash != hash1) continue;
		if (it_cmp(&entry->key, key) == 0) {
			dict->lru[hash2] = entry;
			return entry;
		}
	}

	return NULL;
}

/* reference value and recalculate hash */
static inline void _idict_refval(ivalue_t *dst, const ivalue_t *src)
{
	if (it_type(src) != ITYPE_STR) {
		*dst = *src;
		dst->hash = (iulong)it_int(dst);
		return;
	}
	it_strref(dst, it_str(src), it_size(src));
	dst->rehash = 1;
	dst->hash = src->hash;
	if (it_rehash(src) == 0) {
		it_hashstr(dst);
	}
}

/* search pair in dictionary */
ivalue_t *idict_search(idict_t *dict, const ivalue_t *key, ilong *pos)
{
	idictentry_t *entry;
	ivalue_t kk;

	_idict_refval(&kk, key);

	entry = _idict_search(dict, &kk);

	if (entry == NULL) 
		return NULL;

	if (pos) 
		pos[0] = entry->pos;

	return &entry->val;
}

/* calculate lru hash */
static inline iulong _idict_lruhash(iulong hash)
{
	return ((hash & 0xffff) + (hash >> 16)) & (IDICT_LRUSIZE - 1);
}

/* resize bucket */
static inline int _idict_resize(idict_t *dict, int newshift)
{
	struct IDICTBUCKET *bucket, *table;
	idictentry_t *entry;
	iulong hash;
	ilong mask, newsize;
	ilong pos, i;
	int retval;

	newsize = (1l << newshift);

	retval = iv_resize(&dict->vect, sizeof(struct IDICTBUCKET) * newsize);
	if (retval) return -1;

	dict->table = (struct IDICTBUCKET*)dict->vect.data;

	for (i = 0; i < (ilong)newsize; i++) {
		iqueue_init(&dict->table[i].head);
		dict->table[i].count = 0;
	}

	dict->length = newsize;
	dict->shift = newshift;
	dict->mask = newsize - 1;
	mask = dict->mask;
	table = dict->table;

	pos = imnode_head(&dict->nodes);
	for (; pos >= 0; pos = IMNODE_NEXT(&dict->nodes, pos)) {
		entry = (idictentry_t*)IMNODE_DATA(&dict->nodes, pos);
		hash = entry->key.hash;
		bucket = &table[hash & mask];
		bucket->count++;
		iqueue_init(&entry->queue);
		iqueue_add_tail(&entry->queue, &bucket->head);
	}

	return 0;
}

/* update pair inline */
static inline ilong _idict_update(idict_t *dict, const ivalue_t *key, 
	const ivalue_t *val, int isupdate)
{
	idictentry_t *recent, *entry;
	struct IDICTBUCKET *bucket;
	iqueue_head *head, *p;
	iulong hash1;
	iulong hash2;
	ilong pos;

	hash1 = key->hash;
	hash2 = _idict_lruhash(hash1);
	recent = dict->lru[hash2];

	/* check lru cache */
	if (recent) {
		if (recent->key.hash == hash1) {
			if (it_cmp(&recent->key, key) == 0) {
				if (isupdate == 0) return -1;
				it_cpy(&recent->val, val);
				return recent->pos;
			}
		}
	}

	bucket = &dict->table[hash1 & dict->mask];
	head = &bucket->head;

	/* check bucket queue */
	for (p = head->next; p != head; p = p->next) {
		entry = iqueue_entry(p, idictentry_t, queue);
		if (entry->key.hash != hash1) continue;
		if (it_cmp(&entry->key, key) == 0) {
			dict->lru[hash2] = entry;
			if (isupdate == 0) return -2;
			it_cpy(&entry->val, val);
			return entry->pos;
		}
	}

	/* new entry */
	pos = imnode_new(&dict->nodes);
	if (pos < 0) return -3;

	entry = (struct IDICTENTRY*)IMNODE_DATA(&dict->nodes, pos);

	/* copy key & value */
	it_init(&entry->key, it_type(key));
	it_init(&entry->val, it_type(val));

	it_cpy(&entry->key, key);
	it_cpy(&entry->val, val);
	entry->key.hash = key->hash;

	entry->pos = pos;
	entry->sid = ++dict->inc;

	/* add to bucket queue */
	iqueue_add_tail(&entry->queue, &bucket->head);

	dict->lru[hash2] = entry;

	bucket->count++;
	dict->size++;

	/* check necessary of table-growwing */
	if (dict->size >= (dict->length << 1)) {
		_idict_resize(dict, dict->shift + 1);
	}

	return pos;
}

/* add (key, val) pair into dictionary */
ilong idict_add(idict_t *dict, const ivalue_t *key, const ivalue_t *val)
{
	ivalue_t kk;

	_idict_refval(&kk, key);

	return _idict_update(dict, &kk, val, 0);
}

/* delete pair from dictionary */
static inline int _idict_del(idict_t *dict, idictentry_t *entry)
{
	iulong hash1, hash2, pos;
	struct IDICTBUCKET *bucket;

	hash1 = entry->key.hash;
	hash2 = _idict_lruhash(hash1);

	bucket = &dict->table[hash1 & dict->mask];
	iqueue_del(&entry->queue);

	dict->lru[hash2] = NULL;

	it_destroy(&entry->key);
	it_destroy(&entry->val);
	pos = entry->pos;
	entry->pos = -1;
	entry->sid = -1;

	imnode_del(&dict->nodes, pos);
	bucket->count--;
	dict->size--;

	return 0;
}

/* delete pair from dictionary */
int idict_del(idict_t *dict, const ivalue_t *key)
{
	idictentry_t *entry;
	ivalue_t kk;

	_idict_refval(&kk, key);
	entry = _idict_search(dict, &kk);

	if (entry == NULL) 
		return -1;

	return _idict_del(dict, entry);
}

/* update (key, val) from dictionary */
ilong idict_update(idict_t *dict, const ivalue_t *key, const ivalue_t *val)
{
	ivalue_t kk;

	_idict_refval(&kk, key);

	return _idict_update(dict, &kk, val, 1);
}

/* pick entry from position */
static inline idictentry_t *_idict_pick(const idict_t *dict, ilong pos)
{
	idictentry_t *entry;
	imemnode_t *nodes;

	nodes = (imemnode_t*)&dict->nodes;
	if (pos < 0 || pos >= nodes->node_max) return NULL;
	if (IMNODE_MODE(nodes, pos) == 0) return NULL;
	entry = (idictentry_t*)IMNODE_DATA(nodes, pos);

	return entry;
}

/* get key from position */
ivalue_t *idict_pos_get_key(idict_t *dict, ilong pos)
{
	idictentry_t *entry;
	entry = _idict_pick(dict, pos);
	if (entry == NULL) return NULL;
	return &entry->key;
}

/* get value from position */
ivalue_t *idict_pos_get_val(idict_t *dict, ilong pos)
{
	idictentry_t *entry;
	entry = _idict_pick(dict, pos);
	if (entry == NULL) return NULL;
	return &entry->val;
}

/* get sid from position */
ilong idict_pos_get_sid(idict_t *dict, ilong pos)
{
	idictentry_t *entry;
	entry = _idict_pick(dict, pos);
	if (entry == NULL) return -1;
	return entry->sid;
}

/* update from position */
void idict_pos_update(idict_t *dict, ilong pos, const ivalue_t *val)
{
	idictentry_t *entry;
	entry = _idict_pick(dict, pos);
	if (entry == NULL) return;
	it_cpy(&entry->val, val);
}

/* delete from position */
void idict_pos_delete(idict_t *dict, ilong pos)
{
	idictentry_t *entry;
	entry = _idict_pick(dict, pos);
	if (entry == NULL) return;
	_idict_del(dict, entry);
}

/* get first position */
ilong idict_pos_head(idict_t *dict)
{
	return imnode_head(&dict->nodes);
}

/* get next position */
ilong idict_pos_next(idict_t *dict, ilong pos)
{
	return imnode_next(&dict->nodes, pos);
}

/* clear dictionary */
void idict_clear(idict_t *dict)
{
	assert(dict);
	while (1) {
		long pos = idict_pos_head(dict);
		if (pos < 0) break;
		idict_pos_delete(dict, pos);
	}
}


/*-------------------------------------------------------------------*/
/* directly typing interface                                         */
/*-------------------------------------------------------------------*/

/* search: key(str) val(str) */
int idict_search_ss(idict_t *dict, const char *key, ilong keysize,
	char **val, ilong *valsize)
{
	ivalue_t kk, *vv;
	it_strref(&kk, key, keysize);
	vv = idict_search(dict, &kk, 0);
	if (valsize) valsize[0] = -1;
	if (vv == NULL) return -1;
	if (it_type(vv) != ITYPE_STR) return 1;
	if (val) val[0] = it_str(vv);
	if (valsize) valsize[0] = it_size(vv);
	return 0;
}

/* search: key(int) val(str) */
int idict_search_is(idict_t *dict, ilong key, char **val, ilong *valsize)
{
	ivalue_t kk, *vv;
	it_init_int(&kk, key);
	vv = idict_search(dict, &kk, 0);
	if (valsize) valsize[0] = -1;
	if (vv == NULL) return -1;
	if (it_type(vv) != ITYPE_STR) return 1;
	if (val) val[0] = it_str(vv);
	if (valsize) valsize[0] = it_size(vv);
	return 0;
}

/* search: key(str) val(int) */
int idict_search_si(idict_t *dict, const char *key, ilong keysize, 
	ilong *val)
{
	ivalue_t kk, *vv;
	it_strref(&kk, key, keysize);
	vv = idict_search(dict, &kk, 0);
	if (vv == NULL) return -1;
	if (it_type(vv) != ITYPE_INT) return 1;
	if (val) val[0] = it_int(vv);
	return 0;
}

/* search: key(int) val(int) */
int idict_search_ii(idict_t *dict, ilong key, ilong *val)
{
	ivalue_t kk, *vv;
	it_init_int(&kk, key);
	vv = idict_search(dict, &kk, 0);
	if (vv == NULL) return -1;
	if (it_type(vv) != ITYPE_INT) return 1;
	if (val) val[0] = it_int(vv);
	return 0;
}

/* search: key(str) val(ptr) */
int idict_search_sp(idict_t *dict, const char *key, ilong keysize, void**ptr)
{
	ivalue_t kk, *vv;
	it_strref(&kk, key, keysize);
	vv = idict_search(dict, &kk, 0);
	if (ptr) ptr[0] = NULL;
	if (vv == NULL) return -1;
	if (it_type(vv) != ITYPE_PTR) return 1;
	if (ptr) ptr[0] = it_ptr(vv);
	return 0;
}

/* search: key(int) val(ptr) */
int idict_search_ip(idict_t *dict, ilong key, void**ptr)
{
	ivalue_t kk, *vv;
	it_init_int(&kk, key);
	vv = idict_search(dict, &kk, 0);
	if (ptr) ptr[0] = NULL;
	if (vv == NULL) return -1;
	if (it_type(vv) != ITYPE_PTR) return 1;
	if (ptr) ptr[0] = it_ptr(vv);
	return 0;
}

/* add: key(str) val(str) */
ilong idict_add_ss(idict_t *dict, const char *key, ilong keysize,
	const char *val, ilong valsize)
{
	ivalue_t kk, vv;
	it_strref(&kk, key, keysize);
	it_strref(&vv, val, valsize);
	return idict_add(dict, &kk, &vv);
}

/* add: key(int) val(str) */
ilong idict_add_is(idict_t *dict, ilong key, const char *val, ilong valsize)
{
	ivalue_t kk, vv;
	it_init_int(&kk, key);
	it_strref(&vv, val, valsize);
	return idict_add(dict, &kk, &vv);
}

/* add: key(str) val(int) */
ilong idict_add_si(idict_t *dict, const char *key, ilong keysize, ilong val)
{
	ivalue_t kk, vv;
	it_strref(&kk, key, keysize);
	it_init_int(&vv, val);
	return idict_add(dict, &kk, &vv);
}

/* add: key(int) val(int) */
ilong idict_add_ii(idict_t *dict, ilong key, ilong val)
{
	ivalue_t kk, vv;
	it_init_int(&kk, key);
	it_init_int(&vv, val);
	return idict_add(dict, &kk, &vv);
}

/* add: key(str) val(ptr) */
ilong idict_add_sp(idict_t *dict, const char *key, ilong keysize, 
	const void *ptr)
{
	ivalue_t kk, vv;
	it_strref(&kk, key, keysize);
	it_init_ptr(&vv, ptr);
	return idict_add(dict, &kk, &vv);
}

/* add: key(int) val(ptr) */
ilong idict_add_ip(idict_t *dict, ilong key, const void *ptr)
{
	ivalue_t kk, vv;
	it_init_int(&kk, key);
	it_init_ptr(&vv, ptr);
	return idict_add(dict, &kk, &vv);
}

/* update: key(str) val(str) */
ilong idict_update_ss(idict_t *dict, const char *key, ilong keysize,
	const char *val, ilong valsize)
{
	ivalue_t kk, vv;
	it_strref(&kk, key, keysize);
	it_strref(&vv, val, valsize);
	return idict_update(dict, &kk, &vv);
}

/* update: key(int) val(str) */
ilong idict_update_is(idict_t *dict, ilong key, const char *val, 
	ilong valsize)
{
	ivalue_t kk, vv;
	it_init_int(&kk, key);
	it_strref(&vv, val, valsize);
	return idict_update(dict, &kk, &vv);
}

/* update: key(str) val(int) */
ilong idict_update_si(idict_t *dict, const char *key, ilong keysize,
	ilong val)
{
	ivalue_t kk, vv;
	it_strref(&kk, key, keysize);
	it_init_int(&vv, val);
	return idict_update(dict, &kk, &vv);
}

/* update: key(int) val(int) */
ilong idict_update_ii(idict_t *dict, ilong key, ilong val)
{
	ivalue_t kk, vv;
	it_init_int(&kk, key);
	it_init_int(&vv, val);
	return idict_update(dict, &kk, &vv);
}

/* update: key(str) val(ptr) */
ilong idict_update_sp(idict_t *dict, const char *key, ilong keysize, 
	const void *ptr)
{
	ivalue_t kk, vv;
	it_strref(&kk, key, keysize);
	it_init_ptr(&vv, ptr);
	return idict_update(dict, &kk, &vv);
}

/* update: key(int) val(ptr) */
ilong idict_update_ip(idict_t *dict, ilong key, const void *ptr)
{
	ivalue_t kk, vv;
	it_init_int(&kk, key);
	it_init_ptr(&vv, ptr);
	return idict_update(dict, &kk, &vv);
}

/* delete: key(str) */
int idict_del_s(idict_t *dict, const char *key, ilong keysize)
{
	ivalue_t kk;
	it_strref(&kk, key, keysize);
	return idict_del(dict, &kk);
}

/* delete: key(int) */
int idict_del_i(idict_t *dict, ilong key)
{
	ivalue_t kk;
	it_init_int(&kk, key);
	return idict_del(dict, &kk);
}



/**********************************************************************
 * IRING: The struct definition of the cache descriptor
 **********************************************************************/

/* init circle cache */
void iring_init(struct IRING *cache, void *buffer, ilong size)
{
	cache->data = (char*)buffer;
	cache->size = size;
	cache->head = 0;
	cache->tail = 0;
}

/* return data size */
ilong iring_dsize(const struct IRING *cache)
{
	assert(cache);
	return IRING_DSIZE(cache);
}

/* return free size */
ilong iring_fsize(const struct IRING *cache)
{
	assert(cache);
	return IRING_FSIZE(cache);
}

/* write data */
ilong iring_write(struct IRING *cache, const void *data, ilong size)
{
	char *lptr = (char*)data;
	ilong dsize, dfree, half;

	dsize = IRING_DSIZE(cache);
	dfree = (cache->size - dsize) - 1;
	if (dfree <= 0) return 0;

	size = (size < dfree)? size : dfree;
	half = (cache->size - cache->head);

	if (lptr != NULL) {
		if (half >= size) {
			memcpy(cache->data + cache->head, lptr, (size_t)size);
		} else {
			memcpy(cache->data + cache->head, lptr, (size_t)half);
			memcpy(cache->data, lptr + half, (size_t)(size - half));
		}
	}

	cache->head += size;
	if (cache->head >= cache->size) cache->head -= cache->size;

	return size;
}

/* peek data */
ilong iring_peek(const struct IRING *cache, void *data, ilong size)
{
	char *lptr = (char*)data;
	ilong dsize, half;

	dsize = IRING_DSIZE(cache);
	if (dsize <= 0) return 0;

	size = (size < dsize)? size : dsize;
	half = cache->size - cache->tail;

	if (half >= size) {
		memcpy(lptr, cache->data + cache->tail, (size_t)size);
	}	else {
		memcpy(lptr, cache->data + cache->tail, (size_t)half);
		memcpy(lptr + half, cache->data, (size_t)(size - half));
	}

	return size;
}

/* read data */
ilong iring_read(struct IRING *cache, void *data, ilong size)
{
	ilong nsize;
	
	nsize = iring_peek(cache, data, size);
	if (nsize <= 0) return nsize;

	cache->tail += nsize;
	if (cache->tail >= cache->size) cache->tail -= cache->size;

	return nsize;
}

/* drop data */
ilong iring_drop(struct IRING *cache, ilong size)
{
	ilong dsize ;

	dsize = IRING_DSIZE(cache);
	if (dsize <= 0) return 0;

	size = (size < dsize)? size : dsize;
	cache->tail += size;
	if (cache->tail >= cache->size) cache->tail -= cache->size;

	return size;
}

/* get flat ptr and size */
ilong iring_flat(const struct IRING *cache, ilong offset, void **pointer)
{
	ilong dsize, half;

	dsize = IRING_DSIZE(cache);
	if (dsize <= 0) return 0;

	half = cache->size - cache->tail;
	if (pointer) *pointer = (void*)(cache->data + cache->tail);

	return (half <= dsize)? half : dsize;
}

/* put data to given position */
ilong iring_put(struct IRING *cache, ilong pos, const void *data, ilong len)
{
	char *lptr = (char*)data;
	ilong dsize, nsize, size;
	ilong tail, half;

	assert(pos >= 0 && len >= 0);

	nsize = pos + len;
	dsize = IRING_DSIZE(cache);

	if (nsize > cache->size - 1) 
		nsize = cache->size - 1;

	if (nsize <= pos) 
		return 0;

	size = nsize - pos;

	if (nsize > dsize) {
		cache->head += nsize - dsize;
		if (cache->head >= cache->size) 
			cache->head -= cache->size;
	}

	dsize = IRING_DSIZE(cache);
	assert(dsize >= nsize);

	tail = cache->tail + pos;
	if (tail >= cache->size) tail -= cache->size;

	half = cache->size - tail;

	if (lptr != NULL) {
		if (half >= size) {
			memcpy(cache->data + tail, lptr, (size_t)size);
		} else {
			memcpy(cache->data + tail, lptr, (size_t)half);
			memcpy(cache->data, lptr + half, (size_t)(size - half));
		}
	}

	return size;
}

/* get data from given position */
ilong iring_get(const struct IRING *cache, ilong pos, void *data, ilong len)
{
	char *lptr = (char*)data;
	ilong dsize, nsize, size;
	ilong tail, half;

	assert(pos >= 0 && len >= 0);

	nsize = pos + len;
	dsize = IRING_DSIZE(cache);

	if (nsize > dsize) 
		nsize = dsize;

	if (nsize <= pos) 
		return 0;

	size = nsize - pos;

	tail = cache->tail + pos;
	if (tail >= cache->size) tail -= cache->size;

	half = cache->size - tail;

	if (half >= size) {
		memcpy(lptr, cache->data + tail, (size_t)size);
	}	else {
		memcpy(lptr, cache->data + tail, (size_t)half);
		memcpy(lptr + half, cache->data, (size_t)(size - half));
	}

	return size;
}


/* swap buffer ptr: returns 0 for successful, -1 for size error */
int iring_swap(struct IRING *cache, void *buffer, ilong size)
{
	ilong dsize;
	ilong retval;

	dsize = IRING_DSIZE(cache);
	if (dsize + 1 > size) return -1;
	retval = iring_read(cache, buffer, dsize);
	cache->data = (char*)buffer;
	cache->size = size;
	cache->head = dsize;
	cache->tail = 0;
	return 0;
}


/* ring buffer ptr info */
ilong iring_ptr(struct IRING *ring, char **p1, ilong *s1, char **p2, 
	ilong *s2)
{
	ilong dsize = IRING_DSIZE(ring);

	if (ring->tail + dsize <= ring->size) {
		p1[0] = ring->data + ring->tail;
		s1[0] = dsize;
		p2[0] = NULL;
		s2[0] = 0;
	}	else {
		p1[0] = ring->data + ring->tail;
		s1[0] = ring->size - ring->tail;
		p2[0] = ring->data;
		s2[0] = ring->head;
	}

	return dsize;
}



/**********************************************************************
 * IMSTREAM: The struct definition of the memory stream descriptor
 **********************************************************************/

struct IMSPAGE
{
	struct IQUEUEHEAD head;
	iulong size;
	iulong index;
	unsigned char data[2];
};

#define IMSPAGE_LRU_SIZE	2


/* init memory stream */
void ims_init(struct IMSTREAM *s, imemnode_t *fnode, ilong low, ilong high)
{
	ilong swap;
	iqueue_init(&s->head);
	iqueue_init(&s->lru);
	s->fixed_pages = fnode;
	s->pos_read = 0;
	s->pos_write = 0;
	s->size = 0;
	low = low < 1024 ? 1024 : (low >= 0x10000 ? 0x10000 : low);
	high = high < 1024 ? 1024 : (high >= 0x10000 ? 0x10000 : high);
	if (low >= high) {
		swap = low;
		low = high;
		high = swap;
	}
	s->hiwater = high;
	s->lowater = low;
	s->lrusize = 0;
}

/* alloc new page from kmem-system or IMEMNODE */
static struct IMSPAGE *ims_page_new(struct IMSTREAM *s)
{
	struct IMSPAGE *page;
	ilong newsize, index;
	ilong oldsize;

	newsize = sizeof(struct IMSPAGE) + s->size;
	oldsize = newsize;
	newsize = newsize >= s->hiwater ? s->hiwater : newsize;
	newsize = newsize <= s->lowater ? s->lowater : newsize;

	if (s->fixed_pages != NULL) {
		index = imnode_new(s->fixed_pages);
		if (index < 0) return NULL;
		page = (struct IMSPAGE*)IMNODE_DATA(s->fixed_pages, index);
		page->index = (iulong)index;
		page->size = s->fixed_pages->node_size;
		page->size = page->size - sizeof(struct IMSPAGE);
	}	else {
		page = (struct IMSPAGE*)ikmem_malloc(newsize);
		if (page == NULL) return NULL;
		newsize = ikmem_ptr_size(page);
		page->index = 0xfffffffful;
		page->size = newsize - sizeof(struct IMSPAGE);
	}

	iqueue_init(&page->head);

	return page;
}

/* free page into kmem-system or IMEMNODE */
static void ims_page_del(struct IMSTREAM *s, struct IMSPAGE *page)
{
	if (s->fixed_pages != NULL) {
		assert(page->index != 0xfffffffful);
		imnode_del(s->fixed_pages, page->index);
	}	else {
		assert(page->index == 0xfffffffful);
		ikmem_free(page);
	}
}

/* destroy memory stream */
void ims_destroy(struct IMSTREAM *s)
{
	struct IMSPAGE *current;

	assert(s);

	for (; iqueue_is_empty(&s->head) == 0; ) {
		current = iqueue_entry(s->head.next, struct IMSPAGE, head);
		iqueue_del(&current->head);
		ims_page_del(s, current);
	}

	for (; iqueue_is_empty(&s->lru) == 0; ) {
		current = iqueue_entry(s->lru.next, struct IMSPAGE, head);
		iqueue_del(&current->head);
		ims_page_del(s, current);
	}

	s->pos_read = 0;
	s->pos_write = 0;
	s->size = 0;
	s->lrusize = 0;
}

/* get page from lru cache */
static struct IMSPAGE *ims_page_cache_get(struct IMSTREAM *s)
{
	struct IMSPAGE *page;
	ilong i;
	if (s->lrusize <= 0) {
		for (i = 0; i < IMSPAGE_LRU_SIZE; i++) {
			page = ims_page_new(s);
			iqueue_add_tail(&page->head, &s->lru);
			s->lrusize++;
		}
	}

	assert(s->lru.next != &s->lru);
	assert(s->lrusize > 0);

	page = iqueue_entry(s->lru.next, struct IMSPAGE, head);
	iqueue_del(&page->head);
	s->lrusize--;

	return page;
}

/* give page back to lru cache */
static void ims_page_cache_release(struct IMSTREAM *s, struct IMSPAGE *page)
{
	iqueue_add_tail(&page->head, &s->lru);
	s->lrusize++;
	for (; s->lrusize > (IMSPAGE_LRU_SIZE << 1); ) {
		page = iqueue_entry(s->lru.next, struct IMSPAGE, head);
		iqueue_del(&page->head);
		s->lrusize--;
		ims_page_del(s, page);
	}
}

/* get data size */
ilong ims_dsize(const struct IMSTREAM *s)
{
	return s->size;
}

/* write data into memory stream */
ilong ims_write(struct IMSTREAM *s, const void *ptr, ilong size)
{
	ilong total, canwrite, towrite;
	struct IMSPAGE *current;
	unsigned char *lptr;

	assert(s);

	if (size <= 0) return size;
	lptr = (unsigned char*)ptr;

	for (total = 0; size > 0; size -= towrite, total += towrite) {
		if (iqueue_is_empty(&s->head)) {
			current = NULL;
			canwrite = 0;
		}	else {
			current = iqueue_entry(s->head.prev, struct IMSPAGE, head);
			canwrite = current->size - s->pos_write;
		}
		if (canwrite == 0) {
			current = ims_page_cache_get(s);
			assert(current);
			iqueue_add_tail(&current->head, &s->head);
			s->pos_write = 0;
			canwrite = current->size;
		}
		towrite = (size <= canwrite)? size : canwrite;
		memcpy(current->data + s->pos_write, lptr, towrite);
		lptr += towrite;
		s->pos_write += towrite;
		s->size += towrite;
	}

	return total;
}

/* memory stream main read routine */
ilong ims_read_sub(struct IMSTREAM *s, void *ptr, ilong size, int nodrop)
{
	ilong total, canread, toread, posread;
	struct IQUEUEHEAD *head;
	struct IMSPAGE *current;
	unsigned char *lptr;

	assert(s);

	if (size <= 0) return size;
	lptr = (unsigned char*)ptr;
	posread = s->pos_read;
	head = s->head.next;

	for (total = 0; size > 0; size -= toread, total += toread) {
		if (head == &s->head) break;
		current = iqueue_entry(head, struct IMSPAGE, head);
		head = head->next;
		if (head == &s->head) canread = s->pos_write - posread;
		else canread = current->size - posread;
		toread = (size <= canread)? size : canread;
		if (toread == 0) break;
		if (lptr) {
			memcpy(lptr, current->data + posread, toread);
			lptr += toread;
		}
		posread += toread;
		if (posread >= (ilong)current->size) {
			posread = 0;
			if (nodrop == 0) {
				iqueue_del(&current->head);
				ims_page_cache_release(s, current);
				if (iqueue_is_empty(&s->head)) {
					s->pos_write = 0;
				}
			}
		}
		if (nodrop == 0) {
			s->size -= toread;
			s->pos_read = posread;
		}
	}
	return total;
}

/* read (and drop) data from memory stream */
ilong ims_read(struct IMSTREAM *s, void *ptr, ilong size)
{
	assert(s && ptr);
	return ims_read_sub(s, ptr, size, 0);
}

/* peek (no drop) data from memory stream */
ilong ims_peek(const struct IMSTREAM *s, void *ptr, ilong size)
{
	assert(s && ptr);
	return ims_read_sub((struct IMSTREAM*)s, ptr, size, 1);
}

/* drop data from memory stream */
ilong ims_drop(struct IMSTREAM *s, ilong size)
{
	assert(s);
	return ims_read_sub(s, NULL, size, 0);
}

/* clear stream */
void ims_clear(struct IMSTREAM *s)
{
	assert(s);
	ims_drop(s, s->size);
}

/* get flat ptr and size */
ilong ims_flat(const struct IMSTREAM *s, void **pointer)
{
	struct IMSPAGE *current;
	if (s->size == 0) {
		if (pointer) pointer[0] = NULL;
		return 0;
	}
	current = iqueue_entry(s->head.next, struct IMSPAGE, head);
	if (pointer) pointer[0] = current->data + s->pos_read;

	if (current->head.next != &s->head) 
		return current->size - s->pos_read;

	return s->pos_write - s->pos_read;
}


/**********************************************************************
 * common string operation (not be defined in some compiler)
 **********************************************************************/

/* strcasestr implementation */
char* istrcasestr(char* s1, char* s2)  
{  
	char* ptr = s1;  

	if (!s1 || !s2 || !*s2) return s1;  
	   
	while (*ptr) {  
		if (ITOUPPER(*ptr) == ITOUPPER(*s2)) {  
			char* cur1 = ptr + 1;  
			char* cur2 = s2 + 1;  
			while (*cur1 && *cur2 && ITOUPPER(*cur1) == ITOUPPER(*cur2)) {  
				cur1++;  
				cur2++;  
			}  
			if (!*cur2)  
				return ptr;  
		}  
		ptr++;  
	}  
	return   NULL;  
}

/* strncasecmp implementation */
int istrncasecmp(char* s1, char* s2, size_t num)
{
	char c1, c2;
	if(!s1|| !s2 || num<=0) return 0;
	assert(s1 && s2 && num>0);
	while(num > 0){
		c1 = ITOUPPER(*s1);
		c2 = ITOUPPER(*s2);
		if (c1 - c2) return c1 - c2;
		num--;
		s1++;
		s2++;
	}
	return 0;
}


/* strsep implementation */
char *istrsep(char **stringp, const char *delim)
{
	register char *s;
	register const char *spanp;
	register int c, sc;
	char *tok;

	if ((s = *stringp) == NULL)
		return (NULL);
	for (tok = s;;) {
		c = *s++;
		spanp = delim;
		do {
			if ((sc = *spanp++) == c) {
				if (c == 0) s = NULL;
				else s[-1] = 0;
				*stringp = s;
				return tok;
			}
		}	while (sc != 0);
	}
}

/* istrtoxl macro */
#define IFL_NEG			1
#define IFL_READDIGIT	2
#define IFL_OVERFLOW	4
#define IFL_UNSIGNED	8

/* istrtoxl implementation */
static unsigned long istrtoxl(const char *nptr, const char **endptr,
	int ibase, int flags)
{
	const char *p;
	char c;
	unsigned long number;
	unsigned long digval;
	unsigned long maxval;
	unsigned long limit;

	if (endptr != NULL) *endptr = nptr;
	assert(ibase == 0 || (ibase >= 2 && ibase <= 36));

	p = nptr;
	number = 0;

	c = *p++;
	while (isspace((int)(unsigned char)c)) c = *p++;

	if (c == '+') c = *p++;
	if (c == '-') {
		flags |= IFL_NEG;
		c = *p++;
	}

	if (c == '+') c = *p++;

	if (ibase < 0 || ibase == 1 || ibase > 36) {
		if (endptr) *endptr = nptr;
		return 0;
	}

	if (ibase == 0) {
		if (c != '0') ibase = 10;
		else if (*p == 'x' || *p == 'X') ibase = 16;
		else if (*p == 'b' || *p == 'B') ibase = 2;
		else ibase = 8;
	}

	if (ibase == 16) {
		if (c == '0' && (*p == 'x' || *p == 'X')) {
			p++;
			c = *p++;
		}
	}
	else if (ibase == 2) {
		if (c == '0' && (*p == 'b' || *p == 'B')) {
			p++;
			c = *p++;
		}
	}

	maxval = (~0ul) / ibase;

	for (; ; ) {
		if (isdigit((int)(unsigned char)c)) digval = c - '0';
		else if (isalpha((int)(unsigned char)c)) 
			digval = ITOUPPER(c) - 'A' + 10;
		else break;

		if (digval >= (unsigned long)ibase) break;

		flags |= IFL_READDIGIT;
	
		if (number < maxval || (number == maxval && 
			(unsigned long)digval <= ~0ul / ibase)) {
			number = number * ibase + digval;
		}	else {
			flags |= IFL_OVERFLOW;
			if (endptr == NULL) {
				break;
			}
		}

		c = *p++;
	}

	--p;

	limit = ((unsigned long)ILONG_MAX) + 1;

	if (!(flags & IFL_READDIGIT)) {
		if (endptr) *endptr = nptr;
		number = 0;
	}
	else if ((flags & IFL_UNSIGNED) && (flags & IFL_NEG)) {
		number = 0;
	}
	else if ((flags & IFL_OVERFLOW) || 
		(!(flags & IFL_UNSIGNED) &&
		(((flags & IFL_NEG) && (number > limit)) || 
		(!(flags & IFL_NEG) && (number > limit - 1))))) {
		if (flags & IFL_UNSIGNED) number = ~0ul;
		else if (flags & IFL_NEG) number = (unsigned long)ILONG_MIN;
		else number = (unsigned long)ILONG_MAX;
	}

	if (endptr) *endptr = p;

	if (flags & IFL_NEG)
		number = (unsigned long)(-(long)number);

	return number;
}

/* istrtoxl implementation */
static IUINT64 istrtoxll(const char *nptr, const char **endptr,
	int ibase, int flags)
{
	const char *p;
	char c;
	IUINT64 number;
	IUINT64 digval;
	IUINT64 maxval;
	IUINT64 limit;

	if (endptr != NULL) *endptr = nptr;
	assert(ibase == 0 || (ibase >= 2 && ibase <= 36));

	p = nptr;
	number = 0;

	c = *p++;
	while (isspace((int)(unsigned char)c)) c = *p++;

	if (c == '+') c = *p++;
	if (c == '-') {
		flags |= IFL_NEG;
		c = *p++;
	}

	if (c == '+') c = *p++;

	if (ibase < 0 || ibase == 1 || ibase > 36) {
		if (endptr) *endptr = nptr;
		return 0;
	}

	if (ibase == 0) {
		if (c != '0') ibase = 10;
		else if (*p == 'x' || *p == 'X') ibase = 16;
		else if (*p == 'b' || *p == 'B') ibase = 2;
		else ibase = 8;
	}

	if (ibase == 16) {
		if (c == '0' && (*p == 'x' || *p == 'X')) {
			p++;
			c = *p++;
		}
	}
	else if (ibase == 2) {
		if (c == '0' && (*p == 'b' || *p == 'B')) {
			p++;
			c = *p++;
		}
	}

	maxval = (~((IUINT64)0)) / ibase;

	for (; ; ) {
		if (isdigit((int)(unsigned char)c)) digval = c - '0';
		else if (isalpha((int)(unsigned char)c)) 
			digval = ITOUPPER(c) - 'A' + 10;
		else break;

		if (digval >= (IUINT64)ibase) break;

		flags |= IFL_READDIGIT;
	
		if (number < maxval || (number == maxval && 
			(IUINT64)digval <= ~0ul / ibase)) {
			number = number * ibase + digval;
		}	else {
			flags |= IFL_OVERFLOW;
			if (endptr == NULL) {
				break;
			}
		}

		c = *p++;
	}

	--p;

	limit = ((IUINT64)IINT64_MAX) + 1;

	if (!(flags & IFL_READDIGIT)) {
		if (endptr) *endptr = nptr;
		number = 0;
	}
	else if ((flags & IFL_UNSIGNED) && (flags & IFL_NEG)) {
		number = 0;
	}
	else if ((flags & IFL_OVERFLOW) || 
		(!(flags & IFL_UNSIGNED) &&
		(((flags & IFL_NEG) && (number > limit)) || 
		(!(flags & IFL_NEG) && (number > limit - 1))))) {
		if (flags & IFL_UNSIGNED) number = ~0ul;
		else if (flags & IFL_NEG) number = (IUINT64)IINT64_MIN;
		else number = (IUINT64)IINT64_MAX;
	}

	if (endptr) *endptr = p;

	if (flags & IFL_NEG)
		number = (IUINT64)(-(IINT64)number);

	return number;
}

/* implementation ixtoa */
static int ixtoa(IUINT64 val, char *buf, unsigned radix, int is_neg)
{
	IUINT64 digval;
	char *firstdig, *p;
	char temp;
	int size = 0;

	p = buf;
	if (is_neg) {
		if (buf) *p++ = '-';
		size++;
		val = (IUINT64)(-(IINT64)val);
	}

	firstdig = p;

	do {
		digval = (IUINT64)(val % (int)radix);
		val /= (int)radix;
		if (digval > 9 && buf) *p++ = (char)(digval - 10 + 'a');
		else if (digval <= 9 && buf) *p++ = (char)(digval + '0');
		size++;
	}	while (val > 0);

	if (buf == NULL) {
		return size;
	}

	*p-- = '\0';
	do { 
		temp = *p;
		*p = *firstdig;
		*firstdig = temp;
		--p;
		++firstdig;
	}	while (firstdig < p);

	return 0;
}

/* istrtol implementation */
long istrtol(const char *nptr, const char **endptr, int ibase)
{
	return (long)istrtoxl(nptr, endptr, ibase, 0);
}

/* istrtoul implementation */
unsigned long istrtoul(const char *nptr, const char **endptr, int ibase)
{
	return istrtoxl(nptr, endptr, ibase, IFL_UNSIGNED);
}

/* istrtoll implementation */
IINT64 istrtoll(const char *nptr, const char **endptr, int ibase)
{
	return (IINT64)istrtoxll(nptr, endptr, ibase, 0);
}

/* istrtoull implementation */
IUINT64 istrtoull(const char *nptr, const char **endptr, int ibase)
{
	return istrtoxll(nptr, endptr, ibase, IFL_UNSIGNED);
}

/* iltoa implementation */
int iltoa(long val, char *buf, int radix)
{
	IINT64 mval = val;
	return ixtoa((IUINT64)mval, buf, (unsigned)radix, (val < 0)? 1 : 0);
}

/* iultoa implementation */
int iultoa(unsigned long val, char *buf, int radix)
{
	IUINT64 mval = (IUINT64)val;
	return ixtoa(mval, buf, (unsigned)radix, 0);
}

/* iltoa implementation */
int illtoa(IINT64 val, char *buf, int radix)
{
	IUINT64 mval = (IUINT64)val;
	return ixtoa(mval, buf, (unsigned)radix, (val < 0)? 1 : 0);
}

/* iultoa implementation */
int iulltoa(IUINT64 val, char *buf, int radix)
{
	return ixtoa(val, buf, (unsigned)radix, 0);
}


/* istrstrip implementation */
char *istrstrip(char *ptr, const char *delim)
{
	size_t size, i;
	char *p = ptr;
	const char *spanp;

	assert(ptr && delim);

	size = strlen(ptr);

	while (size > 0) {
		for (spanp = delim; *spanp; spanp++) {
			if (*spanp == ptr[size - 1]) break;
		}
		if (*spanp == '\0') break;
		size--;
	}
	
	ptr[size] = 0;

	while (p[0]) {
		for (spanp = delim; *spanp; spanp++) {
			if (*spanp == p[0]) break;
		}
		if (*spanp == '\0') break;
		p++;
	}

	if (p == ptr) return ptr;
	for (i = 0; p[i]; i++) ptr[i] = p[i];
	ptr[i] = '\0';

	return ptr;
}


/**********************************************************************
 * ivalue_t string library
 **********************************************************************/

/* get sub string */
ivalue_t *it_strsub(const ivalue_t *src, ivalue_t *dst, ilong start, 
	ilong endup)
{
	ilong size;

	if (dst == NULL) {
		return dst;
	}
	if (src == NULL) {
		it_sresize(dst, 0);
		return dst;
	}

	assert(it_type(src) == ITYPE_STR);
	assert(it_type(dst) == ITYPE_STR);

	if (start < 0) start = it_size(src) + start;
	if (start < 0) start = 0;
	if (endup < 0) endup = it_size(src) + endup;
	if (endup < 0) endup = 0;
	if (endup > (ilong)it_size(src)) endup = (ilong)it_size(src);

	if (start >= endup) {
		it_sresize(dst, 0);
	}	else {
		size = (ilong)(endup - start);
		it_sresize(dst, size);
		memcpy(it_str(dst), it_str(src) + start, size);
	}

	return dst;
}

/* string compare */
static int it_strcmpx(const ivalue_t *src, const ivalue_t *str, ilong start, 
	int incase)
{
	iulong minsize, size, i;
	const char *p1, *p2;
	int retval;

	assert(it_type(src) == ITYPE_STR);
	assert(it_type(str) == ITYPE_STR);

	if (start < 0) start = (ilong)it_size(src) + start;
	if (start < 0) start = 0;
	if (start > (ilong)it_size(src)) start = (ilong)it_size(src);

	size = it_size(src) - start;
	minsize = _imin(size, it_size(str));

	p1 = it_str(src) + start;
	p2 = it_str(str);
	retval = 0;
	
	if (incase == 0) {
		for (i = minsize; i > 0; i--, p1++, p2++) {
			if (*p1 > *p2) { retval = 1; break; }
			else if (*p1 < *p2) { retval = -1; break; }
		}
	}	else {
		char c1, c2;
		for (i = minsize; i > 0; i--, p1++, p2++) {
			c1 = ITOUPPER(*p1);
			c2 = ITOUPPER(*p2);
			if (c1 > c2) { retval = 1; break; }
			else if (c1 < c2) { retval = -1; break; }
		}
	}

	if (retval != 0 || size == it_size(str)) 
		return retval;

	if (size > it_size(str)) return 1;

	return -1;
}

/* string compare */
int it_strcmp(const ivalue_t *src, const ivalue_t *str, ilong start)
{
	return it_strcmpx(src, str, start, 0);
}

/* string compare case insensitive */
int it_stricmp(const ivalue_t *src, const ivalue_t *str, ilong start)
{
	return it_strcmpx(src, str, start, 1);
}

/* string compare with c string */
int it_strcmpc(const ivalue_t *src, const char *str, ilong start)
{
	ivalue_t tt;
	it_strref(&tt, str, (ilong)strlen(str));
	return it_strcmpx(src, &tt, start, 0);
}

/* string compare with c string (case insensitive) */
int it_stricmpc(const ivalue_t *src, const char *str, ilong start)
{
	ivalue_t tt;
	it_strref(&tt, str, (ilong)strlen(str));
	return it_strcmpx(src, &tt, start, 1);
}

/* it_strsep implementation */
int it_strsep(const ivalue_t *src, iulong *pos, ivalue_t *dst,
	const ivalue_t *sep)
{
	iulong current, size, endup, i;

	if (src == NULL || dst == NULL) return -1;
	if (it_type(src) != ITYPE_STR || it_type(dst) != ITYPE_STR) return -2;

	current = (pos != NULL)? *pos : 0;

	if (current > it_size(src)) {
		it_sresize(dst, 0);
		return -3;
	}

	if (sep == NULL || pos == NULL) {
		size = it_size(src) - current;
		it_sresize(dst, size);
		memcpy(it_str(dst), it_str(src) + current, size);
		it_cpy(dst, src);
		if (pos) *pos = it_size(src);
		return 0;
	}

	for (endup = current; endup < it_size(src); endup++) {
		for (i = 0; i < it_size(sep); i++) {
			if (it_str(sep)[i] == it_str(src)[endup]) break;
		}
		if (i < it_size(sep)) break;
	}

	size = endup - current;
	if (pos) *pos = (long)endup + 1;

	it_sresize(dst, size);
	memcpy(it_str(dst), it_str(src) + current, size);

	return 0;
}

/* it_strsepc implementation */
int it_strsepc(const ivalue_t *src, iulong *pos, ivalue_t *dst,
	const char *sep)
{
	ivalue_t vsep;
	assert(it_type(src) == ITYPE_STR);
	assert(it_type(dst) == ITYPE_STR);
	it_strref(&vsep, sep, (ilong)strlen(sep));
	return it_strsep(src, pos, dst, &vsep);
}


/* it_strstrip implementation */
ivalue_t *it_strstrip(ivalue_t *str, const ivalue_t *delim)
{
	iulong size, dlen, i, j, k;
	const char *span;
	char *ptr;

	if (it_type(str) != ITYPE_STR || it_type(delim) != ITYPE_STR) 
		return str;

	ptr = it_str(str);
	size = it_size(str);
	dlen = it_size(delim);
	span = it_str(delim);

	while (size > 0) {
		for (j = 0; j < dlen; j++) {
			if (span[j] == ptr[size - 1]) break;
		}
		if (j >= dlen) break;
		size--;
	}

	ptr[size] = 0;
	it_size(str) = size;

	for (i = 0, k = 0; i < size; i++) {
		for (j = 0; j < dlen; j++) {
			if (span[j] == ptr[i]) break;
		}
		if (j >= dlen) break;
		k++;
	}

	if (k > 0) {
		for (i = 0; i + k < size; i++) ptr[i] = ptr[i + k];
		ptr[i] = 0;
		size = size - k;
	}

	it_sresize(str, size);

	return str;
}

/* ivalue_t str strip */
ivalue_t *it_strstripc(ivalue_t *str, const char *delim)
{
	ivalue_t d;
	it_strref(&d, delim, (ilong)strlen(delim));
	return it_strstrip(str, &d);
}

/* find str in src */
static ilong it_strfindx(const ivalue_t *src, const ivalue_t *str,
	ilong start, ilong endup, int incase, int reverse)
{
	const char *p1 = it_str(src);
	const char *p2 = it_str(str);
	const char *pend, *p;
	iulong size, i;

	assert(it_type(src) == ITYPE_STR);
	assert(it_type(str) == ITYPE_STR);

	if (start < 0) start = (ilong)it_size(src) + start;
	if (start < 0) start = 0;
	if (endup < 0) endup = (ilong)it_size(src) + endup;
	if (endup < 0) endup = 0;
	if (endup > (ilong)it_size(src)) endup = (ilong)it_size(src);

	size = it_size(str);
	pend = p1 + endup;

	if (start + size > it_size(src) || start >= endup) 
		return -1;

	if (reverse == 0) {
		if (incase == 0) {
			for (p = p1 + start; p + size <= pend; p++) {
				if (memcmp(p, p2, size) == 0) 
					return (ilong)(p - p1);
			}
		}	else {
			for (p = p1 + start; p + size <= pend; p++) {
				for (i = 0; i < size; i++) {
					if (ITOUPPER(p[i]) != ITOUPPER(p2[i])) break;
				}
				if (i >= size) 
					return (ilong)(p - p1);
			}
		}
	}	
	else {
		if (incase == 0) {
			for (p = pend - size; p >= p1 + start; p--) {
				if (memcmp(p, p2, size) == 0) 
					return (ilong)(p - p1);
			}
		}	else {
			for (p = pend - size; p >= p1 + start; p--) {
				for (i = 0; i < size; i++) {
					if (ITOUPPER(p[i]) != ITOUPPER(p2[i])) break;
				}
				if (i >= size) 
					return (ilong)(p - p1);
			}
		}
	}

	return -1;
}

/* find str in src (s:start, e:endup) */
ilong it_strfind(const ivalue_t *src, const ivalue_t *str, ilong s, ilong e)
{
	return it_strfindx(src, str, s, e, 0, 0);
}

/* find str in src */
ilong it_strfind2(const ivalue_t *src, const ivalue_t *str, ilong start)
{
	return it_strfindx(src, str, start, it_size(src), 0, 0);
}

/* find str in src (s:start, e:endup) case insensitive */
ilong it_strfindi(const ivalue_t *src, const ivalue_t *str, ilong s, ilong e)
{
	return it_strfindx(src, str, s, e, 1, 0);
}

/* find str in src */
ilong it_strfindi2(const ivalue_t *src, const ivalue_t *str, ilong start)
{
	return it_strfindx(src, str, start, it_size(src), 1, 0);
}

/* c: find str in src */
ilong it_strfindc(const ivalue_t *src, const char *str, ilong s, ilong e)
{
	ivalue_t tt;
	it_strref(&tt, str, (long)strlen(str));
	return it_strfindx(src, &tt, s, e, 0, 0);
}

/* c: find str in src */
ilong it_strfindc2(const ivalue_t *src, const char *str, ilong start)
{
	return it_strfindc(src, str, start, it_size(src));
}

/* c: find str in src */
ilong it_strfindic(const ivalue_t *src, const char *str, ilong s, ilong e)
{
	ivalue_t tt;
	it_strref(&tt, str, (ilong)strlen(str));
	return it_strfindx(src, &tt, s, e, 1, 0);
}

/* c: find str in src */
ilong it_strfindic2(const ivalue_t *src, const char *str, ilong start)
{
	return it_strfindic(src, str, start, it_size(src));
}

/* find from right */
ilong it_strfindr(const ivalue_t *src, const ivalue_t *str, ilong s, ilong e)
{
	return it_strfindx(src, str, s, e, 0, 1);
}

/* find from right */
ilong it_strfindri(const ivalue_t *src, const ivalue_t *str, 
	ilong s, ilong e)
{
	return it_strfindx(src, str, s, e, 1, 1);
}


/* case change: change=0: upper, change=1: lowwer */
ivalue_t *it_strcase(ivalue_t *src, int change)
{
	char *p, *pe;
	assert(it_type(src) == ITYPE_STR);
	p = it_str(src);
	pe = it_str(src) + it_size(src);
	if (change == 0) {
		for (; p < pe; p++) {
			if (*p >= 'a' && *p <= 'a') *p -= 'a' - 'A';
		}
	}	else {
		for (; p < pe; p++) {
			if (*p >= 'A' && *p <= 'A') *p += 'a' - 'A';
		}
	}
	return src;
}

/* append integer */
ivalue_t *it_strappendl(ivalue_t *src, ilong val, int radix)
{
	char digit[20];
	assert(it_type(src) == ITYPE_STR);
	iltoa(val, digit, radix);
	it_strcatc2(src, digit);
	return src;
}

/* append unsigned int */
ivalue_t *it_strappendul(ivalue_t *src, iulong val, int radix)
{
	char digit[20];
	assert(it_type(src) == ITYPE_STR);
	iultoa(val, digit, radix);
	it_strcatc2(src, digit);
	return src;
}

/* set integer */
ivalue_t *it_strsetl(ivalue_t *src, ilong val, int radix)
{
	assert(it_type(src) == ITYPE_STR);
	it_sresize(src, 0);
	return it_strappendl(src, val, radix);
}

/* set unsigned int */
ivalue_t *it_strsetul(ivalue_t *src, iulong val, int radix)
{
	assert(it_type(src) == ITYPE_STR);
	it_sresize(src, 0);
	return it_strappendul(src, val, radix);
}


/* left just */
ivalue_t *it_strljust(ivalue_t *src, iulong width, char fill)
{
	iulong size = it_size(src);
	assert(it_type(src) == ITYPE_STR);
	if (size < width) {
		it_sresize(src, width);
		memset(it_str(src) + size, fill, width - size);
	}
	return src;
}

/* right just */
ivalue_t *it_strrjust(ivalue_t *src, iulong width, char fill)
{
	iulong size = it_size(src);
	assert(it_type(src) == ITYPE_STR);
	if (size < width) {
		it_sresize(src, width);
		memmove(it_str(src) + width - size, it_str(src), size);
		memset(it_str(src), fill, width - size);
	}
	return src;
}

/* middle just */
ivalue_t *it_strmiddle(ivalue_t *src, iulong width, char fill)
{
	iulong size = it_size(src);
	assert(it_type(src) == ITYPE_STR);
	if (size < width) {
		it_strljust(src, size + (width - size) / 2, fill);
		it_strrjust(src, width, fill);
	}
	return src;
}



/**********************************************************************
 * BASE64
 **********************************************************************/

/* encode data as a base64 string, returns string size,
   if dst == NULL, returns how many bytes needed for encode (>=real) */
ilong ibase64_encode(const void *src, ilong size, char *dst)
{
	const unsigned char *s = (const unsigned char*)src;
	static const char encode[] = 
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	iulong c;
	char *d = dst;
	int i;

	/* returns nbytes needed */
	if (src == NULL || dst == NULL) {
		ilong nchars, result;
		nchars = ((size + 2) / 3) * 4;
		result = nchars + ((nchars - 1) / 76) + 1;
		return result;
	}

	for (i = 0; i < size; ) {
		c = s[i];
		c <<= 8;
		i++;
		c += (i < size)? s[i] : 0;
		c <<= 8;
		i++;
		c += (i < size)? s[i] : 0;
		i++;
		*d++ = encode[(c >> 18) & 0x3f];
		*d++ = encode[(c >> 12) & 0x3f];
		*d++ = (i > (size + 1))? '=' : encode[(c >> 6) & 0x3f];
		*d++ = (i > (size + 0))? '=' : encode[(c >> 0) & 0x3f];
	}

	d[0] = '\0';

	return (ilong)(d - dst);
}


/* decode a base64 string into data, returns data size 
   if dst == NULL, returns how many bytes needed for decode (>=real) */
ilong ibase64_decode(const char *src, ilong size, void *dst)
{
	static iulong decode[128] = { 0xff };
	const unsigned char *s = (const unsigned char*)src;
	unsigned char *d = (unsigned char*)dst;
	iulong mark, i, j, c, k;
	char b[3];

	if (src == NULL || dst == NULL) {
		ilong nbytes;
		nbytes = ((size + 7) / 4) * 3;
		return nbytes;
	}

	if (decode[0] == 0xff) {
		for (i = 1; i < 128; i++) {
			if (i >= 'A' && i <= 'Z') decode[i] = i - 'A';
			else if (i >= 'a' && i <= 'z') decode[i] = i - 'a' + 26;
			else if (i >= '0' && i <= '9') decode[i] = i - '0' + 52;
			else if (i == '+') decode[i] = 62;
			else if (i == '/') decode[i] = 63;
			else if (i == '=') decode[i] = 64;
			else decode[i] = 88;
		}
		decode[0] = 88;
	}

	#define ibase64_skip(s, i, size) {					\
			for (; i < size; i++) {						\
				if (decode[s[i] & 0x7f] <= 64) break;	\
			}											\
			if (i >= size) { i = size + 1; break; } 	\
		}
	
	for (i = 0, j = 0, k = 0; i < (iulong)size; ) {
		mark = 0;
		c = 0;

		ibase64_skip(s, i, (iulong)size);
		c += decode[s[i] & 0x7f];
		c <<= 6;
		i++;
		
		ibase64_skip(s, i, (iulong)size);
		c += decode[s[i] & 0x7f];
		c <<= 6;
		i++;

		ibase64_skip(s, i, (iulong)size);
		if (s[i] != '=') {
			c += decode[s[i] & 0x7f];
			c <<= 6;
			i++;
			ibase64_skip(s, i, (iulong)size);
			if (s[i] != '=') {
				c += decode[s[i] & 0x7f];
				i++;
			}	else {
				i = size;
				mark = 1;
			}
		}	else {
			i = size;
			mark = 2;
			c <<= 6;
		}

		b[0] = (unsigned char)((c >> 16) & 0xff);
		b[1] = (unsigned char)((c >>  8) & 0xff);
		b[2] = (unsigned char)((c >>  0) & 0xff);

		for (j = 0; j < 3 - mark; j++) {
			d[k++] = b[j];
		}
	}

	return (ilong)k;
}



/**********************************************************************
 * RC4
 **********************************************************************/

/* rc4 init */
void icrypt_rc4_init(unsigned char *box, int *x, int *y, 
	const unsigned char *key, int keylen)
{
	int X, Y, i, j, k, a;
	if (keylen <= 0 || key == NULL) {
		X = -1;
		Y = -1;
	}	else {
		X = Y = 0;
		j = k = 0;
		for (i = 0; i < 256; i++) {
			box[i] = (unsigned char)i;
		}
		for (i = 0; i < 256; i++) {
			a = box[i];
			j = (unsigned char)(j + a + key[k]);
			box[i] = box[j];
			box[j] = a;
			if (++k >= keylen) k = 0;
		}
	}
	x[0] = X;
	y[0] = Y;
}

/* rc4_crypt */
void icrypt_rc4_crypt(unsigned char *box, int *x, int *y, 
	const unsigned char *src, unsigned char *dst, ilong size)
{
	int X = x[0];
	int Y = y[0];
	if (X < 0 || Y < 0) {			// 不加密的情况
		if (src != dst) {
			memmove(dst, src, size);
		}
	}
	else {							// 加密的情况
		int a, b; 
		for (; size > 0; src++, dst++, size--) {
			X = (unsigned char)(X + 1);
			a = box[X];
			Y = (unsigned char)(Y + a);
			box[X] = box[Y];
			b = box[Y];
			box[Y] = a;
			dst[0] = src[0] ^ box[(unsigned char)(a + b)];
		}
		x[0] = X;
		y[0] = Y;
	}
}


