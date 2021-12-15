#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include "executor.h"
#include "error_handler.h"
#include "syntax.h"

static void
execute(struct ExpressionTree *tree, struct SuperStorage *storage);

static void
check_redirection(struct ExpressionTree *tree);

static _Noreturn void
end_process(int status);

static void
check_redirection(struct ExpressionTree *tree)
{
    /*
     * 1) isatty is used to prevent the redirection i/o from
     * the centre of pipeline to the file.
     * For example, it defines the behavior of the program in situation
     * "ls | cat > out | wc". If the isatty is used, the redirection in
     * the said example will be ignored.
     * 2) If one command is used with ">" and ">>", then only the ">>"
     * file is redirected, the ">" file is just truncated.
     */
    if (tree->redirect.need_redirect) {
        if (tree->redirect.out.exists && isatty(1) && tree->redirect.out.file) {
            int out = open(tree->redirect.out.file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (out < 0 || dup2(out, 1) < 0) {
                raise_error(NULL, SYSCALL_ERROR);
            }
            close(out);
        }
        if (tree->redirect.append.exists && isatty(1) && tree->redirect.append.file) {
            int out = open(tree->redirect.append.file, O_WRONLY | O_CREAT | O_APPEND, 0666);
            if (out < 0 || dup2(out, 1) < 0) {
                raise_error(NULL, SYSCALL_ERROR);
            }
            close(out);
        }
        if (tree->redirect.in.exists && isatty(0) && tree->redirect.in.file) {
            int in = open(tree->redirect.in.file, O_RDONLY);
            if (in < 0 || dup2(in, 0) < 0) {
                raise_error(NULL, SYSCALL_ERROR);
            }
            close(in);
        }
    }
}

int
start_execution(struct SuperStorage *storage)
{
    prctl(PR_SET_CHILD_SUBREAPER); // just for safety reasons
    pid_t pid;
    if ((pid = fork()) < 0) {
        raise_error(NULL, SYSCALL_ERROR);
    } else if (pid == 0) {
        execute(storage->parsing_tree, storage);
    }
    int status;
    if (waitpid(pid, &status, 0) <= 0) {
        raise_error(NULL, INTERNAL_ERROR);
    }
    while (wait(NULL) > 0) {}
    if (WIFEXITED(status)) {
        return (WEXITSTATUS(status));
    } else {
        return (SIGNAL_ADD + WTERMSIG(status));
    }
}

static _Noreturn void
end_process(int status)
{
    if (WIFEXITED(status)) {
        exit(WEXITSTATUS(status));
    } else {
        exit(SIGNAL_ADD + WTERMSIG(status));
    }
}

static void
execute(struct ExpressionTree *tree, struct SuperStorage *storage)
{
    if (tree == NULL) {
        exit(0);
    }
    int reverse = 0, status, fd[2];
    pid_t pid1, pid2;
    switch (tree->opcode) {
    case OP_COM:
        check_redirection(tree);
        execvp(tree->argv[0], tree->argv);
        perror(tree->argv[0]);
        exit(EXEC_ERROR);
    case OP_EOF:
        exit(0);
    case OP_DISJ:
        reverse = 1;
    case OP_CONJ:
        if ((pid1 = fork()) < 0) {
            raise_error(NULL, SYSCALL_ERROR);
        } else if (pid1 == 0) {
            execute(tree->left, storage);
        }
        waitpid(pid1, &status, 0);
        if ((reverse == 0) == (WIFEXITED(status) && !WEXITSTATUS(status))) {
            if ((pid2 = fork()) < 0) {
                raise_error(NULL, SYSCALL_ERROR);
            } else if (pid2 == 0) {
                execute(tree->right, storage);
            }
        } else {
            end_process(status);
        }
        waitpid(pid2, &status, 0);
        end_process(status);
    case OP_SEMI:
    case OP_ENDL:
        if ((pid1 = fork()) < 0) {
            raise_error(NULL, SYSCALL_ERROR);
        } else if (pid1 == 0) {
            execute(tree->left, storage);
        }
        waitpid(pid1, &status, 0);
        if (tree->right == NULL) {
            end_process(status);
        }
        if ((pid2 = fork()) < 0) {
            raise_error(NULL, SYSCALL_ERROR);
        } else if (pid2 == 0) {
            execute(tree->right, storage);
        }
        waitpid(pid2, &status, 0);
        end_process(status);
    case OP_PIPE:
        if (pipe(fd) < 0 || (pid1 = fork()) < 0) {
            raise_error(NULL, SYSCALL_ERROR);
        } else if (pid1 == 0) {
            if (dup2(fd[1], 1) < 0) {
                raise_error(NULL, SYSCALL_ERROR);
            }
            close(fd[1]);
            execute(tree->left, storage);
        }
        close(fd[1]);
        if ((pid2 = fork()) < 0) {
            raise_error(NULL, SYSCALL_ERROR);
        } else if (pid2 == 0) {
            if (dup2(fd[0], 0) < 0) {
                raise_error(NULL, SYSCALL_ERROR);
            }
            close(fd[0]);
            execute(tree->right, storage);
        }
        close(fd[0]);
        pid_t wait_ret;
        while ((wait_ret = wait(&status)) != pid2 && wait_ret > 0) {}
        while (wait(NULL) > 0) {}
        end_process(status);
    case OP_PARA:
        if ((pid1 = fork()) < 0) {
            raise_error(NULL, SYSCALL_ERROR);
        } else if (pid1 == 0) {
            execute(tree->left, storage);
        }
        if ((pid2 = fork()) < 0) {
            raise_error(NULL, SYSCALL_ERROR);
        } else if (pid2 == 0) {
            execute(tree->right, storage);
        }
        while (wait(NULL) > 0) {}
        exit(0);
    case OP_LBR:
        check_redirection(tree);
        if (tree->right->opcode != OP_RBR) {
            raise_error(storage->string, BRACKETS_BALANCE);
        }
        if ((pid1 = fork()) < 0) {
            raise_error(NULL, SYSCALL_ERROR);
        } else if (pid1 == 0) {
            execute(tree->left, storage);
        }
        waitpid(pid1, &status, 0);
        end_process(status);
    default:
        raise_error(NULL, INTERNAL_ERROR);
    }
}