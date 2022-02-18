#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "nazlib.h"

static void usage(const char* self) {
    fprintf(stderr, "Usage: %s [-u] <file>\n File needs to be the .naz file to execute.\n Use -u to enable unlimited numbers.\n All other flags are currently not supported\n", self);
    exit(EXIT_FAILURE);
}

static struct callstack* cs;

static void vis_ip(struct instruction_pointer* arg) {
    if (instruction_pointer_is_in_function(arg)) {
        printf("%d:%d\n", instruction_pointer_function_number(arg), instruction_pointer_offset(arg));
    } else {
        printf("Toplevel:%d\n", instruction_pointer_offset(arg));
    }
}

void debug() {
    for(int i=0; i < 10; ++i){
        printf("Function %d: %s\n", i, function_get(i));
    }
    for(int i=0; i< 10; ++i){
        printf("Var %d:", i);
        struct number* tmp = variable_get(i);
        number_print_dbg(tmp);
        number_destroy(tmp);
        printf("\n");
    }

    printf("Acc: ");
    struct number* tmp = accumulator_get();
    number_print_dbg(tmp);
    number_destroy(tmp);
    printf("\nCallstack:\n");
    callstack_iterate(cs,vis_ip);
}

_Noreturn void die(const char msg[]) {
    perror(msg);
    debug();
    exit(EXIT_FAILURE);
}


static char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f)
        die(path);

    if (fseek(f, 0, SEEK_END) == -1)
        die("fseek");

    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(size + 1);
    fread(buf, 1, size, f);

    fclose(f);
    buf[size] = '\0';
    return buf;
}

static void work_tupel(char* pos) {
}

static char* program_code;

static void opcodes(const char* pos) {
    int extra_offset;
    switch(*pos) {
        case '0': {
                      extra_offset = 2;
                      goto offset_manipulation;
                  }
        case '1': {
                      if (pos[3] != 'f') {
                          die("Using 1x without following it with Xf");
                      }
                      int idx = pos[2] - '0';

                      if (idx < 0 || idx > 9) {
                          die("invalid number of 1xXf");
                      }
                      function_set(idx, pos + 4);
                      for(extra_offset = 4; pos[extra_offset] != '\n' && pos[extra_offset] != '\0'; extra_offset += 2) {
                          if (pos[extra_offset] == ' ') {
                              extra_offset--;
                              continue;
                          }
                          if (pos[extra_offset] == '0' && pos[extra_offset + 1] == 'x') {
                              extra_offset += 2;
                              break;
                          }
                      }
                      goto offset_manipulation;
                  }
        case '2': {
                      if (pos[3] != 'v') {
                          die("Using 2x without following it with Xv");
                      }
                      int idx = pos[2] - '0';
                      if (idx < 0 || idx > 9) {
                          die("invalid number of 2xXv");
                      }
                      variable_set(idx, accumulator_get());
                      extra_offset = 4;
                      goto offset_manipulation;
                  }
        case '3': {
                      if (pos[3] != 'v') {
                          die("Using 3x without following it with Xv");
                      }
                      if (pos[5] != 'l' && pos[5] != 'e' && pos[5] != 'g') {
                          die("Using 3x without following it with Xl, Xg or Xe after Yv");
                      }
                      int var = pos[2] - '0';
                      int fun = pos[4] - '0';

                      struct number* acc = accumulator_get();
                      struct number* var_n = variable_get(var);

                      int cmp = number_compare(acc, var_n);

                      int jmp = 0;
                      if (cmp == 0 && pos[5] == 'e') {
                          jmp = 1;
                      } else if (cmp < 0 && pos[5] == 'l') {
                          jmp = 1;
                      } else if (cmp > 0 && pos[5] == 'g') {
                          jmp = 1;
                      }

                      number_destroy(acc);
                      number_destroy(var_n);

                      if (jmp) {
                          struct instruction_pointer* cur = callstack_pop(cs);
                          if (!instruction_pointer_is_in_function(cur)) {
                              /* Conditionals only terminate the function, not script-level thingies */
                              /* So we need to place the old position back on the stack */
                              callstack_push(cs, instruction_pointer_with_offset(cur, instruction_pointer_offset(cur) + 6));
                          }
                          struct instruction_pointer* target = instruction_pointer_from_function(fun, 0);
                          instruction_pointer_delete(cur);
                          callstack_push(cs, target);
                          return;
                      }
                      extra_offset = 6;
                      goto offset_manipulation;
                  }
        default: die("unknown opcode");
    }

offset_manipulation:
;
    struct instruction_pointer* ip = callstack_pop(cs);
    callstack_push(cs, instruction_pointer_with_offset(ip, instruction_pointer_offset(ip) + extra_offset));
    instruction_pointer_delete(ip);
}

