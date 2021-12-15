#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "syntax.h"
#include "error_handler.h"

static struct ExpressionTree *
parse_seps(struct SuperStorage *storage);
// parses separators, that means ";", "&" and end of line

static struct ExpressionTree *
parse_logicals(struct SuperStorage *storage);
// parses logical expressions: conjunctions and disjunctions

static struct ExpressionTree *
parse_pipe(struct SuperStorage *storage);
// parses pipeline lists

static struct ExpressionTree *
parse_command(struct SuperStorage *storage);
// parses the command or the brackets expression

static enum Operation
parse_op(struct SuperStorage *storage);
// defines which operation is next

static struct RedirectionHandle
parse_redirects(struct SuperStorage *storage, struct RedirectionHandle redirect);
// parses redirections of i/o of commands

static void
skip_spaces(struct SuperStorage *storage, int skip_endls);

static void
skip_spaces(struct SuperStorage *storage, int skip_endls)
{
    unsigned long long auto_pos = storage->position;
    while (isspace(*(storage->string + auto_pos)) &&
           ((skip_endls == 0) == (*(storage->string + auto_pos) != '\n'))) {
        ++auto_pos;
    }
    storage->position = auto_pos;
}

static enum Operation
parse_op(struct SuperStorage *storage)
{
    skip_spaces(storage, 0);
    if (!storage->string[storage->position]) {
        return OP_EOF;
    }
    enum Operation opcode;
    switch (storage->string[storage->position]) {
    case '&':
        if (storage->string[storage->position + 1] == '&') {
            opcode = OP_CONJ;
            storage->position += 1;
        } else {
            opcode = OP_PARA;
        }
        break;
    case '|':
        if (storage->string[storage->position + 1] == '|') {
            opcode = OP_DISJ;
            storage->position += 1;
        } else {
            opcode = OP_PIPE;
        }
        break;
    case ';':
        opcode = OP_SEMI;
        break;
    case '<':
        opcode = OP_INP;
        break;
    case '>':
        if (storage->string[storage->position + 1] == '>') {
            opcode = OP_APP;
            storage->position += 1;
        } else {
            opcode = OP_OUT;
        }
        break;
    case ')':
        opcode = OP_RBR;
        break;
    case '\n':
        opcode = OP_ENDL;
        break;
    default:
        opcode = INV_OP;
        return opcode;
    }
    storage->position += 1;
    return opcode;
}

static struct RedirectionHandle
parse_redirects(struct SuperStorage *storage, struct RedirectionHandle redirect)
{
    /*
     * If one command has several redirections of the same kind,
     * then we only use the last one.
     */
    unsigned long long prev_pos = storage->position;
    enum Operation next_op;
    while ((next_op = parse_op(storage)) == OP_APP || next_op == OP_OUT || next_op == OP_INP) {
        struct redirector *curr_redirect;
        switch (next_op) {
        case OP_OUT:
            free(redirect.out.file);
            redirect.out.exists = redirect.need_redirect = 1;
            curr_redirect = &(redirect.out);
            break;
        case OP_INP:
            free(redirect.in.file);
            redirect.in.exists = redirect.need_redirect = 1;
            curr_redirect = &(redirect.in);
            break;
        case OP_APP:
            free(redirect.append.file);
            redirect.append.exists = redirect.need_redirect = 1;
            curr_redirect = &(redirect.append);
            break;
        default:
            set_error_number(storage, INTERNAL_ERROR);
            return redirect;
        }
        if (parse_op(storage) != INV_OP) {
            set_error_number(storage, NO_OPERAND);
            return redirect;
        }
        skip_spaces(storage, 0);

        struct SuperStorage runner_storage = {.position = 0, .string = storage->string + storage->position};
        while (parse_op(&runner_storage) == INV_OP && !isspace(*runner_storage.string)) {
            runner_storage.position = 0;
            ++runner_storage.string;
        }
        curr_redirect->file = strndup(storage->string + storage->position,
                                      runner_storage.string - storage->string - storage->position);
        if (curr_redirect->file == NULL) {
            set_error_number(storage, MEMORY_ERROR);
            return redirect;
        }
        storage->position = runner_storage.string - storage->string;
        prev_pos = storage->position;
    }
    storage->position = prev_pos;
    return redirect;
}

