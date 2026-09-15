#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define NPROCS 5

void
cpu_bound(void)
{
  volatile double x = 0;
  for (int i = 0; i < 50000000; i++) {
    x += 0.001 * i;
  }
  (void)x;
}

void
io_bound(void)
{
  for (int i = 0; i < 20; i++) {
    volatile int x = 0;
    for (int j = 0; j < 50000; j++) {
      x += j;
    }
    (void)x;
    pause(2);
  }
}

void
mixed(void)
{
  for (int i = 0; i < 10; i++) {
    volatile int x = 0;
    for (int j = 0; j < 500000; j++) {
      x += j;
    }
    (void)x;
    pause(1);
  }
}

int
main(void)
{
  int pids[NPROCS];
  int types[NPROCS]; // 0: CPU, 1: IO, 2: Mixed
  int rtimes[NPROCS];
  int wtimes[NPROCS];

  printf("=== Starting schedulertest (%d processes) ===\n", NPROCS);

  for (int i = 0; i < NPROCS; i++) {
    int type = i % 3;
    types[i] = type;
    int pid = fork();
    if (pid < 0) {
      printf("fork failed\n");
      exit(1);
    }
    if (pid == 0) {
      if (type == 0) {
        cpu_bound();
      } else if (type == 1) {
        io_bound();
      } else {
        mixed();
      }
      exit(0);
    }
    pids[i] = pid;
  }

  int total_rtime = 0;
  int total_wtime = 0;
  int total_ttime = 0;

  printf("\nPID\tType\t\trtime\twtime\tTurnaround\n");
  printf("----------------------------------------------------\n");

  for (int i = 0; i < NPROCS; i++) {
    int rtime = 0, wtime = 0;
    int pid = waitx(&rtime, &wtime);
    rtimes[i] = rtime;
    wtimes[i] = wtime;
    int turnaround = rtime + wtime;

    total_rtime += rtime;
    total_wtime += wtime;
    total_ttime += turnaround;

    const char *typestr = (types[i] == 0) ? "CPU-Bound" : (types[i] == 1) ? "IO-Bound " : "Mixed    ";
    printf("%d\t%s\t%d\t%d\t%d\n", pid, typestr, rtime, wtime, turnaround);
  }

  printf("----------------------------------------------------\n");
  printf("Average Run Time:        %d ticks\n", total_rtime / NPROCS);
  printf("Average Waiting Time:    %d ticks\n", total_wtime / NPROCS);
  printf("Average Turnaround Time: %d ticks\n", total_ttime / NPROCS);
  printf("=== schedulertest complete ===\n");

  exit(0);
}
