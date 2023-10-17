/*
 * kvlist - simple key/value store
 *
 * Copyright (C) 2021 FMSoft <https://www.fmsoft.cn>
 * Copyright (C) 2014 Felix Fietkau <nbd@openwrt.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "avl-cmp.h"
#include "kvlist.h"

int kvlist_strlen(struct kvlist *kv, const void *data)
{
    (void) kv;
    return strlen(data) + 1;
}

void kvlist_init(struct kvlist *kv,
        int (*get_len)(struct kvlist *kv, const void *data))
{
    avl_init(&kv->avl, avl_strcmp, false, NULL);
    kv->get_len = get_len;
}

static struct kvlist_node *__kvlist_get(struct kvlist *kv, const char *name)
{
    struct kvlist_node *node;

    return avl_find_element(&kv->avl, name, node, avl);
}

void *kvlist_get(struct kvlist *kv, const char *name)
{
    struct kvlist_node *node;

    node = __kvlist_get(kv, name);
    if (!node)
        return NULL;

    return node->data;
}

bool kvlist_delete(struct kvlist *kv, const char *name)
{
    struct kvlist_node *node;

    node = __kvlist_get(kv, name);
    if (node) {
        avl_delete(&kv->avl, &node->avl);
        free(node);
    }

    return !!node;
}

const char *kvlist_set_ex(struct kvlist *kv, const char *name, const void *data)
{
    struct kvlist_node *node;
    char *name_buf;
    int len = kv->get_len ? kv->get_len(kv, data) : (int)(sizeof (void *));

    node = calloc_a(sizeof(struct kvlist_node) + len,
        &name_buf, strlen(name) + 1);
    if (!node)
        return NULL;

    kvlist_delete(kv, name);

    memcpy(node->data, data, len);

    node->avl.key = strcpy(name_buf, name);
    avl_insert(&kv->avl, &node->avl);

    return node->avl.key;
}

void kvlist_free(struct kvlist *kv)
{
    struct kvlist_node *node, *tmp;

    avl_remove_all_elements(&kv->avl, node, avl, tmp)
        free(node);
}

#define foreach_arg(_arg, _addr, _len, _first_addr, _first_len) \
    for (_addr = (_first_addr), _len = (_first_len); \
        _addr; \
        _addr = va_arg(_arg, void **), _len = _addr ? va_arg(_arg, size_t) : 0)

#define C_PTR_ALIGN    (sizeof(size_t))
#define C_PTR_MASK     (-C_PTR_ALIGN)

void *mc_calloc_a(size_t len, ...)
{
    va_list ap, ap1;
    void *ret;
    void **cur_addr;
    size_t cur_len;
    int alloc_len = 0;
    char *ptr;

    va_start(ap, len);

    va_copy(ap1, ap);
    foreach_arg(ap1, cur_addr, cur_len, &ret, len)
        alloc_len += (cur_len + C_PTR_ALIGN - 1 ) & C_PTR_MASK;
    va_end(ap1);

    ptr = calloc(1, alloc_len);
    if (!ptr) {
        va_end(ap);
        return NULL;
    }

    alloc_len = 0;
    foreach_arg(ap, cur_addr, cur_len, &ret, len) {
        *cur_addr = &ptr[alloc_len];
        alloc_len += (cur_len + C_PTR_ALIGN - 1) & C_PTR_MASK;
    }
    va_end(ap);

    return ret;
}

