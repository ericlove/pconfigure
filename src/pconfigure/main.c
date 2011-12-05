
/*
 * Copyright (C) 2011 Daniel Dabbelt
 *   <palmem@comcast.net>
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>

#include "defaults.h"
#include "error.h"
#include "languages.h"
#include "target_stack.h"
#include "string_list.h"
#include "makefile.h"

#define FREE(x) {free(x); x = NULL;}

/* Our targets live in a stack */
static struct target_stack target_stack;

/* This is the actual output */
static struct makefile makefile;

/* Functions for parsing the different types of options availiable in pconfigure
   files. */
typedef enum error (*parsefunc) (char *, char *);

static enum error parsefunc_languages(char *, char *);
static enum error parsefunc_prefix(char *, char *);
static enum error parsefunc_compileopts(char *, char *);
static enum error parsefunc_linkopts(char *, char *);
static enum error parsefunc_binaries(char *, char *);
static enum error parsefunc_sources(char *, char *);

/* Selects the correct parsing function to use, calls it, and gets returns
   what it returns */
static enum error select_parsefunc(char *, char *, char *);

/* Parses an entire file, returning 0 on success, 1 on failure, and 2 on
   file-not-found (or otherwise not readable) */
static enum error parse_file(const char *file_name);
static enum error parse_line(const char *line, char *left, char *op,
                             char *right);

int main(int argc, char **argv)
{
    enum error err;

    /* Checks if we were called with any arguments, and if so dies */
    if (argc != 1)
    {
        fprintf(stderr, "%s: Palmer's Makefile Generator\n", argv[0]);
        fprintf(stderr, "\tThis should be called with a Configfile this dir");
        return 1;
    }

    /* Starts all the sub-modules required by this code */
    defaults_boot();
    string_list_boot();
    languages_boot();
    target_boot();
    target_stack_boot();
    makefile_boot();

    /* Initializes an empty makefile */
    err = makefile_init(&makefile);
    if (err != ERROR_NONE)
    {
        fprintf(stderr, "pconfigure initialization failed\n"
                "%d: %s\n", err, error_to_string(err));
        return err;
    }

    /* Initializes an empty target stack */
    err = target_stack_init(&target_stack);
    if (err != ERROR_NONE)
    {
        fprintf(stderr, "pconfigure initialization failed\n"
                "%d: %s\n", err, error_to_string(err));
        return err;
    }

    /* Parses the two given files */
    err = parse_file(DEFAULT_INFILE_LOCAL);
    if ((err != ERROR_FILE_NOT_FOUND) && (err != ERROR_NONE))
    {
        fprintf(stderr, "pconfigure failed on file %s\n",
                DEFAULT_INFILE_LOCAL);
        fprintf(stderr, "error %d: %s\n", err, error_to_string(err));
        return err;
    }

    err = parse_file(DEFAULT_INFILE);
    if (err != ERROR_NONE)
    {
        fprintf(stderr, "pconfigure failed on file %s\n"
                "error %d: %s\n", DEFAULT_INFILE, err, error_to_string(err));
        return err;
    }

    /* Cleans up the mess we made */
    err = target_stack_clear(&target_stack);
    if (err != ERROR_NONE)
    {
        fprintf(stderr, "target_stack_clear failed\n"
                "error %d: %s\n", err, error_to_string(err));
        return err;
    }
    err = makefile_clear(&makefile);
    if (err != ERROR_NONE)
    {
        fprintf(stderr, "makefile_clear failed\n"
                "error %d: %s\n", err, error_to_string(err));
        return err;
    }

    /* Halts what was booted */
    languages_halt();

    return err;
}

/*
 *	File main loop and string tokenizer
 */
enum error parse_file(const char *file_name)
{
    FILE *file;
    char line[MAX_LINE_SIZE];
    char left[MAX_LINE_SIZE], op[MAX_LINE_SIZE], right[MAX_LINE_SIZE];
    unsigned int line_num;

    file = fopen(file_name, "r");
    if (file == NULL)
        return 2;

    /* Reads the entire file */
    line_num = 1;
    while (fgets(line, MAX_LINE_SIZE, file) != NULL)
    {
        enum error err;

        err = parse_line(line, left, op, right);
        if (err != 0)
        {
            fprintf(stderr, "Read error %d on line %d of %s:\n\t%s",
                    err, line_num, file_name, line);
            return err;
        }

        err = select_parsefunc(left, op, right);
        if (err != 0)
        {
            fprintf(stderr, "Parse erro on line %d of %s:\n\t%s",
                    line_num, file_name, line);
            return err;
        }

        line_num++;
    }

    fclose(file);

    return 0;
}

enum error parse_line(const char *line, char *left, char *op, char *right)
{
    int line_cur, left_cur, op_cur, right_cur;

