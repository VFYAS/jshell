#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>

/*
 * Runs the main program (passed as argv[1])
 * with preset input (passed as argv[2]) and
 * compares the output with keys (passed as argv[3])
 * and the exit status with argv[4].
 */

enum
{
    ARGC_NEEDED = 5
};

enum
{
    BUF_SIZE = 1024
};

enum Errors
{
    NO_FILES_ERROR = 0x02,
    CHILD_ERROR = 0x03
};

int
cmp_files(int fd1, int fd2)
{
    char buf1[BUF_SIZE], buf2[BUF_SIZE];
    ssize_t syscall_ret1, syscall_ret2;
    do {
        if ((syscall_ret1 = read(fd1, buf1, sizeof(buf1))) < 0 || (syscall_ret2 = read(fd2, buf2, sizeof buf2)) < 0) {
            exit(1);
        }// works for the certain situation, won't work in general case
        for (int i = 0; i < syscall_ret1; ++i) {
            if (buf1[i] != buf2[i]) {
                return 0;
            }
        }
    } while (syscall_ret1 != 0 && syscall_ret2 != 0);
    if (syscall_ret1 == 0 && syscall_ret2 == 0) {
        return 1;
    }
    return 0;
}

int
main(int argc, char **argv)
{
    if (argc < ARGC_NEEDED) {
        fprintf(stderr, "Not enough arguments!\n");
        _exit(NO_FILES_ERROR);
    }
    pid_t tester;
    char template[] = "tmpXXXXXX";
    int test_answers = mkstemp(template);
    unlink(template);
    if ((tester = fork()) < 0) {
        fprintf(stderr, "Failed to fork: %s\n", strerror(errno));
        _exit(1);
    } else if (tester == 0) {
        int fd_in = open(argv[2], O_RDONLY);
        if (fd_in < 0 || test_answers < 0) {
            fprintf(stderr, "Failed to open %s file: %s\n", (fd_in < 0) ? "input" :
                                                            "output", strerror(errno));
            _exit(1);
        }
        dup2(fd_in, 0);
        dup2(test_answers, 1);
        dup2(test_answers, 2);
        close(fd_in);
        close(test_answers);
        execl(argv[1], argv[1], (char *) NULL);
        fprintf(stderr, "Failed to execl: %s\n", strerror(errno));
        _exit(errno);
    }
    int status;
    waitpid(tester, &status, 0);
    if (!WIFEXITED(status)) {
        fprintf(stderr, "Failed\n");
        _exit(CHILD_ERROR);
    }
    int test_keys = open(argv[3], O_RDONLY);
    if (test_keys < 0 || test_answers < 0) {
        fprintf(stderr, "Failed to open: %s\n", strerror(errno));
        _exit(errno);
    }
    lseek(test_answers, 0L, SEEK_SET);
    int cmp_ans = cmp_files(test_answers, test_keys);
    close(test_keys);
    close(test_answers);
    if (cmp_ans && WEXITSTATUS(status) == strtol(argv[4], NULL, 10)) {
        printf("OK!\n");
        return 0;
    } else {
        printf("Failed!\n");
        return 1;
    }
}
