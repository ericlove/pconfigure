
/*
 * Copyright (C) 2011 Palmer Dabbelt
 *   <palmer@dabbelt.com>
 *
 * This file is part of pconfigure.
 * 
 * pconfigure is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * pconfigure is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with pconfigure.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "cxx.h"
#include <string.h>
#include <unistd.h>

#ifdef HAVE_TALLOC
#include <talloc.h>
#else
#include "extern/talloc.h"
#endif

static struct language *language_asm_search(struct language *l_uncast,
                                            struct language *parent,
                                            const char *path,
                                            struct context *c);
static void language_asm_deps(struct language *l_uncast, struct context *c,
                              void (*func) (void *arg, const char *, ...),
                              void *);

struct language *language_asm_new(struct clopts *o, const char *name)
{
    struct language_c *l;

    if (strcmp(name, "asm") != 0)
        return NULL;

    l = language_c_new_uc(o, "c");
    if (l == NULL)
        return NULL;

    l->l.name = talloc_strdup(l, "asm");
    l->l.compiled = true;
    l->l.compile_str = talloc_strdup(l, "ASM");
    l->l.compile_cmd = talloc_strdup(l, "${CC} -xassembler");
    l->l.link_str = talloc_strdup(l, "LD");
    l->l.link_cmd = talloc_strdup(l, "${CC}");
    l->l.search = &language_asm_search;
    l->l.deps = &language_asm_deps;
    l->cflags = talloc_strdup(l, "$(ASMFLAGS)");

    return &(l->l);
}

struct language *language_asm_search(struct language *l_uncast,
                                     struct language *parent,
                                     const char *path, struct context *c)
{
    struct language_c *l;

    l = talloc_get_type(l_uncast, struct language_c);
    if (l == NULL)
        return NULL;

    if ((strcmp(path + strlen(path) - 2, ".s") != 0)
        && (strcmp(path + strlen(path) - 2, ".S") != 0))
        return NULL;

    if (parent == NULL)
        return l_uncast;

    if (strcmp(parent->link_name, l_uncast->link_name) != 0)
        return NULL;

    return l_uncast;
}

void language_asm_deps(struct language *l_uncast, struct context *c,
                       void (*func) (void *arg, const char *, ...), void *arg)
{
    struct language_c *l;

    l = talloc_get_type(l_uncast, struct language_c);
    if (l == NULL)
        abort();

    func(arg, "%s", c->full_path);
}
