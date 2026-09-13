#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>

#include "snoop.h"
#include "executor.h"

#ifdef __linux__
#include <sys/ptrace.h>
#include <sys/user.h>
#if defined(__aarch64__)
#include <sys/uio.h>
#include <elf.h>
#endif
#else
#define PTRACE_TRACEME 0
#define PTRACE_ATTACH 16
#define PTRACE_SYSCALL 24
int ptrace(int req, pid_t pid, void *addr, void *data);
int ptrace(int req, pid_t pid, void *addr, void *data) {
    (void)req; (void)pid; (void)addr; (void)data;
    return -1;
}
#endif

struct sc_map {
    long n;
    const char *s;
};

#if defined(__aarch64__)
static const struct sc_map smap[] = {
    {56, "openat"}, {57, "close"}, {63, "read"}, {64, "write"},
    {78, "readlinkat"}, {79, "newfstatat"}, {80, "fstat"}, {93, "exit"},
    {94, "exit_group"}, {96, "set_tid_address"}, {98, "futex"}, {101, "nanosleep"},
    {113, "clock_gettime"}, {115, "clock_nanosleep"}, {134, "rt_sigaction"},
    {135, "rt_sigprocmask"}, {160, "uname"}, {172, "getpid"}, {178, "gettid"},
    {214, "brk"}, {215, "munmap"}, {216, "mremap"}, {220, "clone"},
    {221, "execve"}, {222, "mmap"}, {226, "mprotect"}, {260, "wait4"},
    {-1, NULL}
};
#else
static const struct sc_map smap[] = {
    {0, "read"}, {1, "write"}, {2, "open"}, {3, "close"}, {4, "stat"},
    {5, "fstat"}, {6, "lstat"}, {7, "poll"}, {8, "lseek"}, {9, "mmap"},
    {10, "mprotect"}, {11, "munmap"}, {12, "brk"}, {13, "rt_sigaction"},
    {14, "rt_sigprocmask"}, {15, "rt_sigreturn"}, {16, "ioctl"}, {17, "pread64"},
    {18, "pwrite64"}, {19, "readv"}, {20, "writev"}, {21, "access"},
    {22, "pipe"}, {23, "select"}, {24, "sched_yield"}, {25, "mremap"},
    {26, "msync"}, {27, "mincore"}, {28, "madvise"}, {29, "shmget"},
    {30, "shmat"}, {31, "shmctl"}, {32, "dup"}, {33, "dup2"},
    {34, "pause"}, {35, "nanosleep"}, {36, "getitimer"}, {37, "alarm"},
    {38, "setitimer"}, {39, "getpid"}, {40, "sendfile"}, {41, "socket"},
    {42, "connect"}, {43, "accept"}, {44, "sendto"}, {45, "recvfrom"},
    {46, "sendmsg"}, {47, "recvmsg"}, {48, "shutdown"}, {49, "bind"},
    {50, "listen"}, {51, "getsockname"}, {52, "getpeername"}, {53, "socketpair"},
    {54, "setsockopt"}, {55, "getsockopt"}, {56, "clone"}, {57, "fork"},
    {58, "vfork"}, {59, "execve"}, {60, "exit"}, {61, "wait4"},
    {62, "kill"}, {63, "uname"}, {72, "fcntl"}, {73, "flock"},
    {74, "fsync"}, {75, "fdatasync"}, {76, "truncate"}, {77, "ftruncate"},
    {78, "getdents"}, {79, "getcwd"}, {80, "chdir"}, {81, "fchdir"},
    {82, "rename"}, {83, "mkdir"}, {84, "rmdir"}, {85, "creat"},
    {86, "link"}, {87, "unlink"}, {88, "symlink"}, {89, "readlink"},
    {90, "chmod"}, {91, "fchmod"}, {92, "chown"}, {93, "fchown"},
    {94, "lchown"}, {95, "umask"}, {96, "gettimeofday"}, {97, "getrlimit"},
    {98, "getrusage"}, {99, "sysinfo"}, {100, "times"}, {102, "getuid"},
    {104, "getgid"}, {105, "setuid"}, {106, "setgid"}, {107, "geteuid"},
    {108, "getegid"}, {109, "setpgid"}, {110, "getppid"}, {111, "getpgrp"},
    {112, "setsid"}, {137, "statfs"}, {138, "fstatfs"}, {157, "prctl"},
    {158, "arch_prctl"}, {186, "gettid"}, {201, "time"}, {202, "futex"},
    {204, "sched_getaffinity"}, {217, "getdents64"}, {218, "set_tid_address"},
    {228, "clock_gettime"}, {230, "clock_nanosleep"}, {231, "exit_group"},
    {232, "epoll_wait"}, {233, "epoll_ctl"}, {257, "openat"}, {258, "mkdirat"},
    {262, "newfstatat"}, {263, "unlinkat"}, {267, "readlinkat"}, {268, "fchmodat"},
    {269, "faccessat"}, {293, "pipe2"}, {302, "prlimit64"}, {318, "getrandom"},
    {332, "statx"}, {-1, NULL}
};
#endif

