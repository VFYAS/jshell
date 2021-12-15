#include "error_handler.h"
#include "syntax.h"
#include <stdio.h>
#include <stdlib.h>

void
set_error_number(struct SuperStorage *storage, enum ErrorCode code)
{
    if (!storage->container.err_happened) {
        storage->container.err_happened = 1;
        storage->container.code = code;
        storage->container.place = (char *) (storage->string + storage->position);
    }
}

_Noreturn void
raise_error(const char *error_string, enum ErrorCode ErrorCode)
{
    if (ErrorCode != SYSCALL_ERROR && ErrorCode != MEMORY_ERROR && ErrorCode != INTERNAL_ERROR) {
        fprintf(stderr, "Error while parsing: ");
    }
    switch (ErrorCode) {
    case BRACKETS_BALANCE:
        fprintf(stderr, "The balance of brackets is broken at: %s\n", error_string);
        break;
    case NO_OPERAND:
        fprintf(stderr, "No operand spotted at: %s\n", error_string);
        break;
    case NO_OPERATION:
        fprintf(stderr, "No operation between operands at: %s\n", error_string);
        break;
    case INVALID_OPERATION:
        fprintf(stderr, "Invalid operation %c\n", *error_string);
        break;
    case INVALID_OPERAND:
        fprintf(stderr, "Invalid operand at: %s\n", error_string);
        break;
    case MEMORY_ERROR:
        fprintf(stderr, "Out of memory\n");
        break;
    case SYSCALL_ERROR:
        perror(NULL);
        break;
    default:
        break;
    }
    exit(ERROR_EXIT);
}