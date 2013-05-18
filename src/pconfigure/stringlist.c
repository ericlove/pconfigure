
/*
 * Copyright (C) 2011 Palmer Dabbelt
 *   <palmer@dabbelt.com>
 *
 * This file is part of tek.
 * 
 * tek is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * tek is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with tek.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "stringlist.h"

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_TALLOC
#include <talloc.h>
#else
#include "extern/talloc.h"
#endif

struct stringlist *stringlist_new(void *context)
{
    struct stringlist *l;

    l = talloc(context, struct stringlist);
    l->head = NULL;

    return l;
}

int stringlist_add(struct stringlist *l, const char *to_add)
{
    if (l->head == NULL) {
        l->head = talloc(l, struct stringlist_node);
        l->head->next = NULL;
        l->head->data = talloc_reference(l->head, to_add);
    }
    else {
        struct stringlist_node *cur;

        cur = l->head;
        while (cur->next != NULL)
            cur = cur->next;

        cur->next = talloc(l, struct stringlist_node);
        cur->next->next = NULL;
        cur->next->data = talloc_reference(cur->next, to_add);
    }

    return 0;
}

struct stringlist *stringlist_copy(struct stringlist *l, void *context)
{
    struct stringlist *new;
    struct stringlist_node *cur;

    new = stringlist_new(context);
    if (new == NULL)
        return NULL;

    cur = stringlist_start(l);
    while (stringlist_notend(cur)) {
        stringlist_add(new, stringlist_data(cur));
        cur = stringlist_next(cur);
    }

    return new;
}

int stringlist_del(struct stringlist *l, const char *to_del)
{
    struct stringlist_node *cur, *prev;

    prev = NULL;
    cur = l->head;
    while (cur != NULL) {
        if (strcmp(cur->data, to_del) == 0)
            break;

        prev = cur;
        cur = cur->next;
    }

    if (cur == NULL)
        return -1;

    if (prev == NULL) {
        struct stringlist_node *old;

        old = l->head;
        l->head = old->next;
        talloc_free(old);
    }
    else {
        struct stringlist_node *old;

        old = cur;
        prev->next = old->next;
        talloc_free(old);
    }

    return 0;
}

bool stringlist_include(struct stringlist * l, const char *s)
{
    struct stringlist_node *cur;

    cur = l->head;
    while (cur != NULL) {
        if (strcmp(cur->data, s) == 0)
            return true;

        cur = cur->next;
    }

    return false;
}

int stringlist_size(struct stringlist *l)
{
    struct stringlist_node *cur;
    int count;

    cur = l->head;
    count = 0;
    while (cur != NULL) {
        count++;
        cur = cur->next;
    }

    return count;
}

int stringlist_each(struct stringlist *l, int (*func) (const char *))
{
    struct stringlist_node *cur;

    cur = l->head;
    while (cur != NULL) {
        int out;
        out = func(cur->data);
        if (out != 0)
            return out;

        cur = cur->next;
    }

    return 0;
}

const char *stringlist_hashcode(struct stringlist *l, void *context)
{
    unsigned int hash;
    struct stringlist_node *cur;

    hash = 5381;
    cur = l->head;
    while (cur != NULL) {
        const char *str;
        char c;

        str = cur->data;
        /* FIXME: http://www.cse.yorku.ca/~oz/hash.html */
        while ((c = *str++) != '\0')
            hash = hash * 33 ^ c;
        hash = hash * 33 ^ ' ';

        cur = cur->next;
    }

    return talloc_asprintf(context, "%u", hash);
}