static const char *get_name(long n) {
    for (int i = 0; smap[i].n != -1; i++) {
        if (smap[i].n == n) return smap[i].s;
    }
    return NULL;
}

static long get_sc(pid_t p) {
#if defined(__linux__) && defined(__x86_64__)
    struct user_regs_struct r;
    if (ptrace(PTRACE_GETREGS, p, 0, &r) < 0) return -1;
    return (long)r.orig_rax;
#elif defined(__linux__) && defined(__aarch64__)
    struct user_pt_regs r;
    struct iovec v = { .iov_base = &r, .iov_len = sizeof(r) };
    if (ptrace(PTRACE_GETREGSET, p, 1, &v) < 0) return -1;
    return (long)r.regs[8];
#else
    (void)p;
    return -1;
#endif
}

struct sc_rec {
    long sc;
    int cnt;
    double tm;
    int ord;
};

static int is_num(const char *s) {
    if (!s || !*s) return 0;
    for (const char *x = s; *x; x++) {
        if (!isdigit((unsigned char)*x)) return 0;
    }
    return 1;
}

void cmd_snoop(int argc, char **argv) {
    if (argc == 0) {
        printf("snoop: invalid syntax\n");
        return;
    }

    pid_t p;
    int att = 0;

    if (strcmp(argv[0], "-p") == 0) {
        if (argc != 2) {
            printf("snoop: invalid syntax\n");
            return;
        }
        if (!is_num(argv[1])) {
            printf("snoop: no such process\n");
            return;
        }
        p = (pid_t)atoi(argv[1]);
        if (kill(p, 0) == -1 && errno == ESRCH) {
            printf("snoop: no such process\n");
            return;
        }
        if (ptrace(PTRACE_ATTACH, p, 0, 0) < 0) {
            printf("snoop: no such process\n");
            return;
        }
        att = 1;
    } else {
        char *cmd = resolve_command_path(argv[0]);
        if (!cmd) {
            printf("snoop: command not found\n");
            return;
        }
        p = fork();
        if (p < 0) {
            free(cmd);
            return;
        }
        if (p == 0) {
            ptrace(PTRACE_TRACEME, 0, 0, 0);
            execv(cmd, argv);
            _exit(1);
        }
        free(cmd);
    }

    int st;
    waitpid(p, &st, 0);

    struct sc_rec recs[1024];
    int nrecs = 0;

    int in_sys = 0;
    long cur_sc = -1;
    struct timespec t1, t2;

    while (1) {
        if (ptrace(PTRACE_SYSCALL, p, 0, 0) < 0) break;
        if (waitpid(p, &st, 0) < 0) break;
        if (WIFEXITED(st) || WIFSIGNALED(st)) break;

        if (WIFSTOPPED(st)) {
            long sc = get_sc(p);
            if (!in_sys) {
                in_sys = 1;
                cur_sc = sc;
                clock_gettime(CLOCK_MONOTONIC, &t1);
            } else {
                in_sys = 0;
                clock_gettime(CLOCK_MONOTONIC, &t2);
                double dt = (double)(t2.tv_sec - t1.tv_sec) + (double)(t2.tv_nsec - t1.tv_nsec) / 1e9;
                if (dt < 0) dt = 0;

                int fnd = 0;
                for (int i = 0; i < nrecs; i++) {
                    if (recs[i].sc == cur_sc) {
                        recs[i].cnt++;
                        recs[i].tm += dt;
                        fnd = 1;
                        break;
                    }
                }
                if (!fnd && nrecs < 1024) {
                    recs[nrecs].sc = cur_sc;
                    recs[nrecs].cnt = 1;
                    recs[nrecs].tm = dt;
                    recs[nrecs].ord = nrecs;
                    nrecs++;
                }
            }
        }
    }

    (void)att;

    for (int i = 0; i < nrecs; i++) {
        for (int j = i + 1; j < nrecs; j++) {
            if (recs[i].cnt < recs[j].cnt || (recs[i].cnt == recs[j].cnt && recs[i].ord > recs[j].ord)) {
                struct sc_rec tmp = recs[i];
                recs[i] = recs[j];
                recs[j] = tmp;
            }
        }
    }

    printf("syscall calls time\n");
    for (int i = 0; i < nrecs; i++) {
        const char *s = get_name(recs[i].sc);
        char tmp[64];
        if (!s) {
            snprintf(tmp, sizeof(tmp), "syscall_%ld", recs[i].sc);
            s = tmp;
        }
        printf("%s %d %.3fs\n", s, recs[i].cnt, recs[i].tm);
    }
}
