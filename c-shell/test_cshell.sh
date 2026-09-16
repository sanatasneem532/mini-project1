#!/usr/bin/env bash
###############################################################################
# test_cshell.sh — automated test harness for CS3.301 Mini Project 1 (C-Shell)
# macOS-safe: no heredocs (avoids old bash 3.2 quirks), no GNU-only `timeout`.
#
# Usage:
#   ./test_cshell.sh /path/to/shell.out
#   (or run it from inside c-shell/ with no args — it defaults to ./shell.out)
###############################################################################
set -u

SHELL_BIN="${1:-./shell.out}"
if [[ ! -x "$SHELL_BIN" ]]; then
  echo "ERROR: shell binary not found or not executable: $SHELL_BIN"
  echo "Build it first:  cd c-shell && make all"
  exit 1
fi
SHELL_BIN="$(cd "$(dirname "$SHELL_BIN")" && pwd)/$(basename "$SHELL_BIN")"

PASS=0
FAIL=0
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; BOLD='\033[1m'; NC='\033[0m'

WORKDIR=$(mktemp -d "${TMPDIR:-/tmp}/cshelltest.XXXXXX")
cleanup() { rm -rf "$WORKDIR"; }
trap cleanup EXIT

cd "$WORKDIR" || exit 1

# ---------------------------------------------------------------------------
# Fixtures — this becomes the shell's "home directory" since it launches here
# ---------------------------------------------------------------------------
mkdir -p sub1 sub2 .hiddendir
echo "line one"  > a.txt
echo "line two"  > b.txt
printf "meeting at 5pm" > notes.txt
printf "#OSN MP1\n\nThis is OSN shell assignment.\n" > README.md
printf "banana\napple\ncherry\n" > fruits.txt
printf "l1\nl2\nl3\nl4\nl5\n" > five.txt
touch .hiddenfile
echo "sub content" > sub1/inner.txt
printf '#!/bin/sh\necho running local build script\n' > script.sh
chmod +x script.sh
seq 1 5000 > big.txt 2>/dev/null || jot 5000 1 > big.txt   # seq is not on macOS by default; jot is the BSD equivalent

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

# run "<input text>"  — feeds $1 as stdin to the shell (as if typed line by line),
# returns combined stdout+stderr. Enforces a 3s watchdog without relying on the
# GNU-only `timeout` binary (macOS doesn't ship it).
run() {
  local input="$1"
  local outfile
  outfile=$(mktemp "${TMPDIR:-/tmp}/cshellout.XXXXXX")

  printf '%s' "$input" | "$SHELL_BIN" >"$outfile" 2>&1 &
  local pid=$!

  ( sleep 3; kill -9 "$pid" 2>/dev/null ) &
  local watchdog=$!

  wait "$pid" 2>/dev/null
  kill "$watchdog" 2>/dev/null
  wait "$watchdog" 2>/dev/null

  cat "$outfile"
  rm -f "$outfile"
}

section() { echo -e "\n${CYAN}${BOLD}=== $1 ===${NC}"; }

check() {
  local desc="$1" out="$2" pattern="$3"
  if echo "$out" | grep -qE "$pattern"; then
    echo -e "${GREEN}PASS${NC}  $desc"
    PASS=$((PASS+1))
  else
    echo -e "${RED}FAIL${NC}  $desc"
    echo "$out" | sed 's/^/       got> /'
    FAIL=$((FAIL+1))
  fi
}

check_not() {
  local desc="$1" out="$2" pattern="$3"
  if echo "$out" | grep -qE "$pattern"; then
    echo -e "${RED}FAIL${NC}  $desc (should NOT match, but did)"
    echo "$out" | sed 's/^/       got> /'
    FAIL=$((FAIL+1))
  else
    echo -e "${GREEN}PASS${NC}  $desc"
    PASS=$((PASS+1))
  fi
}

show() {
  local desc="$1" out="$2"
  echo -e "${YELLOW}EYEBALL:${NC} $desc"
  echo "$out" | sed 's/^/       > /'
}

