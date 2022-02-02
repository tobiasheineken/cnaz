/*
    Copyright (C) 2022 Tobias Heineken

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

*/

#include <stdlib.h>
#include <stdio.h>
#include "nazlib.h"

struct instruction_pointer {
    int function;
    int offset;
    struct instruction_pointer* next;
};

struct instruction_pointer* instruction_pointer_from_function(int function, int offset) {
    struct instruction_pointer *out = malloc(sizeof(*out));
    out->function = function;
    out->offset = offset;
    out->next = NULL;
    return out;
}

struct instruction_pointer* instruction_pointer_from_file(int offset) {
    return instruction_pointer_from_function(-1, offset);
}

struct instruction_pointer* instruction_pointer_with_offset(struct instruction_pointer *arg, int offset) {
    return instruction_pointer_from_function(arg->function, offset);
}

int instruction_pointer_is_in_function(struct instruction_pointer *arg) {
    return arg->function > -1;
}

int instruction_pointer_function_number(struct instruction_pointer *arg) {
    return arg->function;
}

int instruction_pointer_offset(struct instruction_pointer *arg) {
    return arg->offset;
}

void instruction_pointer_delete(struct instruction_pointer *arg) {
    /* As next is part of the callstack, we do not touch it here */
    free(arg);
}

struct callstack {
    struct instruction_pointer *top;
};

struct callstack* callstack_new_empty() {
    struct callstack *out = malloc(sizeof(*out));
    out->top = NULL;
    return out;
}

void callstack_push(struct callstack *cs, struct instruction_pointer *ip) {
    ip->next = cs->top;
    cs->top = ip;
}

struct instruction_pointer* callstack_pop(struct callstack *cs) {
    struct instruction_pointer *out = cs->top;
    if (out)
        cs->top = out->next;
    return out;
}

void callstack_iterate(struct callstack *cs, void(*callback)(struct instruction_pointer*)) {
    struct instruction_pointer* cur = cs->top;
    while(cur) {
        struct instruction_pointer* next = cur->next;
        callback(cur);
        cur = next;
    }
}

void callstack_destroy(struct callstack *cs) {
    callstack_iterate(cs, instruction_pointer_delete);
    free(cs);
}

struct number {
    long long val;
    /* TODO */
};


struct number* number_from(int i) {
    struct number* out = malloc(sizeof(*out));
    out->val = i;
    return out;
}

struct number* number_invalid() {
    return number_from(-128);
}

struct number* number_copy(struct number* in) {
    struct number* out = malloc(sizeof(*out));
    out->val = in->val; /* TODO */
    return out;
}

void number_add(struct number* in, int i) {
    in->val += i;
    if (in->val < -127 || in->val > 127) {
        die("invalid result");
    }
}

void number_multiply(struct number* in, int i) {
    in->val *= i;
    if (in->val < -127 || in->val > 127) {
        die("invalid result");
    }
}

static long long divide_round_down(long long val, int rhs) {
    if ((val < 0) == (rhs < 0)) {
        /* End result will be positive */
        return val / rhs;
    }
    /* End result will be negative */
    int res = val / rhs;
    int rem = val % rhs;
    if (rem == 0) return res;
    return res - 1;
}

void number_divide(struct number* in, int i) {
    in->val = divide_round_down(in->val, i);
}
void number_remainder(struct number* in, int i) {
    in->val %= i;
}

void number_destroy(struct number* in) {
    free(in);
}

void number_print(struct number* in) {
    if (in->val >= 0 && in-> val < 10) {
        printf("%lld", in->val);
        return;
    }
    if (in->val == 10 || (in->val >= 32 && in->val <= 126)) {
        printf("%c", (char)in->val);
        return;
    }
    die("trying to print unknown number");
}

void number_print_dbg(struct number* in) {
    printf("%lld", in->val);
}

int number_compare(struct number* lhs, struct number* rhs) {
    return lhs->val - rhs->val;
}



/* VARIABLES */

static struct number* variables[10];
static struct number* accumucator;

struct number* variable_get(int number) {
    return number_copy(variables[number]);
}

struct number* accumulator_get() {
    return number_copy(accumucator);
}

void variable_set(int number, struct number* val) {
    number_destroy(variables[number]);
    variables[number] = val;
}

void accumulator_set(struct number *val) {
    number_destroy(accumucator);
    accumucator = val;
}

void variable_init() {
    for(int i=0; i < sizeof(variables) / sizeof(variables[0]); ++i)
    {
        variables[i] = number_invalid();
    }
    accumucator = number_from(0);
}

void variable_cleanup() {
    for(int i=0; i < sizeof(variables) / sizeof(variables[0]); ++i)
    {
        number_destroy(variables[i]);
    }
    number_destroy(accumucator);
}


/* FUNCTIONS */

#include <stdio.h>
#include <string.h>

static char* functions[10] = {0};

const char* function_get(int number) {
    return functions[number];
}
/* Does NOT take ownership of the string */
const char* function_set(int number, const char* string) {
    if (functions[number]) {
        fprintf(stderr, "Redefining function %d, aborting\n", number);
        exit(EXIT_FAILURE);
    }
    char* temp_str = strdup(string);
    for (char* c = temp_str ; *c != '\0'; ++c) {
        if (*c == '\n') {
            *c = '\0';
            break;
        }
        if (*c == '#') {
            *c = '\0';
            break;
        }
        if (*c == '0' && c[1] == 'x') {
            *c = '\0';
            break;
        }
    }
    functions[number] = strdup(temp_str);
    free(temp_str);
}

void function_cleanup() {
    for(int i=0; i < sizeof(functions) / sizeof(functions[0]); ++i){
        free(functions[i]);
        functions[i] = NULL;
    }
}
