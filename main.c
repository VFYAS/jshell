#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "syntax.h"
#include "executor.h"
#include "error_handler.h"

enum
{
    INIT_STR_SIZE = 256
};

void
delete_all(void)
{
    Utils utils = saver(NULL);
    delete_expression_tree(utils.parsing_tree, &utils);
    delete_expression_tree(utils.separate_tree, &utils);
    free((void *) utils.string);
}

int
main(void)
{
    atexit(delete_all);
    char *str = calloc(INIT_STR_SIZE, sizeof(*str));
    int c;
    long long idx = 0, str_len = INIT_STR_SIZE;
    c = fgetc(stdin);
    if (c == EOF || c == '\n') { // TODO: get rid of \n
        // if the expression is empty
        free(str);
        fprintf(stderr, "Empty statement!\n");
        exit(ERROR_EXIT);
    }
    while (c != EOF && c != '\n') {
        if (idx == str_len) {
            str_len <<= 1;
            str = realloc(str, str_len * sizeof(*str));
            if (str == NULL) {
                fprintf(stderr, "Memory allocation failed!\n");
                exit(MEMORY_ERROR);
            }
        }
        str[idx++] = (char) c;
        c = fgetc(stdin);
    }
    str[idx] = '\0';
    if (c == EOF) {
        fprintf(stderr, "Incorrect statement!\n");
        free(str);
        exit(ERROR_EXIT);
    }

    Utils utils = syntax_analyse(str);

    return start_execution(&utils);
}