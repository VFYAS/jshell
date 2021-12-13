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
    OP_PIPE, //pipeline
    OP_PARA, //parallel run
    OP_OUT, //redirection of output
    OP_APP, //redirection of output on append
    OP_INP, //redirection of input
    OP_LBR, //opening bracket
    OP_RBR, //closing bracket
    INV_OP //invalid operation
};

enum
{
    INIT_ARGC = 6
};

struct redirector
{
    char *file;
    int exists;
};

typedef struct
{
    struct redirector out;
    struct redirector in;
    struct redirector app;
} RedirectionHandle;

typedef struct ExpressionTree
{
    enum Operation opcode;
    struct ExpressionTree *left;
    struct ExpressionTree *right;
    RedirectionHandle *redirect;
    char **argv;
    long long argc;
    long long cur_argc;
} ExpressionTree;

typedef struct Utils
{
    ExpressionTree *parsing_tree;
    ExpressionTree *separate_tree;
    const char *string;
    unsigned long long position;
    ErrorContainer container;
} Utils;

void
delete_expression_tree(ExpressionTree *parse_tree, Utils *utils);
// frees up the memory used by shell

Utils
syntax_analyse(const char *str);
// analyses the given expression, converting it into tree

#endif
