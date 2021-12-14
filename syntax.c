#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "syntax.h"
#include "error_handler.h"

static ExpressionTree *
parse_seps(const char *parse_string, unsigned long long *parse_pos, Utils *utils);

static ExpressionTree *
parse_logicals(const char *parse_string, unsigned long long *parse_pos, Utils *utils);

static ExpressionTree *
parse_pipe(const char *parse_string, unsigned long long *parse_pos, Utils *utils);

static ExpressionTree *
parse_command(const char *parse_string, unsigned long long *parse_pos, Utils *utils);

static enum Operation
parse_op(const char *parse_string, unsigned long long *parse_pos);
// defines which operation is next

static RedirectionHandle
parse_redirects(const char *parse_string, unsigned long long *parse_pos, Utils *utils);

static void
skip_spaces(const char *parse_string, unsigned long long *pos, int skip_endls);

static void
skip_spaces(const char *parse_string, unsigned long long *pos, int skip_endls)
{
    unsigned long long auto_pos = *pos;
    while (isspace(*(parse_string + auto_pos)) &&
           ((skip_endls == 0) == (*(parse_string + auto_pos) != '\n'))) {
        ++auto_pos;
    }
    *pos = auto_pos;
}

static enum Operation
parse_op(const char *parse_string, unsigned long long *parse_pos)
{
    skip_spaces(parse_string, parse_pos, 0);
    if (!parse_string[*parse_pos]) {
        return OP_EOF;
    }
    enum Operation opcode;
    switch (parse_string[*parse_pos]) {
    case '&':
        if (parse_string[*parse_pos + 1] == '&') {
            opcode = OP_CONJ;
            *parse_pos += 1;
        } else {
            opcode = OP_PARA;
        }
        break;
    case '|':
        if (parse_string[*parse_pos + 1] == '|') {
            opcode = OP_DISJ;
            *parse_pos += 1;
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
        if (parse_string[*parse_pos + 1] == '>') {
            opcode = OP_APP;
            *parse_pos += 1;
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
    *parse_pos += 1;
    return opcode;
}

static RedirectionHandle
parse_redirects(const char *parse_string, unsigned long long *parse_pos, Utils *utils)
{
    /*
     * If one command has several redirections of the same kind,
     * then we use only the last one.
     */
    unsigned long long prev_pos = *parse_pos;
    enum Operation next_op;
    RedirectionHandle redir = {};
    while ((next_op = parse_op(parse_string, parse_pos)) == OP_APP || next_op == OP_OUT || next_op == OP_INP) {
        struct redirector *curr_redirect;
        switch (next_op) {
        case OP_OUT:
            redir.out.exists = redir.need_redirect = 1;
            curr_redirect = &(redir.out);
            break;
        case OP_INP:
            redir.in.exists = redir.need_redirect = 1;
            curr_redirect = &(redir.in);
            break;
        case OP_APP:
            redir.app.exists = redir.need_redirect = 1;
            curr_redirect = &(redir.app);
            break;
        default:
            if (!utils->container.err_happened) {
                utils->container.err_happened = 1;
                utils->container.code = INTERNAL_ERROR;
                utils->container.place = (char *) (parse_string + *parse_pos);
            }
            return redir;
        }
        if (parse_op(parse_string, parse_pos) != INV_OP) {
            if (!utils->container.err_happened) {
                utils->container.err_happened = 1;
                utils->container.code = NO_OPERAND;
                utils->container.place = (char *) (parse_string + *parse_pos - 1);
            }
            return redir;
        }
        skip_spaces(parse_string, parse_pos, 0);

        const char *runner = parse_string + *parse_pos;
        unsigned long long pos = 0;
        while (parse_op(runner, &pos) == INV_OP && !isspace(*runner)) {
            pos = 0;
            ++runner;
        }
        curr_redirect->file = strndup(parse_string + *parse_pos, runner - parse_string - *parse_pos);
        if (curr_redirect->file == NULL) {
            if (!utils->container.err_happened) {
                utils->container.err_happened = 1;
                utils->container.code = MEMORY_ERROR;
                utils->container.place = (char *) (parse_string + *parse_pos);
            }
            return redir;
        }
        *parse_pos = runner - parse_string;
        prev_pos = *parse_pos;
    }
    *parse_pos = prev_pos;
    return redir;
}

static ExpressionTree *
parse_command(const char *parse_string, unsigned long long *parse_pos, Utils *utils)
{
    ExpressionTree *res;
    skip_spaces(parse_string, parse_pos, 0);
    if (parse_string[*parse_pos] == '(') {
        *parse_pos += 1;
        ExpressionTree *term_tree = parse_seps(parse_string, parse_pos, utils);
        skip_spaces(parse_string, parse_pos, 0);

        if (parse_string[*parse_pos] != ')') {
            delete_expression_tree(term_tree, utils);
            if (!utils->container.err_happened) {
                utils->container.err_happened = 1;
                utils->container.code = BRACKETS_BALANCE;
                utils->container.place = (char *) (parse_string + *parse_pos);
            }
            return NULL;
        }

        res = calloc(1, sizeof(*res));

        if (res == NULL) {
            delete_expression_tree(term_tree, utils);
            if (!utils->container.err_happened) {
                utils->container.err_happened = 1;
                utils->container.code = MEMORY_ERROR;
                utils->container.place = (char *) (parse_string + *parse_pos);
            }
            return NULL;
        }

        res->opcode = OP_LBR;
        res->left = term_tree;
        res->right = calloc(1, sizeof(*res->right));
        if (res->right == NULL) {
            delete_expression_tree(res, utils);
            if (!utils->container.err_happened) {
                utils->container.err_happened = 1;
                utils->container.code = MEMORY_ERROR;
                utils->container.place = (char *) (parse_string + *parse_pos);
            }
            return NULL;
        }

        res->right->opcode = OP_RBR;
        *parse_pos += 1;
    } else {
        skip_spaces(parse_string, parse_pos, 0);
        unsigned long long prev_pos = *parse_pos;
        if (parse_op(parse_string, parse_pos) != INV_OP) {
            *parse_pos = prev_pos;

            return NULL;
        }
        *parse_pos = prev_pos;
        res = calloc(1, sizeof *res);
        if (res == NULL) {
            if (!utils->container.err_happened) {
                utils->container.err_happened = 1;
                utils->container.code = MEMORY_ERROR;
                utils->container.place = (char *) (parse_string + *parse_pos);
            }
            return NULL;
        }
        res->argc = INIT_ARGC;
        res->argv = calloc(res->argc, sizeof(*res->argv));

        if (res->argv == NULL) {
            if (!utils->container.err_happened) {
                utils->container.err_happened = 1;
                utils->container.code = MEMORY_ERROR;
                utils->container.place = (char *) (parse_string + *parse_pos);
            }
            delete_expression_tree(res, utils);
            return NULL;
        }

        res->cur_argc = 0;
        res->opcode = OP_COM;
        while (parse_op(parse_string, parse_pos) == INV_OP) {
            if (res->cur_argc == res->argc) {
                res->argc <<= 1;
                res->argv = realloc(res->argv, res->argc * sizeof(*res->argv));
                if (res->argv == NULL) {
                    if (!utils->container.err_happened) {
                        utils->container.err_happened = 1;
                        utils->container.code = MEMORY_ERROR;
                        utils->container.place = (char *) (parse_string + *parse_pos);
                    }
                    delete_expression_tree(res, utils);
                    return NULL;
                }
            }
            skip_spaces(parse_string, parse_pos, 0);
            const char *runner = parse_string + *parse_pos;
            unsigned long long pos = 0;
            while (parse_op(runner, &pos) == INV_OP && !isspace(*runner)) {
                pos = 0;
                ++runner;
            }
            res->argv[res->cur_argc] = strndup(parse_string + *parse_pos, runner - parse_string - *parse_pos);
            *parse_pos = runner - parse_string;
            prev_pos = *parse_pos;
            res->cur_argc += 1;
        }
        *parse_pos = prev_pos;
        res->argc = res->cur_argc + 1;
        res->argv = realloc(res->argv, res->argc * sizeof(*res->argv));
        if (res->argv == NULL) {
            res->argc = 0;
            if (!utils->container.err_happened) {
                utils->container.err_happened = 1;
                utils->container.code = MEMORY_ERROR;
                utils->container.place = (char *) (parse_string + *parse_pos);
            }
            delete_expression_tree(res, utils);
            return NULL;
        }
        res->argv[--res->argc] = NULL;
    }
    res->redirect = parse_redirects(parse_string, parse_pos, utils);
    if (utils->container.err_happened) {
        delete_expression_tree(res, utils);
        return NULL;
    }
    return res;
}

static ExpressionTree *
parse_pipe(const char *parse_string, unsigned long long *parse_pos, Utils *utils)
{
    skip_spaces(parse_string, parse_pos, 0);
    ExpressionTree *tree1 = parse_command(parse_string, parse_pos, utils);
    if (tree1 == NULL) {
        return NULL;
    }
    unsigned long long prev_pos = *parse_pos;
    while (1) {
        enum Operation op = parse_op(parse_string, parse_pos);

        switch (op) {
        case OP_EOF:
            return tree1;
        case INV_OP:
            delete_expression_tree(tree1, utils);
            if (!utils->container.err_happened) {
                utils->container.err_happened = 1;
                utils->container.place = (char *) (parse_string + *parse_pos);
            }
            if (isalnum(parse_string[*parse_pos])) {
                utils->container.code = NO_OPERATION;
                return NULL;
            }
            utils->container.code = INVALID_OPERATION;
            return NULL;
        case OP_PIPE:
            break;
        default:
            *parse_pos = prev_pos;
            return tree1;
        }

        ExpressionTree *tree2 = parse_command(parse_string, parse_pos, utils);
        if (tree2 == NULL) {
            delete_expression_tree(tree1, utils);
            if (!utils->container.err_happened) {
                utils->container.err_happened = 1;
                utils->container.code = NO_OPERAND;
                utils->container.place = (char *) (parse_string + *parse_pos);
            }
            return NULL;
        }
        ExpressionTree *parent = calloc(1, sizeof *parent);
        if (parent == NULL) {
            if (!utils->container.err_happened) {
                utils->container.err_happened = 1;
                utils->container.code = MEMORY_ERROR;
                utils->container.place = (char *) (parse_string + *parse_pos);
            }
            delete_expression_tree(tree2, utils);
            delete_expression_tree(tree1, utils);
            return NULL;
        }

        parent->left = tree1;
        parent->right = tree2;
        parent->opcode = op;
        tree1 = parent;
        utils->parsing_tree = tree1;
        prev_pos = *parse_pos;
    }
}

static ExpressionTree *
parse_logicals(const char *parse_string, unsigned long long *parse_pos, Utils *utils)
{
    skip_spaces(parse_string, parse_pos, 0);
    ExpressionTree *tree1 = parse_pipe(parse_string, parse_pos, utils);
    if (tree1 == NULL) {
        return NULL;
    }
    unsigned long long prev_pos = *parse_pos;
    while (1) {
        enum Operation op = parse_op(parse_string, parse_pos);

        switch (op) {
        case OP_EOF:
            return tree1;
        case INV_OP:
            delete_expression_tree(tree1, utils);
            if (!utils->container.err_happened) {
                utils->container.err_happened = 1;
                utils->container.place = (char *) (parse_string + *parse_pos);
            }
            if (isalnum(parse_string[*parse_pos])) {
                utils->container.code = NO_OPERATION;
                return NULL;
            }
            utils->container.code = INVALID_OPERATION;
            return NULL;
        case OP_DISJ:
        case OP_CONJ:
            break;
        default:
            *parse_pos = prev_pos;
            return tree1;
        }

        ExpressionTree *tree2 = parse_pipe(parse_string, parse_pos, utils);
        if (tree2 == NULL) {
            if (!utils->container.err_happened) {
                utils->container.err_happened = 1;
                utils->container.code = NO_OPERAND;
                utils->container.place = (char *) (parse_string + *parse_pos);
            }
            delete_expression_tree(tree1, utils);
            return NULL;
        }
        ExpressionTree *parent = calloc(1, sizeof *parent);
        if (parent == NULL) {
            if (!utils->container.err_happened) {
                utils->container.err_happened = 1;
                utils->container.code = MEMORY_ERROR;
                utils->container.place = (char *) (parse_string + *parse_pos);
            }
            delete_expression_tree(tree1, utils);
            delete_expression_tree(tree2, utils);
            return NULL;
        }

        parent->left = tree1;
        parent->right = tree2;
        parent->opcode = op;
        tree1 = parent;
        utils->parsing_tree = tree1;
        prev_pos = *parse_pos;
    }
}

static ExpressionTree *
parse_seps(const char *parse_string, unsigned long long *parse_pos, Utils *utils)
{
    unsigned long long prev_pos;
    skip_spaces(parse_string, parse_pos, 0);
    ExpressionTree *tree1 = parse_logicals(parse_string, parse_pos, utils);
    if (tree1 == NULL) {
        return NULL;
    }
    prev_pos = *parse_pos;
    while (1) {
        enum Operation op = parse_op(parse_string, parse_pos);
        switch (op) {
        case INV_OP:
            delete_expression_tree(tree1, utils);
            if (!utils->container.err_happened) {
                utils->container.err_happened = 1;
                utils->container.place = (char *) (parse_string + *parse_pos);
            }
            if (isalnum(parse_string[*parse_pos])) {
                utils->container.code = NO_OPERATION;
                return NULL;
            }
            utils->container.code = INVALID_OPERATION;
            return NULL;
        case OP_SEMI:
        case OP_PARA:
        case OP_ENDL:
            break;
        case OP_RBR:
            *parse_pos = prev_pos;
        default:
            return tree1;
        }

        if (op == OP_ENDL) {
            skip_spaces(parse_string, parse_pos, 1);
        }
        ExpressionTree *tree2 = parse_logicals(parse_string, parse_pos, utils);
        if (tree2 == NULL && utils->container.err_happened) {
            delete_expression_tree(tree1, utils);
            return NULL;
        }
        ExpressionTree *parent = calloc(1, sizeof *parent);
        if (parent == NULL) {
            if (!utils->container.err_happened) {
                utils->container.err_happened = 1;
                utils->container.code = MEMORY_ERROR;
                utils->container.place = (char *) (parse_string + *parse_pos);
            }
            delete_expression_tree(tree1, utils);
            delete_expression_tree(tree2, utils);
            return NULL;
        }
        parent->left = tree1;
        parent->right = tree2;
        parent->opcode = op;
        tree1 = parent;
        utils->parsing_tree = parent;
        prev_pos = *parse_pos;
    }
}

Utils
saver(Utils *utils)
{
    static Utils keep;
    if (utils != NULL) {
        keep = *utils;
    }
    return keep;
}

Utils
syntax_analyse(const char *str)
{
    Utils utils = {};
    unsigned long long pos = 0;
    utils.string = str;
    utils.position = 0;
    utils.container.err_happened = 0;
    utils.container.place = NULL;
    utils.parsing_tree = parse_seps(str, &pos, &utils);

    saver(&utils);

    if (utils.container.err_happened) {
        raise_error(utils.container.place, utils.container.code);
    }

    skip_spaces(str, &pos, 1);
    if (str[pos] != '\0') {
        if (str[pos] == ')') {
            raise_error(str + pos, BRACKETS_BALANCE);
        } else {
            raise_error(str + pos, INVALID_OPERATION);
        }
    }

    return utils;
}

void
delete_expression_tree(ExpressionTree *parse_tree, Utils *utils)
{
    if (parse_tree != NULL) {
        delete_expression_tree(parse_tree->left, utils);
        delete_expression_tree(parse_tree->right, utils);

        if (parse_tree == utils->parsing_tree) {
            utils->parsing_tree = NULL;
        }

        for (long long i = 0; i < parse_tree->argc; ++i) {
            free(parse_tree->argv[i]);
        }
        free(parse_tree->redirect.app.file);
        free(parse_tree->redirect.out.file);
        free(parse_tree->redirect.in.file);

        free(parse_tree->argv);
        free(parse_tree);
    }
}