static struct ExpressionTree *
parse_command(struct SuperStorage *storage)
{
    struct ExpressionTree *res;
    skip_spaces(storage, 0);
    if (storage->string[storage->position] == '(') {
        storage->position += 1;
        struct ExpressionTree *term_tree = parse_seps(storage);
        skip_spaces(storage, 0);

        if (storage->string[storage->position] != ')') {
            delete_expression_tree(term_tree, storage);
            set_error_number(storage, BRACKETS_BALANCE);
            return NULL;
        }

        res = calloc(1, sizeof(*res));

        if (res == NULL) {
            delete_expression_tree(term_tree, storage);
            set_error_number(storage, MEMORY_ERROR);
            return NULL;
        }

        res->opcode = OP_LBR;
        res->left = term_tree;
        res->right = calloc(1, sizeof(*res->right));
        if (res->right == NULL) {
            delete_expression_tree(res, storage);
            set_error_number(storage, MEMORY_ERROR);
            return NULL;
        }

        res->right->opcode = OP_RBR;
        storage->position += 1;

        res->redirect = parse_redirects(storage, res->redirect);
        if (storage->container.err_happened) {
            delete_expression_tree(res, storage);
            return NULL;
        }
    } else {
        skip_spaces(storage, 0);
        unsigned long long prev_pos = storage->position;
        if (parse_op(storage) != INV_OP) {
            storage->position = prev_pos;
            return NULL;
        }
        storage->position = prev_pos;
        res = calloc(1, sizeof *res);
        if (res == NULL) {
            set_error_number(storage, MEMORY_ERROR);
            return NULL;
        }
        res->argc = INIT_ARGC;
        res->argv = calloc(res->argc, sizeof(*res->argv));

        if (res->argv == NULL) {
            set_error_number(storage, MEMORY_ERROR);
            delete_expression_tree(res, storage);
            return NULL;
        }

        res->cur_argc = 0;
        res->opcode = OP_COM;
        while (parse_op(storage) == INV_OP) {
            if (res->cur_argc == res->argc) {
                res->argc <<= 1;
                res->argv = realloc(res->argv, res->argc * sizeof(*res->argv));
                if (res->argv == NULL) {
                    set_error_number(storage, MEMORY_ERROR);
                    delete_expression_tree(res, storage);
                    return NULL;
                }
            }
            skip_spaces(storage, 0);
            struct SuperStorage runner_storage = {.position = 0, .string = storage->string + storage->position};
            while (parse_op(&runner_storage) == INV_OP && !isspace(*runner_storage.string)) {
                runner_storage.position = 0;
                ++runner_storage.string;
            }
            res->argv[res->cur_argc] = strndup(storage->string + storage->position,
                                               runner_storage.string - storage->string - storage->position);
            storage->position = runner_storage.string - storage->string;
            res->redirect = parse_redirects(storage, res->redirect);
            if (storage->container.err_happened) {
                delete_expression_tree(res, storage);
                return NULL;
            }
            prev_pos = storage->position;
            res->cur_argc += 1;
        }
        storage->position = prev_pos;
        res->argc = res->cur_argc + 1;
        res->argv = realloc(res->argv, res->argc * sizeof(*res->argv));
        if (res->argv == NULL) {
            res->argc = 0;
            set_error_number(storage, MEMORY_ERROR);
            delete_expression_tree(res, storage);
            return NULL;
        }
        res->argv[--res->argc] = NULL;
    }
    return res;
}

static struct ExpressionTree *
parse_pipe(struct SuperStorage *storage)
{
    skip_spaces(storage, 0);
    struct ExpressionTree *tree1 = parse_command(storage);
    if (tree1 == NULL) {
        return NULL;
    }
    unsigned long long prev_pos = storage->position;
    while (1) {
        enum Operation op = parse_op(storage);

        switch (op) {
        case OP_EOF:
            return tree1;
        case INV_OP:
            delete_expression_tree(tree1, storage);
            set_error_number(storage, INVALID_OPERATION);
            return NULL;
        case OP_PIPE:
            break;
        default:
            storage->position = prev_pos;
            return tree1;
        }

        struct ExpressionTree *tree2 = parse_command(storage);
        if (tree2 == NULL) {
            delete_expression_tree(tree1, storage);
            set_error_number(storage, NO_OPERAND);
            return NULL;
        }
        struct ExpressionTree *parent = calloc(1, sizeof *parent);
        if (parent == NULL) {
            set_error_number(storage, MEMORY_ERROR);
            delete_expression_tree(tree1, storage);
            delete_expression_tree(tree2, storage);
            return NULL;
        }

        parent->left = tree1;
        parent->right = tree2;
        parent->opcode = op;
        tree1 = parent;
        storage->parsing_tree = tree1;
        prev_pos = storage->position;
    }
}

