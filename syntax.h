#ifndef SHELL_SYNTAX_H
#define SHELL_SYNTAX_H

#include "error_handler.h"

enum Operation
{
    OP_EOF,
    OP_COM, //command
    OP_CONJ, //conjunction
    OP_DISJ, //disjunction
    OP_SEMI, //";" symbol
    OP_ENDL, //end of line
    OP_PIPE, //pipeline
    OP_PARA, //parallel run, "&"
    OP_OUT, //redirection of output
    OP_APP, //redirection of output to append
    OP_INP, //redirection of input
    OP_LBR, //opening bracket
    OP_RBR, //closing bracket
    INV_OP /*
 * invalid operation, used to determine the text entity,
 * that means the command name, arguments or a redirection file
 */
};

enum
{
    INIT_ARGC = 6
};
// the initial amount of a command's arguments

struct redirector
{
    char *file;
    int exists;
};

struct RedirectionHandle
{
    struct redirector out;
    struct redirector in;
    struct redirector append;
    int need_redirect;
};

struct ExpressionTree
{
    enum Operation opcode;
    struct ExpressionTree *left;
    struct ExpressionTree *right;
    struct RedirectionHandle redirect;
    char **argv;
    long long argc;
    long long cur_argc;
};

struct SuperStorage
{
    struct ExpressionTree *parsing_tree;
    char *string;
    unsigned long long position;
    ErrorContainer container;
};

void
delete_expression_tree(struct ExpressionTree *parse_tree, struct SuperStorage *storage);
// frees up the memory used by shell

struct SuperStorage
syntax_analyse(char *str);
// analyses the given expression, converting it into tree

struct SuperStorage
saver(struct SuperStorage *storage);
// memorizes the given struct data if it's not NULL
// if NULL, returns the last saved struct

#endif
