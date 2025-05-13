#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h> // for open()

#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/user.h>

#include <dwarf.h>
#include <libdwarf/libdwarf.h>


void read_cu_list(Dwarf_Debug dbg)
{  
  Dwarf_Unsigned cu_header_length = 0;
  Dwarf_Half version_stamp = 0;
  Dwarf_Unsigned abbrev_offset = 0;
  Dwarf_Half address_size = 0;
  Dwarf_Unsigned next_cu_header = 0;
  Dwarf_Error error;
  int cu_number = 0;

  for (;; ++cu_number) {
    Dwarf_Die no_die = 0;
    Dwarf_Die cu_die = 0;
    int res = DW_DLV_ERROR;
    res = dwarf_next_cu_header(dbg, &cu_header_length, &version_stamp,
        &abbrev_offset, &address_size, &next_cu_header, &error);

    if (res == DW_DLV_ERROR) {
      printf("Error in dwarf_next_cu_header\n");
      exit(1);
    }
    if (res == DW_DLV_NO_ENTRY) {
      printf("DONE - cu_number = %d\n", cu_number);
      return;
    }
  }
}

void run_dwarf(char* prog) {
  
  printf("Dwarf started\n");
  
  Dwarf_Debug dbg = 0;
  int fd = -1;
  const char *filepath = NULL;
  int res = DW_DLV_ERROR;
  Dwarf_Error error;
  Dwarf_Handler errhand = 0;
  Dwarf_Ptr errarg = 0;

  fd = open(prog, O_RDONLY);
  if (fd < 0) {
    printf("Cant open %s\n", prog);
  }

  res = dwarf_init(fd, DW_DLC_READ, errhand, errarg, &dbg, &error);
  if (res != DW_DLV_OK) {
    printf("Cant do dwarf stuff\n");
    exit(1);
  }

  read_cu_list(dbg);

  res = dwarf_finish(dbg, &error);
  if (res != DW_DLV_OK) {
    printf("dwarf_finish failed!\n");
  }
  close(fd);


}

void run_debugger(pid_t child_pid, uint64_t addr) {
  int wait_status;
  struct user_regs_struct regs;
  printf("debugger started\n");

  /* Wait for child to stop on its first instruction */
  wait(&wait_status);

  ptrace(PTRACE_GETREGS, child_pid, 0, &regs);
  printf("Child started. EIP = %p\n", regs.rip);

  long data = ptrace(PTRACE_PEEKTEXT, child_pid, (void*)addr, 0);
  printf("Original data at %p: %p\n", addr, data);

  long data_with_trap = (data & ~0xFF) | 0xCC;
  ptrace(PTRACE_POKETEXT, child_pid, (void*)addr, (void*)data_with_trap);

  long readback_data = ptrace(PTRACE_PEEKTEXT, child_pid, (void*)addr, 0);
  printf("After trap, data at %p: %p\n", addr, readback_data);
  
  ptrace(PTRACE_CONT, child_pid, 0, 0);
  
  wait(&wait_status);
  if (WIFSTOPPED(wait_status)) {
    printf("Child got a signal...\n");
  }

  ptrace(PTRACE_GETREGS, child_pid, 0, &regs);
  printf("Child stopped at EIP = %p\n", regs.rip);

  /* Remove the breakpoint by restoring the previous data
  ** at the target address, and unwind the EIP back by 1 to 
  ** let the CPU execute the original instruction that was 
  ** there.
  */
  ptrace(PTRACE_POKETEXT, child_pid, (void*) addr, (void*)data);
  regs.rip -= 1;
  ptrace(PTRACE_SETREGS, child_pid, 0, &regs);
  
  printf("EIP: %p, rax: %p, rbx: %p \n", regs.rip, regs.rax, regs.rbx);

  ptrace(PTRACE_CONT, child_pid, 0, 0);
  wait(&wait_status);

  if (WIFEXITED(wait_status)) {
    printf("Child exited\n");
  }
}


int main(int argc, char *argv[]) {

  Dwarf_Debug dbg = 0;

  if (argc < 3) {
    printf("Usage: %s <target> <address>\n", argv[0]);
    return 0;
  }

  char *prog = argv[1];

  run_dwarf (prog);

  pid_t pid = fork();
  if (pid == 0) {
    
    //we're in the child process
    //execute debugee
    ptrace(PTRACE_TRACEME, 0, NULL, NULL);
    execl(prog, prog, NULL);
  }
  else if (pid >= 1)  {
    uint64_t addr = (uint64_t) strtol(argv[2], NULL, 16);
    //run_dwarf (pid);
    run_debugger(pid, addr);
  }
}