static void execute() {
    struct instruction_pointer *cur;
    cur = callstack_pop(cs);
    while(cur) {
        const char* next_code;
        if (instruction_pointer_is_in_function(cur)) {
            next_code = function_get(instruction_pointer_function_number(cur));
            if (!next_code) {
                die("Using an undefined function");
            }
        } else {
            next_code = program_code;
        }
        for(int offset = instruction_pointer_offset(cur); next_code[offset] != '\0'; ++offset) {
            if (next_code[offset] == '\n' || next_code[offset] == ' ') {
                continue;
            }
            if(/*DEBUG*/ 0) {
                debug();
                printf("execute: %.6s\n", next_code + offset);
            }
            switch(next_code[offset+1]) {
                case 'x': {
                              // opcodes takes an unknown number of characters and might or might not return to here at all.
                              // In order to do that, we push our current state on callstack,
                              // opcodes will take that from there, put the current next one on it,
                              // and we need to pull that.
                              callstack_push(cs, instruction_pointer_with_offset(cur, offset));
                              opcodes(next_code+offset);
                              goto cleanup_forloop;
                          }
                case 'a': {
                              struct number* acc = accumulator_get();
                              number_add(acc, next_code[offset] - '0');
                              accumulator_set(acc);
                              break;
                          }
                case 's': {
                              struct number* acc = accumulator_get();
                              number_add(acc, -(next_code[offset] - '0'));
                              accumulator_set(acc);
                              break;
                          }
                case 'm': {
                              struct number* acc = accumulator_get();
                              number_multiply(acc, next_code[offset] - '0');
                              accumulator_set(acc);
                              break;
                          }
                case 'd': {
                              struct number* acc = accumulator_get();
                              number_divide(acc, next_code[offset] - '0');
                              accumulator_set(acc);
                              break;
                          }
                case 'p': {
                              struct number* acc = accumulator_get();
                              number_remainder(acc, next_code[offset] - '0');
                              accumulator_set(acc);
                              break;
                          }
                case 'f': {
                              int functon = next_code[offset] - '0';
                              if (next_code[offset+2] != '\0') {
                                  struct instruction_pointer *after = instruction_pointer_with_offset(cur, offset+2);
                                  callstack_push(cs, after);
                              }
                              callstack_push(cs, instruction_pointer_from_function(functon, 0));
                              goto cleanup_forloop;
                          }
                case 'r': {
                              int position = next_code[offset] - '0';
                              if (position != 1)
                                die("R for anything else than 1st is unimplemented for now");
                              int res = getchar();
                              struct number *acc = number_from(res);
                              accumulator_set(acc);
                              break;
                          }
                case 'h': {
                              die("Halt for debugging");
                          }
                case 'o': {
                              struct number *acc = accumulator_get();
                              for (int i=next_code[offset] - '1'; i >= 0; i--) {
                                    number_print(acc);
                              }
                              number_destroy(acc);
                              break;
                          }
                case 'v': {
                              int number = next_code[offset] - '0';
                              struct number *var = variable_get(number);
                              accumulator_set(var);
                              break;
                          }
                case 'n': {
                              int number = next_code[offset] - '0';
                              struct number *var = variable_get(number);
                              number_multiply(var, -1);
                              variable_set(number, var);
                              break;
                          }
                default: die("unknown char for interpreter loop");
            }
            offset++;
        }
cleanup_forloop:
        instruction_pointer_delete(cur);
        cur = callstack_pop(cs);
    }
}

int main(int argc, char** argv) {

    int c;
    const char* self_name = argv[0];
    while ((c = getopt(argc, argv, "u")) != -1) {
        switch(c) {
            case 'u': naz_set_unlimited(1);
                      break;
            default: usage(self_name);
        }
    }
    argc -= optind;
    argv += optind;
    if (argc != 1) {
        usage(self_name);
    }

    variable_init();

    char* program = read_file(argv[0]);

    int is_comment = 0;
    for(int offset = 0; program[offset] != '\0'; ) {
        if (is_comment && program[offset] != '\n') {
            /* Keep the comment flag, clear the content */
            program[offset] = ' ';
            offset++;
            continue;
        } else if (is_comment) {
            offset++;
            is_comment = 0;
            continue;
        }
        if (program[offset] == '#') {
            is_comment = 1;
            program[offset] = ' ';
            offset++;
            continue;
        }
        if (program[offset] >= '0' && program[offset] <= '9') {
            switch(program[offset+1]) {
                case 'a':
                case 'd':
                case 'e':
                case 'f':
                case 'g':
                case 'h':
                case 'l':
                case 'm':
                case 'n':
                case 'o':
                case 'p':
                case 'r':
                case 's':
                case 'v':
                case 'x':
                    break;
                default:
                    die("unexpected tupel");
            }
            work_tupel(program+offset);
            offset += 2;
            continue;
        }
        if (program[offset] == '\n') {
            offset++;
            continue;
        }
        if (program[offset] == ' ') {
            /* Do nothing */
            offset++;
            continue;
        }
        printf("Unexcepted char: %c\n", program[offset]);
        offset++;
    }


    //printf("%s", program);

    program_code = program;
    cs = callstack_new_empty();
    callstack_push(cs, instruction_pointer_from_file(0));

    execute();

    free(program);
    callstack_destroy(cs);
    variable_cleanup();
    function_cleanup();
    return EXIT_SUCCESS;
}
