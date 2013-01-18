/*
 mindf+ ( http://github.com/phyrrus9/mindf+ )
 Copyright (c) 2013 Ethan Laur (phyrrus9)
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MPP_VERSION     5

#define OP_END          0
#define OP_INC_DP       1
#define OP_DEC_DP       2
#define OP_INC_VAL      3
#define OP_DEC_VAL      4
#define OP_OUT          5
#define OP_IN           6
#define OP_JMP_FWD      7
#define OP_JMP_BCK      8
#define OP_IN_CONS      9
#define OP_IN_CONS_C    10
#define OP_FOR_LOOP     11
#define OP_CONTINUE     12
#define COMMENT_NO_OP   13
#define COMMENT_ERROR   14
#define OP_SETJMP       15
#define OP_SPECIAL      16
#define OP_GO_JMP       17
#define OP_JMP_IF_END   18
#define OP_FUNC_CALL    19
#define OP_MEM_BLK      20

#define SUCCESS         0
#define FAILURE         1

#define PROGRAM_SIZE    4096
#define STACK_SIZE      512
#define DATA_SIZE       65535

//define some stack options and commands
#define STACK_PUSH(A)   (STACK[SP++] = A)
#define STACK_POP()     (STACK[--SP])
#define STACK_EMPTY()   (SP == 0)
#define STACK_FULL()    (SP == STACK_SIZE)

struct instruction_t {
    unsigned short operator;
    unsigned short operand;
};

static char PROGRAM_SPAWN_CALL[55];

static unsigned char compile = 0;
static unsigned char execute = 0;
static unsigned char memdump = 0;
static unsigned short prog_end = 0;

//define program containers (and stacks)
static struct instruction_t PROGRAM[PROGRAM_SIZE];
static unsigned short STACK[STACK_SIZE];
static unsigned int JUMPS[STACK_SIZE];
static unsigned int IN_CONS[STACK_SIZE];
static unsigned int FCALLS[STACK_SIZE];
static unsigned int SP = 0;
static unsigned int JP = 0;
static unsigned int JC = 0;
static unsigned int CP = 0;
static unsigned int CC = 0;
static unsigned int FC = 0;
static unsigned int FP = 0;


unsigned short data[DATA_SIZE];

static unsigned int LOOP_LOC[DATA_SIZE / 2];
static unsigned int LOOP_LIMIT[DATA_SIZE / 2];
static unsigned char IN_CONS_C[DATA_SIZE];
static unsigned short LOOP_COUNT = 0;
static unsigned short LOOP_POS = 0;

static int JMP_TEST_VAL = 0;
static int GO_TO_X = 0;

void call_function(int func_num)
{
    char func_file_name[17];
    char func_call_comd[52];
    FILE *fp;
    int i;
    sprintf(func_file_name, "%d.ms", func_num);
    fp = fopen(func_file_name, "wb");
    for (i = 0; i < PROGRAM_SIZE; i++) {
        fprintf(fp, "%c", data[i]);
    }
    fclose(fp);
    sprintf(func_call_comd, "%s em %d.mp %s", PROGRAM_SPAWN_CALL, func_num, func_file_name);
    system(func_call_comd);
}

int compile_bf(FILE* fp) {
    unsigned short pc = 0, jmp_pc, loop_pc;
    int c, tc;
    while ((c = getc(fp)) != EOF && pc < PROGRAM_SIZE) {
        switch (c) {
            //case ' ': break; //ignore whitespace
            case '>': PROGRAM[pc].operator = OP_INC_DP; break;
            case '<': PROGRAM[pc].operator = OP_DEC_DP; break;
            case '+': PROGRAM[pc].operator = OP_INC_VAL; break;
            case '-': PROGRAM[pc].operator = OP_DEC_VAL; break;
            case '.': PROGRAM[pc].operator = OP_OUT; break;
            case ',': PROGRAM[pc].operator = OP_IN; break;
            case 'B': PROGRAM[pc].operator = OP_MEM_BLK; break;
            case '=':
                PROGRAM[pc].operator = OP_IN_CONS;
                fscanf(fp, "%d", &tc);
                IN_CONS[CC++] = tc;
                break;
            case '@':
                PROGRAM[pc].operator = OP_IN_CONS_C;
                tc = getc(fp);
                IN_CONS[CC++] = tc;
                break;
            case '[':
                PROGRAM[pc].operator = OP_JMP_FWD;
                if (STACK_FULL()) {
                    return FAILURE;
                }
                STACK_PUSH(pc);
                break;
            case '}':
                if ( STACK_EMPTY() ) {
                    printf("stack empty\n");
                    return FAILURE;
                }
                PROGRAM[pc].operator = OP_JMP_IF_END;
                jmp_pc = STACK_POP();
                break;
            case ']':
                if (STACK_EMPTY()) {
                    return FAILURE;
                }
                jmp_pc = STACK_POP();
                PROGRAM[pc].operator =  OP_JMP_BCK;
                PROGRAM[pc].operand = jmp_pc;
                PROGRAM[jmp_pc].operand = pc;
                break;
            case '#':
                fscanf(fp, "%d:%d|", &LOOP_LOC[LOOP_COUNT], &LOOP_LIMIT[LOOP_COUNT]);
                LOOP_COUNT++;
                PROGRAM[pc].operator = OP_FOR_LOOP;
                if (STACK_FULL()) {
                    return FAILURE;
                }
                STACK_PUSH(pc);
                break;
            case ';':
                if (STACK_EMPTY()) {
                    return FAILURE;
                }
                loop_pc = STACK_POP();
                PROGRAM[pc].operator = OP_CONTINUE;
                PROGRAM[pc].operand = loop_pc;
                PROGRAM[loop_pc].operand = pc;
                break;
            case '/':
                PROGRAM[pc].operator = COMMENT_ERROR;
                if (getc(fp) == '/')
                {
                    pc--;
                    PROGRAM[pc].operator = COMMENT_NO_OP;
                    while (getc(fp) != '\n') { }
                }
                break;
            case '$':
                PROGRAM[pc].operator = OP_SETJMP;
                fscanf(fp, "%d", &tc);
                JUMPS[JC++] = tc;
                break;
            case 'S':
                fscanf(fp, "%c", (char *)&tc);
                switch (tc) {
                    case '0':
                        tc = '\0';
                        break;
                    case 'n':
                        tc = '\n';
                        break;
                    default: break;
                }
                IN_CONS[CC++] = tc;
                PROGRAM[pc].operator = OP_SPECIAL;
                break;
            case 'H':
                PROGRAM[pc].operator = OP_GO_JMP;
                break;
            case 'C':
                fscanf(fp, "%d", &tc);
                FCALLS[FC++] = tc;
                PROGRAM[pc].operator = OP_FUNC_CALL;
                break;
            default: pc--; break;
        }
        pc++;
    }
    if (!STACK_EMPTY() || pc == PROGRAM_SIZE) {
        return FAILURE;
    }
    PROGRAM[pc].operator = OP_END;
    prog_end = pc;
    return SUCCESS;
}

int execute_bf() {
    unsigned short pc = 0;
    unsigned int ptr = DATA_SIZE;
    //while (--ptr) { data[ptr] = 0; }
    ptr = 0;
    while (PROGRAM[pc].operator != OP_END && ptr < DATA_SIZE) {
        switch (PROGRAM[pc].operator) {
            case OP_INC_DP: ptr++; break;
            case OP_DEC_DP: ptr--; break;
            case OP_INC_VAL: data[ptr]++; break;
            case OP_DEC_VAL: data[ptr]--; break;
            case OP_OUT: putchar(data[ptr]); break;
            case OP_IN: data[ptr] = (unsigned short)getchar(); break;
            case OP_JMP_FWD: if(data[ptr] == JMP_TEST_VAL) { pc = PROGRAM[pc].operand; } break;
            case OP_JMP_BCK: if(data[ptr] != JMP_TEST_VAL) { pc = PROGRAM[pc].operand; } break;
            case OP_JMP_IF_END: break;
            case OP_IN_CONS:
            case OP_SPECIAL:
            case OP_IN_CONS_C: data[ptr] = IN_CONS[CP++]; break;
            case OP_GO_JMP: ptr = 0; break;
            case OP_MEM_BLK: memset(data, 0, DATA_SIZE); break;
            case OP_FUNC_CALL:
                call_function(FCALLS[FP++]);
                break;
            case OP_SETJMP:
                JMP_TEST_VAL = JUMPS[JP++];
                break;
            case COMMENT_NO_OP: break;
            case COMMENT_ERROR: return FAILURE;
            case OP_FOR_LOOP:
                if (data[LOOP_LOC[LOOP_POS]] < LOOP_LIMIT[LOOP_POS]) {
                    data[LOOP_LOC[LOOP_POS]]++;
                }
                break;
                case OP_CONTINUE:
                if (data[LOOP_LOC[LOOP_POS]] < LOOP_LIMIT[LOOP_POS]) {
                    data[LOOP_LOC[LOOP_POS]]++;
                    pc = PROGRAM[pc].operand;
                }
                else {
                    LOOP_POS++;
                }
                break;
            default: return FAILURE;
        }
        pc++;
    }
    return ptr != DATA_SIZE ? SUCCESS : FAILURE;
}

int main(int argc, const char * argv[])
{
    int status, i;
    FILE *fp;
    char memdumpfname[17];
    sprintf(PROGRAM_SPAWN_CALL, "%s", argv[0]);
    memset(data, 0, DATA_SIZE);
    memset(JUMPS, 0, STACK_SIZE);
    if (argc == 2) {
        fp = fopen(argv[1], "r");
    }
    else if (argc >= 3) {
        if (argv[1][0] == 'c') {
            compile = 1;
        }
        if (argv[1][0] == 'e') {
            if (argv[1][1] == 'm' && argc > 3) {
                memdump = atoi(argv[3]);
                //printf("memory loaded: %d.ms\n", memdump);
                sprintf(memdumpfname, "%d.ms", memdump);
                fp = fopen(memdumpfname, "rb");
                for (i = 0; i < DATA_SIZE; i++) {
                    fscanf(fp, "%c", (char *)&data[i]);
                }
                fclose(fp);
                //exit(1);
            }
            execute = 1;
        }
        fp = fopen(argv[2], "r");
    }
    else {
        fprintf(stderr, "Version: %d\n"
                        "Usage: %s <opts> <file> <memdump>\n"
                        "opts: optional field\n"
                        "->c - compile\n"
                        "->e - execute\n"
                        "->em- execute function (requires memdump)\n"
                        "file: required field\n"
                        "->filename of program or function\n"
                        "memdump: used only with em\n"
                        "->specifies memory image to load\n"
                        "-->NOT FOR CONSUMER USE\n",
                        MPP_VERSION, argv[0]);
        exit(-1);
    }
    if(execute == 0) {
        status = compile_bf(fp);
    }
    fclose(fp);
    if (compile == 1 && execute == 0) {
        fp = fopen("out.mp", "wb");
        for (i = 0; i < prog_end; i++) {
            fprintf(fp, "%c", (char)PROGRAM[i].operator);
            switch (PROGRAM[i].operator)
            {
                case OP_JMP_FWD:
                case OP_JMP_BCK:
                    fprintf(fp, "%c", (char)PROGRAM[i].operand);
                    break;
                case OP_IN_CONS:
                case OP_SPECIAL:
                case OP_IN_CONS_C:
                    fprintf(fp, "%c", (char)IN_CONS[CP++]);
                    break;
                case OP_SETJMP:
                    fprintf(fp, "%c", (char)JUMPS[JP++]);
                    break;
                case OP_FUNC_CALL:
                    fprintf(fp, "%c", (char)FCALLS[FP++]);
                    break;
            }
        }
        fclose(fp);
        return 0;
    }
    if (compile == 0 && execute == 1) {
        i = 0;
        fp = fopen(argv[2], "rb");
        while ( fscanf(fp, "%c", (char *)&PROGRAM[i].operator) != EOF) {
            switch (PROGRAM[i].operator)
            {
                case OP_JMP_FWD:
                case OP_JMP_BCK:
                    fscanf(fp, "%c", (char *)&PROGRAM[i].operand);
                    break;
                case OP_IN_CONS:
                case OP_SPECIAL:
                case OP_IN_CONS_C:
                    fscanf(fp, "%c", (char *)&IN_CONS[CC++]);
                    break;
                case OP_SETJMP:
                    fscanf(fp, "%c", (char *)&JUMPS[CC++]);
                    break;
                case OP_FUNC_CALL:
                    fscanf(fp, "%c", (char *)&FCALLS[FC++]);
                    break;
            }
            prog_end = i;
            i++;
        }
        status = SUCCESS;
    }
    if (status == SUCCESS) {
        status = execute_bf();
    }
    if (status == FAILURE) {
        fprintf(stderr, "Error!\n");
    }
    return status;
}