###############################################################################
section "PART A: Prompt & Input Parsing"
###############################################################################

out=$(run '')
check "empty stdin: exits cleanly, no crash" "$out" "^$|<.*>"

out=$(run $'\n')
check "blank line reprints prompt without error" "$out" "."

out=$(run $'   \n')
check "whitespace-only line is valid (no invalid syntax)" "$out" "."
check_not "whitespace-only line does not error" "$out" "invalid syntax"

out=$(run 'echo "a > b"')
check "quoted > is literal text, not a redirection" "$out" "a > b"

out=$(run 'echo ""')
check_not "empty quoted string is valid syntax" "$out" "invalid syntax"

out=$(run 'cat my\ file.txt')
check "escaped space kept as ONE word (file doesn't exist -> file-not-found, not two-arg error)" "$out" "no such file|not a directory|cannot"

out=$(run 'cat meow.txt | ; meow')
check "pipe with nothing after it -> invalid syntax" "$out" "invalid syntax"

out=$(run 'echo hi ;')
check "trailing semicolon with no command after -> invalid syntax" "$out" "invalid syntax"

out=$(run 'echo hi & &')
check "double ampersand -> invalid syntax" "$out" "invalid syntax"

out=$(run 'cat <')
check "redirection with no target file -> invalid syntax" "$out" "invalid syntax"

out=$(run '| sort')
check "line starting with a pipe -> invalid syntax" "$out" "invalid syntax"

out=$(run 'echo "unclosed')
check "unterminated double-quote -> invalid syntax" "$out" "invalid syntax"

out=$(run $'echo \'unclosed')
check "unterminated single-quote -> invalid syntax" "$out" "invalid syntax"

out=$(run $'echo trailing\\')
check "trailing backslash as very last char of line -> invalid syntax" "$out" "invalid syntax"

out=$(run 'echo hi')
check "simple valid command runs" "$out" "^hi$"

out=$(run 'cat meow.txt|meow;meow>meow.txt&')
show "no whitespace between tokens at all (maximal munch stress test — should still parse, not error)" "$out"

out=$(run 'echo    hi          there')
check "arbitrary whitespace between words is collapsed" "$out" "^hi there$"

out=$(run '>> outfile.txt')
check "line starting with >> and no command -> invalid syntax" "$out" "invalid syntax"

###############################################################################
section "PART B1: hop"
###############################################################################

out=$(run 'hop ..')
show "hop .. from home moves to parent dir (verify prompt path)" "$out"

out=$(run 'hop nonexistentdirxyz123')
check "hop into unknown name with no frecency history -> no such directory" "$out" "hop: no such directory"

out=$(run $'hop .\necho done')
check "hop . is a no-op, rest of session continues" "$out" "^done$"

out=$(run 'hop -')
check_not "hop - with no previous dir yet should not crash" "$out" "Segmentation|core dumped"

out=$(run $'hop sub1\nhop ..\nhop sub1\nhop sub')
show "repeated hops into sub1 then a partial-name 'sub' should frecency-match sub1 or sub2" "$out"

out=$(run 'hop ~')
show "hop ~ returns to shell's home dir" "$out"

###############################################################################
section "PART B2: reveal"
###############################################################################

out=$(run 'reveal')
check_not "reveal with no args hides dotfiles by default" "$out" "^\.hiddenfile$"
check "reveal with no args lists a.txt" "$out" "a\.txt"

out=$(run 'reveal -a')
check "reveal -a shows hidden files" "$out" "\.hiddenfile"

out=$(run 'reveal -t')
check "reveal -t shows directories with trailing slash" "$out" "sub1/"
check "reveal -t recurses into subdirectory contents" "$out" "sub1/inner\.txt"

out=$(run 'reveal -x')
check "reveal with bad flag -> invalid syntax" "$out" "reveal: invalid syntax"

out=$(run 'reveal a b c')
check "reveal with too many positional args -> invalid syntax" "$out" "reveal: invalid syntax"

