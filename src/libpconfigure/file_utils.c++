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

#include "file_utils.h++"
#include "vector_util.h++"
#include <cstdio>
#include <sstream>
using namespace file_utils;

#ifndef LINE_MAX
#define LINE_MAX 1024
#endif

std::vector<struct line_and_number> file_utils::readlines_and_numbers(FILE *f)
{
    char line[LINE_MAX];
    size_t num = 1;
    std::vector<struct line_and_number> out;

    while (std::fgets(line, LINE_MAX, f) != NULL) {
        struct line_and_number lan;
        lan.line = line;
        lan.number = num;
        out.push_back(lan);
        num++;
    }

    return out;
}

std::vector<std::string> file_utils::readlines(FILE *f)
{
    return vector_util::map(readlines_and_numbers(f),
                            [](struct line_and_number ln)
                            {
                                return ln.line;
                            });
}

std::vector<std::string>
file_utils::execlines(std::string path, std::vector<std::string> args)
{
    std::ostringstream cmd;
    cmd << "\"" + path + "\"" + " ";
    for (const auto& arg: args)
        cmd << "\"" << arg << "\"" << " ";

    std::vector<std::string> out;
    auto file = popen(cmd.str().c_str(), "r");
    char *lineptr = NULL;
    size_t n = 0;
    while (getline(&lineptr, &n, file) > 0) {
        auto line = std::string(lineptr);
        line = line.substr(0, line.size()-1);
        out.push_back(line);
    }

    free(lineptr);
    if (pclose(file) != 0)
        abort();

    return out;
}
