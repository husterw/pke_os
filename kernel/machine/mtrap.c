#include "kernel/riscv.h"
#include "kernel/process.h"
#include "spike_interface/spike_utils.h"
#include "spike_interface/spike_file.h"
#include <string.h>

static void handle_instruction_access_fault() { panic("Instruction access fault!"); }

static void handle_load_access_fault() { panic("Load access fault!"); }

static void handle_store_access_fault() { panic("Store/AMO access fault!"); }

static void handle_illegal_instruction() { panic("Illegal instruction!"); }

static void handle_misaligned_load() { panic("Misaligned Load!"); }

static void handle_misaligned_store() { panic("Misaligned AMO!"); }

// added @lab1_3
static void handle_timer() {
  int cpuid = 0;
  // setup the timer fired at next time (TIMER_INTERVAL from now)
  *(uint64*)CLINT_MTIMECMP(cpuid) = *(uint64*)CLINT_MTIMECMP(cpuid) + TIMER_INTERVAL;

  // setup a soft interrupt in sip (S-mode Interrupt Pending) to be handled in S-mode
  write_csr(sip, SIP_SSIP);
}

void print_error(process* p) {
  uint64 epc = read_csr(mepc);
  int i = 0;
  while(1) {
    // find the error line through the epc
    if(p->line[i].addr == epc){
      uint64 line = p->line[i].line;
      char* file = p->file[p->line[i].file].file;
      char* dir = p->dir[p->file[p->line[i].file].dir];

      // get the code line
      // open file
      char full_path[256];
      strcpy(full_path, dir);
      strcpy(full_path + strlen(dir), "/");
      strcpy(full_path + strlen(dir) + 1, file);
      spike_file_t* f = spike_file_open(full_path, O_RDONLY, 0);

      // get file content
      char file_content[5192];
      struct stat f_stat;
      spike_file_stat(f, &f_stat);
      spike_file_read(f, file_content, f_stat.st_size);
      spike_file_close(f);

      // get the error line
      uint64 line_count = 1;
      for(int k = 0; k < f_stat.st_size; k++) {
        if(line_count == line) {
          int offset = 0;
          while(file_content[k + offset] != '\n') offset++;
          file_content[k + offset] = '\0';
          sprint("Runtime error at %s/%s: %d\n%s\n", dir, file, line,file_content + k);
          break;
        }
        if(file_content[k] == '\n') line_count++;
      }
      break;
    }
    i++;
  }
}

//
// handle_mtrap calls a handling function according to the type of a machine mode interrupt (trap).
//
void handle_mtrap() {
  uint64 mcause = read_csr(mcause);
  print_error(current);
  switch (mcause) {
    case CAUSE_MTIMER:
      handle_timer();
      break;
    case CAUSE_FETCH_ACCESS:
      handle_instruction_access_fault();
      break;
    case CAUSE_LOAD_ACCESS:
      handle_load_access_fault();
    case CAUSE_STORE_ACCESS:
      handle_store_access_fault();
      break;
    case CAUSE_ILLEGAL_INSTRUCTION:
      // TODO (lab1_2): call handle_illegal_instruction to implement illegal instruction
      // interception, and finish lab1_2.
      // panic( "call handle_illegal_instruction to accomplish illegal instruction interception for lab1_2.\n" );
      handle_illegal_instruction();

      break;
    case CAUSE_MISALIGNED_LOAD:
      handle_misaligned_load();
      break;
    case CAUSE_MISALIGNED_STORE:
      handle_misaligned_store();
      break;

    default:
      sprint("machine trap(): unexpected mscause %p\n", mcause);
      sprint("            mepc=%p mtval=%p\n", read_csr(mepc), read_csr(mtval));
      panic( "unexpected exception happened in M-mode.\n" );
      break;
  }
}