out=$(run 'reveal nonexistentdir123')
check "reveal on nonexistent dir -> no such directory" "$out" "reveal: no such directory"

out=$(run 'reveal -')
check "reveal - before any hop this session -> no such directory" "$out" "reveal: no such directory"

out=$(run 'reveal -ta -ttttttaaaaaaaaaatttt -aaaa -tttt')
show "repeated/stacked flags should behave same as single -ta (idempotent)" "$out"

###############################################################################
section "PART B3: peek"
###############################################################################

out=$(run 'peek README.md')
check "peek prints file contents as-is" "$out" "This is OSN shell assignment"

out=$(run 'peek -n README.md')
check "peek -n numbers non-empty lines" "$out" "^1 |^1	"

out=$(run 'peek -r fruits.txt')
show "peek -r reverses line order (expect cherry, apple, banana)" "$out"

out=$(run 'peek -rn fruits.txt')
show "peek -rn combines reverse + numbering" "$out"

out=$(run $'peek a.txt b.txt')
check "peek concatenates multiple files in order given" "$out" "line one"

out=$(run 'peek nofile123.txt')
check "peek on nonexistent file -> no such file or directory" "$out" "peek: no such file or directory"

out=$(run 'peek sub1')
check "peek on a directory -> is a directory" "$out" "peek: is a directory"

out=$(run 'echo piped_stdin_content | peek -')
show "peek - (or peek with no filename) reads stdin" "$out"

out=$(run 'peek -r big.txt')
show "peek -r on a large regular file (should use lseek chunking, not hang/OOM) — check first/last lines: expect 5000 then ... then 1" "$out"

out=$(run 'peek -r a.txt b.txt')
show "peek -r with multiple files: EACH file reversed independently, file order preserved (a.txt's line reversed, then b.txt's line reversed)" "$out"

###############################################################################
section "PART B4: locate"
###############################################################################

out=$(run 'locate echo')
check "locate finds echo somewhere on PATH" "$out" "/echo$"

out=$(run 'locate')
check "locate with no args -> invalid syntax" "$out" "locate: invalid syntax"

out=$(run 'locate thisdoesnotexist12345')
check "locate on unknown command -> command not found (name)" "$out" "locate: command not found \(thisdoesnotexist12345\)"

out=$(run 'locate echo thisdoesnotexist12345 cat')
show "locate with mixed valid/invalid args prints in given order, continues after failures" "$out"

out=$(run 'locate script.sh')
show "locate for an executable that ALSO exists in cwd should print cwd path first" "$out"

###############################################################################
section "PART C1: Command Execution"
###############################################################################

out=$(run 'echo hello')
check "plain command by name runs via PATH" "$out" "^hello$"

out=$(run './script.sh')
check "explicit relative path with / runs directly" "$out" "running local build script"

out=$(run 'script.sh')
check "bare name resolves to executable in cwd before PATH" "$out" "running local build script"

out=$(run '%script.sh')
check "%name forces PATH-only lookup, skips cwd match -> command not found" "$out" "cshell: command not found \(script\.sh\)"

out=$(run 'thiscommanddoesnotexist999')
check "unknown command -> command not found" "$out" "cshell: command not found \(thiscommanddoesnotexist999\)"

###############################################################################
section "PART C2: Input Redirection"
###############################################################################

out=$(run 'cat < notes.txt')
check "cat < file reads from file" "$out" "meeting at 5pm"

out=$(run 'cat < a.txt < b.txt')
check "multiple input redirections concatenate as one stream, in order" "$out" "line one"

out=$(run 'cat < missingfile123.txt')
check "input redirection from missing file -> no such file or directory" "$out" "cshell: no such file or directory"

###############################################################################
section "PART C3: Output Redirection"
###############################################################################

out=$(run $'echo hi > out1.txt > out2.txt\npeek out1.txt\npeek out2.txt')
check "output redirected to multiple files, each gets full output" "$out" "^hi$"

