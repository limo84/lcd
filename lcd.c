#include <curses.h>
#include <stdio.h>

#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>

#include <dwarf.h>
#include <libdwarf/libdwarf.h>

void run_debugger(pid_t child_pid) {
  int wait_status;
  unsigned icounter = 0;
  printf("debugger started\n");

  /* Wait for child to stop on its first instruction */
  wait(&wait_status);

  while (WIFSTOPPED(wait_status)) {
    icounter++;
    struct user_regs_struct regs;
    ptrace(PTRACE_GETREGS, child_pid, 0, &regs);
    unsigned instr = ptrace(PTRACE_PEEKTEXT, child_pid, regs.rip, 0);

    printf("icounter = %u, EIP? = 0x%08x, instr = 0x%08x, edx = %u\n", 
        icounter, regs.rip, instr, regs.rdx);

    /* Make the child execute another instruction */
    if (ptrace(PTRACE_SINGLESTEP, child_pid, 0, 0) < 0) {
      perror("ptrace");
        return;
    }
    /* Wait for child to stop on its next instruction */
    wait(&wait_status);
  }
  printf("the child executed %u instructions\n", icounter);
}


int main(int argc, char *argv[]) {

  Dwarf_Debug dbg = 0;

  if (argc < 2) {
    printf("Program name not specified\n");
    return -1;
  }

  char *prog = argv[1];
  printf("prog: %s\n", prog);

  pid_t pid = fork();
  if (pid == 0) {
    
    printf("child_pid: %d\n", pid); 
    
    //we're in the child process
    //execute debugee
    ptrace(PTRACE_TRACEME, 0, NULL, NULL);
    execl(prog, prog, NULL);
  }
  else if (pid >= 1)  {
    printf("lcd_pid: %d\n", pid);
    run_debugger(pid);
    //we're in the parent process
    //execute debugger
  }
}