    line_cur = left_cur = op_cur = right_cur = 0;
    memset(left, 0, MAX_LINE_SIZE);
    memset(op, 0, MAX_LINE_SIZE);
    memset(right, 0, MAX_LINE_SIZE);

    /* Removes all leading whitespace */
    while ((line[line_cur] != '\0') && isspace(line[line_cur]))
        line_cur++;

    /* Splits into 3 parts */
    while ((line[line_cur] != '\0') && !isspace(line[line_cur]))
    {
        assert(left_cur < MAX_LINE_SIZE);
        left[left_cur] = line[line_cur];
        left_cur++;
        line_cur++;
    }

    while ((line[line_cur] != '\0') && isspace(line[line_cur]))
        line_cur++;

    while ((line[line_cur] != '\0') && !isspace(line[line_cur]))
    {
        assert(op_cur < MAX_LINE_SIZE);
        op[op_cur] = line[line_cur];
        op_cur++;
        line_cur++;
    }

    while ((line[line_cur] != '\0') && isspace(line[line_cur]))
        line_cur++;

    while (line[line_cur] != '\0')
    {
        assert(right_cur < MAX_LINE_SIZE);
        right[right_cur] = line[line_cur];
        right_cur++;
        line_cur++;
    }

    assert(right_cur != 1);
    right_cur--;

    while ((right_cur > 0) && (isspace(right[right_cur])))
    {
        right[right_cur] = '\0';
        right_cur--;
    }

    /* I suppose we always succeed? */
    return 0;
}

/* Function for parsing specific commands */
enum error select_parsefunc(char *left, char *op, char *right)
{
    if ((right[0] == '`') && (right[strlen(right) - 1] == '`'))
    {
        int tmpfd;
        char *tmpfile;
        char *exec;
        FILE *tmpf;

        right++;
        right[strlen(right) - 1] = '\0';

        tmpfile = strdup("/tmp/pconfXXXXXX");
        tmpfd = mkstemp(tmpfile);
        close(tmpfd);

        exec = malloc(strlen(right) + strlen(" >& ") + strlen(tmpfile) + 2);
        exec[0] = '\0';
        strcat(exec, right);
        strcat(exec, " >& ");
        strcat(exec, tmpfile);
        if (system(exec) != 0)
            fprintf(stderr, "'%s' failed, which is probably bad\n", right);

        tmpf = fopen(tmpfile, "r");
        unlink(tmpfile);
        assert(tmpf != NULL);
        if (fgets(right, MAX_LINE_SIZE - 4, tmpf) == NULL)
            return ERROR_NULL_POINTER;
        fclose(tmpf);

        while ((strlen(right) > 0) && (isspace(right[strlen(right) - 1])))
            right[strlen(right) - 1] = '\0';

        free(exec);
        free(tmpfile);
    }

    if (strlen(left) == 0)
        return 0;
    if (strcmp(left, "LANGUAGES") == 0)
        return parsefunc_languages(op, right);
    if (strcmp(left, "PREFIX") == 0)
        return parsefunc_prefix(op, right);
    if (strcmp(left, "COMPILEOPTS") == 0)
        return parsefunc_compileopts(op, right);
    if (strcmp(left, "LINKOPTS") == 0)
        return parsefunc_linkopts(op, right);
    if (strcmp(left, "BINARIES") == 0)
        return parsefunc_binaries(op, right);
    if (strcmp(left, "SOURCES") == 0)
        return parsefunc_sources(op, right);

    RETURN_UNIMPLEMENTED;
}

enum error parsefunc_languages(char *op, char *right)
{
    if (strcmp(op, "+=") == 0)
    {
        enum error err;

        err = languages_add(right);
        if (err != ERROR_NONE)
            return err;

        return ERROR_NONE;
    }

    fprintf(stderr, "Only += is supported for LANGUAGES, tried '%s'\n", op);
    return ERROR_ILLEGAL_OP;
}

enum error parsefunc_prefix(char *op, char *right)
{
    struct target *t;

    if (strcmp(op, "=") != 0)
    {
        fprintf(stderr, "Only = is supported for PREFIX, tried '%s'\n", op);
        return ERROR_ILLEGAL_OP;
    }

    t = target_stack_peek(&target_stack);
    if (t != NULL)
    {
        FREE(t->prefix);
        t->prefix = strdup(right);
        return ERROR_NONE;
    }
    else
    {
        FREE(default_context_prefix);
        default_context_prefix = strdup(right);
        return ERROR_NONE;
    }
}

enum error parsefunc_compileopts(char *op, char *right)
{
    struct language *last_lang;
    struct target *t;

    /* If a target has been added, add to that specifically */
    t = target_stack_peek(&target_stack);
    if (t != NULL)
    {
        if (strcmp(op, "+=") == 0)
            return string_list_add(t->compile_opts, right);
        if (strcmp(op, "-=") == 0)
            return string_list_del(t->compile_opts, right);
    }

