/*
 * Copyright (C) 2015 Palmer Dabbelt
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

#ifndef LIBMAKEFILE__TARGET_HXX
#define LIBMAKEFILE__TARGET_HXX

#include <memory>

namespace makefile {
    /* A fully-generated Makefile target, which contains a large batch of  */
    class target {
    public:
        typedef std::shared_ptr<target> ptr;

    private:

    public:

    public:
        /* Writes this target (and with its commands) to the given
         * file. */
        void write_to_file(FILE *file);
    };
}

#endif