static struct ExpressionTree *
parse_logicals(struct SuperStorage *storage)
{
    skip_spaces(storage, 0);
    struct ExpressionTree *tree1 = parse_pipe(storage);
    if (tree1 == NULL) {
        return NULL;
    }
    unsigned long long prev_pos = storage->position;
    while (1) {
        enum Operation op = parse_op(storage);

        switch (op) {
        case OP_EOF:
            return tree1;
        case INV_OP:
            delete_expression_tree(tree1, storage);
            set_error_number(storage, INVALID_OPERATION);
            return NULL;
        case OP_DISJ:
        case OP_CONJ:
            break;
        default:
            storage->position = prev_pos;
            return tree1;
        }

        struct ExpressionTree *tree2 = parse_pipe(storage);
        if (tree2 == NULL) {
            set_error_number(storage, NO_OPERAND);
            delete_expression_tree(tree1, storage);
            return NULL;
        }
        struct ExpressionTree *parent = calloc(1, sizeof *parent);
        if (parent == NULL) {
            set_error_number(storage, MEMORY_ERROR);
            delete_expression_tree(tree1, storage);
            delete_expression_tree(tree2, storage);
            return NULL;
        }

        parent->left = tree1;
        parent->right = tree2;
        parent->opcode = op;
        tree1 = parent;
        storage->parsing_tree = tree1;
        prev_pos = storage->position;
    }
}

static struct ExpressionTree *
parse_seps(struct SuperStorage *storage)
{
    unsigned long long prev_pos;
    skip_spaces(storage, 0);
    struct ExpressionTree *tree1 = parse_logicals(storage);
    if (tree1 == NULL) {
        return NULL;
    }
    prev_pos = storage->position;
    while (1) {
        enum Operation op = parse_op(storage);
        switch (op) {
        case INV_OP:
            delete_expression_tree(tree1, storage);
            set_error_number(storage, INVALID_OPERATION);
            return NULL;
        case OP_SEMI:
        case OP_PARA:
        case OP_ENDL:
            break;
        case OP_RBR:
            storage->position = prev_pos;
        default:
            return tree1;
        }

        if (op == OP_ENDL) {
            skip_spaces(storage, 1);
        }
        struct ExpressionTree *tree2 = parse_logicals(storage);
        if (tree2 == NULL && storage->container.err_happened) {
            delete_expression_tree(tree1, storage);
            return NULL;
        }
        struct ExpressionTree *parent = calloc(1, sizeof *parent);
        if (parent == NULL) {
            set_error_number(storage, MEMORY_ERROR);
            delete_expression_tree(tree1, storage);
            delete_expression_tree(tree2, storage);
            return NULL;
        }
        parent->left = tree1;
        parent->right = tree2;
        parent->opcode = op;
        tree1 = parent;
        storage->parsing_tree = parent;
        prev_pos = storage->position;
    }
}

struct SuperStorage
saver(struct SuperStorage *storage)
{
    static struct SuperStorage keep = {};
    if (storage != NULL) {
        keep = *storage;
    }
    return keep;
}

struct SuperStorage
syntax_analyse(const char *str)
{
    struct SuperStorage storage = {};
    storage.string = str;
    storage.position = 0;
    storage.container.err_happened = 0;
    storage.container.place = NULL;
    storage.parsing_tree = parse_seps(&storage);

    saver(&storage);

    if (storage.container.err_happened) {
        raise_error(storage.container.place, storage.container.code);
    }

    skip_spaces(&storage, 1);
    if (storage.string[storage.position] != '\0') {
        if (storage.string[storage.position] == ')') {
            raise_error(storage.string + storage.position, BRACKETS_BALANCE);
        } else {
            raise_error(storage.string + storage.position, INVALID_OPERATION);
        }
    }
    return storage;
}

void
delete_expression_tree(struct ExpressionTree *parse_tree, struct SuperStorage *storage)
{
    if (parse_tree != NULL) {
        delete_expression_tree(parse_tree->left, storage);
        delete_expression_tree(parse_tree->right, storage);

        if (parse_tree == storage->parsing_tree) {
            storage->parsing_tree = NULL;
        }

        for (long long i = 0; i < parse_tree->argc; ++i) {
            free(parse_tree->argv[i]);
        }
        free(parse_tree->redirect.append.file);
        free(parse_tree->redirect.out.file);
        free(parse_tree->redirect.in.file);

        free(parse_tree->argv);
        free(parse_tree);
    }
}