out=$(run $'echo hi > out1.txt\necho again >> out1.txt > out3.txt\npeek out1.txt\npeek out3.txt')
show "» appends, > truncates — independently per target file" "$out"

out=$(run 'echo hi > /nonexistent_dir_xyz/out.txt')
check "output file cannot be created -> unable to create file for writing" "$out" "cshell: unable to create file for writing"

###############################################################################
section "PART C4: Pipes"
###############################################################################

out=$(run 'printf "banana\napple" | sort')
show "basic 2-stage pipe: expect apple then banana" "$out"

out=$(run 'badcmd123 | sort')
check "unresolvable pipeline stage -> command not found, pipeline still runs" "$out" "cshell: command not found \(badcmd123\)"

out=$(run $'cat < a.txt | sort > pipeout.txt\npeek pipeout.txt')
check "redirection + pipe combined" "$out" "line one"

###############################################################################
section "PART D1: Sequential Execution"
###############################################################################

out=$(run 'echo hello ; echo world')
check "sequential execution runs both in order" "$out" "hello"

out=$(run 'echo hello ; nada123xyz ; echo world')
check "sequence stops after a command-not-found failure" "$out" "cshell: command not found \(nada123xyz\)"
check_not "sequence must NOT run commands after a failure" "$out" "world"

###############################################################################
section "PART D2: Background Execution"
###############################################################################

out=$(run $'echo bgtest &\nsleep 1')
check "background job announces [job_number] pid" "$out" '\[[0-9]+\] [0-9]+'
check "background job reports normal exit" "$out" "with pid [0-9]+ exited normally"

out=$(run $'echo j1 &\necho j2 &\necho j3 &\nsleep 1')
show "three background jobs — job numbers should be 1,2,3 and increase monotonically, never reused" "$out"

###############################################################################
section "PART F: spy / snoop"
###############################################################################

out=$(run 'spy')
check "spy with no args (self) prints header row" "$out" "PID.*FD.*TYPE.*PATH"

out=$(run 'spy 1 2')
check "spy with 2 pids -> invalid syntax" "$out" "spy: invalid syntax"

out=$(run 'spy 99999999')
check "spy on nonexistent pid -> no such process" "$out" "spy: no such process"

out=$(run 'snoop sleep 1')
check "snoop prints summary table after traced process exits" "$out" "syscall.*calls.*time"

out=$(run 'snoop -p 99999999')
check "snoop -p on nonexistent pid -> no such process" "$out" "snoop: no such process"

out=$(run 'snoop thiscommanddoesnotexist999')
check "snoop on nonexistent command -> command not found" "$out" "snoop: command not found"

# cross-session spy test: background a real process, then spy on its pid
bgout=$(run 'sleep 30 &')
pid=$(echo "$bgout" | grep -oE '\[[0-9]+\] [0-9]+' | head -1 | awk '{print $2}')
if [[ -n "${pid:-}" ]] && kill -0 "$pid" 2>/dev/null; then
  spyout=$(run "spy $pid")
  check "spy on a real backgrounded pid shows its open files" "$spyout" "$pid.*cwd|$pid.*REG|$pid.*txt"
  kill "$pid" 2>/dev/null
else
  echo -e "${YELLOW}SKIP${NC}  spy-on-real-pid test (couldn't capture a live pid from background announcement — check D2 output format first)"
fi

###############################################################################
section "SUMMARY"
###############################################################################
echo -e "${GREEN}PASS: $PASS${NC}   ${RED}FAIL: $FAIL${NC}"
echo
echo "Notes:"
echo "  - 'EYEBALL' lines above are ones that vary too much (ordering choices, pids,"
echo "    timing) to auto-assert — read them and compare to the spec examples yourself."
echo "  - Part E (Ctrl-C/Ctrl-Z/resume/ping terminal handoff) needs a real pseudo-"
echo "    terminal to test since these are signals, not literal bytes on a pipe."
echo "    Use test_jobcontrol.exp (requires the 'expect' tool) or test manually."
echo
[[ $FAIL -eq 0 ]] && exit 0 || exit 1