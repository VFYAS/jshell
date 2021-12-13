#ifndef SHELL_ERROR_HANDLER_H
#define SHELL_ERROR_HANDLER_H

enum ErrorCode
{
    ERROR_EXIT = 0x01,
    BRACKETS_BALANCE = 0x01,
    NO_OPERAND = 0x02,
    NO_OPERATION = 0x03,
    INVALID_OPERATION = 0x04,
    INVALID_OPERAND = 0x05,
    INTERNAL_ERROR = 0x07,
    MEMORY_ERROR = 0x08,
    SYSCALL_ERROR = 0x09,
    EXEC_ERROR = 0x7F,
    SIGNAL_ADD = 0x80
};

typedef struct
{
    enum ErrorCode code;
    char *place;
    int err_happened;
} ErrorContainer;

_Noreturn void
raise_error(const char *parse_string, enum ErrorCode ErrorCode);
// prints the error message to stderr and terminates the process

#endif
