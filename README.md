# Operating Systems & Networks: Mini Project 1

**Student**: `sanatasneem`  
**Course**: CS3.301 Operating Systems and Networks  

This repository contains the complete implementation of:
1. **C-Shell**: A feature-complete POSIX compliant C shell supporting custom intrinsics, lexer/grammar parsing, redirections, pipelines, background/sequential execution, job control, and process debugging (`spy` and `snoop`).
2. **xv6 MLFQ Scheduler**: An implementation of Multi-Level Feedback Queue (MLFQ) scheduling in the xv6 RISC-V operating system, alongside comparisons with FIFO and Round Robin.

---

## Repository Structure

```text
mini-project1/
├── c-shell/
│   ├── src/
│   ├── include/
│   └── Makefile
├── xv6/
│   ├── user/
│   ├── mkfs/
│   ├── kernel/
│   ├── Makefile
│   ├── report.md
│   ├── generate_plots.py
│   └── mlfq_timeline.png
├── AI-usage.pdf
└── README.md
```

---

## 1. C-Shell

### Compilation
Navigate to `c-shell/` and build:
```bash
cd c-shell
make all
```
This produces `shell.out`. Clean build artifacts with:
```bash
make clean
```

### Features Supported
- **Part A (Input)**: Prompt `<username@hostname:path>`, input reading, tokenization, and regular grammar validation.
- **Part B (Intrinsics)**: `hop` (with frecency fallback), `reveal` (`-a`, `-t`), `peek` (`-n`, `-r` with chunked `lseek`), `locate` (CWD then PATH lookup).
- **Part C (Redirections & Pipes)**: Multi-input (`<`) stream concatenation, multi-output (`>`, `>>`) tee branching, pipelines (`|`).
- **Part D (Execution)**: Sequential execution (`;`), background jobs (`&`) with non-blocking `SIGCHLD` reaping.
- **Part E (Job Control)**: `activities` (grouped by PGID), `tcsetpgrp` terminal control (Ctrl-C, Ctrl-Z, Ctrl-D), `resume` (`fg`, `bg`, `--timeout`), `ping`.
- **Part F (Exotic Builtins)**: `spy [pid]` (open file inspection via `/proc`), `snoop command [args...]` / `snoop -p pid` (syscall tracer via `ptrace`).

---

## 2. xv6 MLFQ Scheduler

### Compilation & Running
In `xv6/`, compile and run with QEMU selecting the scheduler:

- **Default (Round Robin)**:
  ```bash
  cd xv6
  make clean
  make qemu
  ```
- **MLFQ (Multi-Level Feedback Queue)**:
  ```bash
  cd xv6
  make clean
  make qemu SCHEDULER=MLFQ
  ```
- **FIFO (First-In First-Out)**:
  ```bash
  cd xv6
  make clean
  make qemu SCHEDULER=FIFO
  ```

### MLFQ Specifications
- **4 Priority Queues**: Queue 0 (highest) to Queue 3 (lowest).
- **Time Slices**: Queue 0 (1 tick), Queue 1 (4 ticks), Queue 2 (8 ticks), Queue 3 (16 ticks).
- **Strict Preemption**: Higher priority processes preempt lower priority tasks at tick boundaries.
- **Voluntary Yield**: Preserves queue priority.
- **Priority Boosting**: All processes move to Queue 0 every 48 timer ticks to prevent starvation.
- **procdump**: Press `Ctrl+P` to inspect live process queue levels, slice usage, and wait/run times.

### Benchmarking & Testing
Inside xv6, run:
```bash
schedulertest
```
This exercises CPU-bound, I/O-bound, and mixed processes, reporting turnaround, waiting, and response times.

### Timeline Plot Generation
Generate the queue trajectory scatter plot with watermark:
```bash
python3 generate_plots.py
```
Outputs `mlfq_timeline.png`.

Full analysis and discussion can be found in [xv6/report.md](xv6/report.md).