    /* Tries to find the last added language */
    last_lang = languages_last_added();
    if (last_lang == NULL)
    {
        fprintf(stderr, "Called COMPILEOPTS before LANGUAGES or TARGETS\n");
        return ERROR_NULL_POINTER;
    }

    /* Adds the compile option to the last found language */
    if (strcmp(op, "+=") == 0)
        return string_list_add(last_lang->compile_opts, right);
    if (strcmp(op, "-=") == 0)
        return string_list_del(last_lang->compile_opts, right);

    /* All ops but += and -= are illegal */
    fprintf(stderr, "Unrecognized operation '%s' on COMPILEOPTS\n", op);
    return ERROR_ILLEGAL_OP;
}

enum error parsefunc_linkopts(char *op, char *right)
{
    struct language *last_lang;
    struct target *t;

    /* If a target has been added, add to that specifically */
    t = target_stack_peek(&target_stack);
    if (t != NULL)
    {
        if (strcmp(op, "+=") == 0)
            return string_list_add(t->link_opts, right);
        if (strcmp(op, "-=") == 0)
            return string_list_del(t->link_opts, right);
    }

    /* Tries to find the last added language */
    last_lang = languages_last_added();
    if (last_lang == NULL)
    {
        fprintf(stderr, "Called LINKOPTS before LANGUAGES or TARGETS\n");
        return ERROR_NULL_POINTER;
    }

    /* Adds the compile option to the last found language */
    if (strcmp(op, "+=") == 0)
        return string_list_add(last_lang->link_opts, right);
    if (strcmp(op, "-=") == 0)
        return string_list_del(last_lang->link_opts, right);

    /* All ops but += and -= are illegal */
    fprintf(stderr, "Unrecognized operation '%s' on COMPILEOPTS\n", op);
    return ERROR_ILLEGAL_OP;
}

enum error parsefunc_binaries(char *op, char *right)
{
    enum error err;
    struct target *t;

    /* The list of binaries can only be added to, not subtracted from */
    if (strcmp(op, "+=") != 0)
    {
        fprintf(stderr, "BINARIES only supports +=, not '%s'\n", op);
        return ERROR_ILLEGAL_OP;
    }

    /* Pops of a source and a binary */
    t = target_stack_peek(&target_stack);
    if (t != NULL && t->type != TARGET_TYPE_SOURCE)
    {
        fprintf(stderr, "Passed a target without a source\n");
        return ERROR_ILLEGAL_OP;
    }
    if (t != NULL)
        target_stack_pop(&target_stack);

    t = target_stack_peek(&target_stack);
    if (t != NULL && t->type != TARGET_TYPE_BINARY)
    {
        fprintf(stderr, "Multiple sources for a single target\n");
        return ERROR_INTERNAL_STACK;
    }
    if (t != NULL)
        target_stack_pop(&target_stack);

    /* Makes a new target, which will end up representing this binary */
    err = target_stack_push(&target_stack);
    if (err != ERROR_NONE)
        return err;

    /* Fills out the current target */
    t = target_stack_peek(&target_stack);
    ASSERT_RETURN(t != NULL, ERROR_NULL_POINTER);       /* No way to recover here */

    t->type = TARGET_TYPE_BINARY;
    t->passed_path = strdup(right);
    t->makefile = &makefile;

    return ERROR_NONE;
}

enum error parsefunc_sources(char *op, char *right)
{
    enum error err;
    struct target *t;

    /* The list of sources can only be added to, not subtracted from */
    if (strcmp(op, "+=") != 0)
    {
        fprintf(stderr, "SOURCES only supports +=, not '%s'\n", op);
        return ERROR_ILLEGAL_OP;
    }

    /* If we just grabbed a source, then drop it */
    t = target_stack_peek(&target_stack);
    ASSERT_RETURN(t != NULL, ERROR_NULL_POINTER);

    if (t->type == TARGET_TYPE_SOURCE)
    {
        err = target_stack_pop(&target_stack);
        if (err != ERROR_NONE)
            return err;
    }

    /* Ensures that the stack is proper */
    t = target_stack_peek(&target_stack);
    ASSERT_RETURN(t != NULL, ERROR_NULL_POINTER);
    ASSERT_RETURN(t->type == TARGET_TYPE_BINARY, ERROR_INTERNAL_STACK);

    /* Makes a new target, which will end up representing this binary */
    err = target_stack_push(&target_stack);
    if (err != ERROR_NONE)
        return err;

    /* Fills out the current target */
    t = target_stack_peek(&target_stack);
    ASSERT_RETURN(t != NULL, ERROR_NULL_POINTER);       /* No way to recover */

    t->type = TARGET_TYPE_SOURCE;
    t->passed_path = strdup(right);
    t->makefile = &makefile;

    return ERROR_NONE;
}
