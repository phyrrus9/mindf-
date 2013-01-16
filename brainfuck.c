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

#define SUCCESS         0
#define FAILURE         1

#define PROGRAM_SIZE    4096
#define STACK_SIZE      512
#define DATA_SIZE       65535

#define STACK_PUSH(A)   (STACK[SP++] = A)
#define STACK_POP()     (STACK[--SP])
#define STACK_EMPTY()   (SP == 0)
#define STACK_FULL()    (SP == STACK_SIZE)

struct instruction_t {
    unsigned short operator;
    unsigned short operand;
};

static struct instruction_t PROGRAM[PROGRAM_SIZE];
static unsigned short STACK[STACK_SIZE];
static unsigned int SP = 0;

unsigned short data[DATA_SIZE];

static unsigned int IN_CONS[DATA_SIZE];
static unsigned int LOOP_LOC[DATA_SIZE / 2];
static unsigned int LOOP_LIMIT[DATA_SIZE / 2];
static unsigned char IN_CONS_C[DATA_SIZE];
static unsigned short IN_CONS_COUNT = 0;
static unsigned short IN_CONS_POS = 0;
static unsigned short IN_CONS_C_COUNT = 0;
static unsigned short IN_CONS_C_POS = 0;
static unsigned short LOOP_COUNT = 0;
static unsigned short LOOP_POS = 0;

int compile_bf(FILE* fp) {
    unsigned short pc = 0, jmp_pc, loop_pc;
    int c, tc;
    while ((c = getc(fp)) != EOF && pc < PROGRAM_SIZE) {
        switch (c) {
            case ' ': break; //ignore whitespace
            case '>': PROGRAM[pc].operator = OP_INC_DP; break;
            case '<': PROGRAM[pc].operator = OP_DEC_DP; break;
            case '+': PROGRAM[pc].operator = OP_INC_VAL; break;
            case '-': PROGRAM[pc].operator = OP_DEC_VAL; break;
            case '.': PROGRAM[pc].operator = OP_OUT; break;
            case ',': PROGRAM[pc].operator = OP_IN; break;
            case '=':
                PROGRAM[pc].operator = OP_IN_CONS;
                fscanf(fp, "%d", &IN_CONS[IN_CONS_COUNT]);
                //printf("Read constant: %d\n", IN_CONS);
                IN_CONS_C_COUNT++;
                break;
            case '@':
                PROGRAM[pc].operator = OP_IN_CONS_C;
                IN_CONS_C[IN_CONS_C_COUNT] = getc(fp);
                //printf("Read constant char: %c\n", IN_CONS_C);
                IN_CONS_C_COUNT++;
                break;
            case '[':
                PROGRAM[pc].operator = OP_JMP_FWD;
                if (STACK_FULL()) {
                    return FAILURE;
                }
                STACK_PUSH(pc);
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
                if (getc(fp) == '/')
                {
                    while (getc(fp) != '\n') { }
                }
                break;
            default: pc--; break;
        }
        pc++;
    }
    if (!STACK_EMPTY() || pc == PROGRAM_SIZE) {
        return FAILURE;
    }
    PROGRAM[pc].operator = OP_END;
    return SUCCESS;
}

int execute_bf() {
    unsigned short pc = 0;
    unsigned int ptr = DATA_SIZE;
    while (--ptr) { data[ptr] = 0; }
    ptr = 0;
    while (PROGRAM[pc].operator != OP_END && ptr < DATA_SIZE) {
        switch (PROGRAM[pc].operator) {
            case OP_INC_DP: ptr++; break;
            case OP_DEC_DP: ptr--; break;
            case OP_INC_VAL: data[ptr]++; break;
            case OP_DEC_VAL: data[ptr]--; break;
            case OP_OUT: putchar(data[ptr]); break;
            case OP_IN: data[ptr] = (unsigned int)getchar(); break;
            case OP_JMP_FWD: if(!data[ptr]) { pc = PROGRAM[pc].operand; } break;
            case OP_JMP_BCK: if(data[ptr]) { pc = PROGRAM[pc].operand; } break;
            case OP_IN_CONS: data[ptr] = IN_CONS[IN_CONS_POS]; IN_CONS_POS++; break;
            case OP_IN_CONS_C: data[ptr] = IN_CONS_C[IN_CONS_C_POS]; IN_CONS_C_POS++; break;
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
    int status;
    FILE *fp;
    if (argc != 2 || (fp = fopen(argv[1], "r")) == NULL) {
        fprintf(stderr, "Usage: %s filename\n", argv[0]);
        return FAILURE;
    }
    memset(data, 0, DATA_SIZE);
    status = compile_bf(fp);
    fclose(fp);
    if (status == SUCCESS) {
        status = execute_bf();
    }
    if (status == FAILURE) {
        fprintf(stderr, "Error!\n");
    }
    return status;
}
