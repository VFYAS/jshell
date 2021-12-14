#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include "executor.h"
#include "error_handler.h"

static int
execute(ExpressionTree *tree, Utils *utils);

static void
check_redirection(ExpressionTree *tree);

static _Noreturn void
end_process(int status);

static void
check_redirection(ExpressionTree *tree)
{
    // isatty is used or not used if we want to redirect i/o at
    // the centre of pipeline to the file or to the next command.
    // As example, it defines the behavior of the program in situation
    // "ls | cat > out | wc".
    if (tree->redirect.need_redirect) {
        if (tree->redirect.out.exists /*&& isatty(1)*/ && tree->redirect.out.file) {
            int out = open(tree->redirect.out.file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (out < 0 || dup2(out, 1) < 0) {
                raise_error(NULL, SYSCALL_ERROR);
            }
            close(out);
        }
        if (tree->redirect.app.exists /*&& isatty(1)*/ && tree->redirect.app.file) {
            int out = open(tree->redirect.app.file, O_WRONLY | O_CREAT | O_APPEND, 0666);
            if (out < 0 || dup2(out, 1) < 0) {
                raise_error(NULL, SYSCALL_ERROR);
            }
            close(out);
        }
        if (tree->redirect.in.exists /*&& isatty(0)*/ && tree->redirect.in.file) {
            int in = open(tree->redirect.in.file, O_RDONLY);
            if (in < 0 || dup2(in, 0) < 0) {
                raise_error(NULL, SYSCALL_ERROR);
            }
            close(in);
        }
    }
}

int
start_execution(Utils *utils)
{
    prctl(PR_SET_CHILD_SUBREAPER);
    pid_t pid;
    if ((pid = fork()) < 0) {
        raise_error(NULL, SYSCALL_ERROR);
    } else if (pid == 0) {
        execute(utils->parsing_tree, utils);
    }
    pid_t wait_ret;
    int status;
    while ((wait_ret = wait(&status)) != pid && wait_ret > 0) {}
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

static int
execute(ExpressionTree *tree, Utils *utils)
{
    if (tree == NULL) {
        exit(0);
    }
    int reverse = 0, status, fd[2];
    pid_t pid1, pid2;
    switch (tree->opcode) {
    case OP_COM:
        if ((pid1 = fork()) < 0) {
            raise_error(NULL, SYSCALL_ERROR);
        } else if (pid1 == 0) {
            check_redirection(tree);
            execvp(tree->argv[0], tree->argv);
            perror(NULL);
            exit(EXEC_ERROR);
        }
        waitpid(pid1, &status, 0);
        end_process(status);
    case OP_EOF:
        exit(0);
    case OP_DISJ:
        // ||
        reverse = 1;
    case OP_CONJ:
        // &&
        if ((pid1 = fork()) < 0) {
            raise_error(NULL, SYSCALL_ERROR);
        } else if (pid1 == 0) {
            execute(tree->left, utils);
        }
        waitpid(pid1, &status, 0);
        if ((reverse == 0) == (WIFEXITED(status) && !WEXITSTATUS(status))) {
            if ((pid2 = fork()) < 0) {
                raise_error(NULL, SYSCALL_ERROR);
            } else if (pid2 == 0) {
                execute(tree->right, utils);
            }
        } else {
            end_process(status);
        }
        waitpid(pid2, &status, 0);
        end_process(status);
    case OP_SEMI:
        if ((pid1 = fork()) < 0) {
            raise_error(NULL, SYSCALL_ERROR);
        } else if (pid1 == 0) {
            execute(tree->left, utils);
        }
        waitpid(pid1, NULL, 0);
        if ((pid2 = fork()) < 0) {
            raise_error(NULL, SYSCALL_ERROR);
        } else if (pid2 == 0) {
            execute(tree->right, utils);
        }
        waitpid(pid2, &status, 0);
        end_process(status);
    case OP_PIPE:
        if (pipe(fd) < 0) {
            raise_error(NULL, SYSCALL_ERROR);
        }
        if ((pid1 = fork()) < 0) {
            raise_error(NULL, SYSCALL_ERROR);
        } else if (pid1 == 0) {
            if (dup2(fd[1], 1) < 0) {
                raise_error(NULL, SYSCALL_ERROR);
            }
            close(fd[1]);
            execute(tree->left, utils);
        }
        close(fd[1]);
        if ((pid2 = fork()) < 0) {
            raise_error(NULL, SYSCALL_ERROR);
        } else if (pid2 == 0) {
            if (dup2(fd[0], 0) < 0) {
                raise_error(NULL, SYSCALL_ERROR);
            }
            close(fd[0]);
            execute(tree->right, utils);
        }
        close(fd[0]);
        waitpid(pid2, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) != EXEC_ERROR) {
            waitpid(pid1, NULL, 0);
            end_process(status);
        } else {
            waitpid(pid1, &status, 0);
            end_process(status);
        }
    case OP_PARA:
        if ((pid1 = fork()) < 0) {
            raise_error(NULL, SYSCALL_ERROR);
        } else if (pid1 == 0) {
            execute(tree->left, utils);
        }
        if ((pid2 = fork()) < 0) {
            raise_error(NULL, SYSCALL_ERROR);
        } else if (pid2 == 0) {
            execute(tree->right, utils);
        }
        while (wait(NULL) > 0) {}
        exit(0);
    case OP_LBR:
        check_redirection(tree);
        if (tree->right->opcode != OP_RBR) {
            raise_error(utils->string, BRACKETS_BALANCE);
        }
        if ((pid1 = fork()) < 0) {
            raise_error(NULL, SYSCALL_ERROR);
        } else if (pid1 == 0) {
            execute(tree->left, utils);
        }
        waitpid(pid1, &status, 0);
        end_process(status);
    default:
        raise_error(NULL, INTERNAL_ERROR);
    }
}