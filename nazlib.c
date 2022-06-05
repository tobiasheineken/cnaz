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
#include <limits.h>
#include <string.h>
#include "nazlib.h"

struct instruction_pointer {
    int function;
    int offset;
    struct instruction_pointer* next;
};


static int unlimited_numbers = 0;
static int debug = 0;

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

struct lnumber {
    long long val;
};
struct unumber {
    size_t len;
    size_t cap;
    unsigned int* data;
    int negative;
};

struct number {
    union {
        struct lnumber* lptr;
        struct unumber* uptr;
    };
};
static void unumber_destroy(struct unumber* in);

static struct lnumber* lnumber_from(int i) {
    struct lnumber* out = malloc(sizeof(*out));
    out->val = i;
    return out;
}

static struct unumber* unumber_from(int i) {
    struct unumber* out = malloc(sizeof(*out));
    out->cap = 2;/* We need at least 2 so that *1.5 actually increases stuff */
    out->data = malloc(sizeof(int) * out->cap);
    out->len = 1;
    out->negative = 0;
    if (i == INT_MIN) {
        die("from INT_MIN is not supported");
    } else if (i < 0) {
        i = -i;
        out->negative = 1;
    }
    out->data[0] = i;
    return out;
}

struct number* number_from(int i) {
    struct number* out = malloc(sizeof(*out));
    if (unlimited_numbers) {
        out->uptr = unumber_from(i);
    } else {
        out->lptr = lnumber_from(i);
    }
    return out;
}


static void unumber_check (struct unumber* check) {
    if (check->data == NULL) {
        die("using an undef number");
    }
}

struct number* number_invalid() {
    if (unlimited_numbers) {
        struct number* n_out = malloc(sizeof(*n_out));
        struct unumber* u_out = malloc(sizeof(*u_out));
        n_out->uptr = u_out;
        u_out->data = NULL;
        u_out->len = 0;
        u_out->cap = 0;
        u_out->negative = 0;
        return n_out;
    }
    return number_from(-128);
}

static struct lnumber* lnumber_copy(struct lnumber* in) {
    struct lnumber* out = malloc(sizeof(*out));
    out->val = in->val;
    return out;
}

static struct unumber* unumber_copy(struct unumber* in) {
    struct unumber* out = malloc(sizeof(*out));
    out->len = in->len;
    out->cap = in->len;
    out->data = malloc(sizeof(int) * out->cap);
    out->negative = in->negative;
    memcpy(out->data, in->data, in->len * sizeof(int));
    return out;
}

struct number* number_copy(struct number* in) {
    struct number* out = malloc(sizeof(*out));
    if (unlimited_numbers) {
        out->uptr = unumber_copy(in->uptr);
    } else {
        out->lptr = lnumber_copy(in->lptr);
    }
    return out;
}

static void lnumber_add(struct lnumber* in, int i) {
    in->val += i;
    if (in->val < -127 || in->val > 127) {
        die("invalid result");
    }
}

static size_t unumber_new_size(struct unumber* in) {
    size_t new_size = in->cap;
    size_t add_size = new_size / 2;
    if (new_size + add_size > 1000) {
        add_size = 10;
    }
    return new_size + add_size;
}

static void unumber_enlarge(struct unumber* in, size_t new_size) {
    unsigned int* old_data = in->data;
    in->data = malloc(sizeof(unsigned int) * new_size);
    size_t min_size = (in->cap < new_size)? in->cap : new_size;
    in->cap = new_size;
    memcpy(in->data, old_data, min_size * sizeof(unsigned int));
    free(old_data);
}

static void unumber_fit_len(struct unumber* in) {
    while((in->len > 1) && (in->data[in->len-1] == 0)) in->len--;
}

static void unumber_add(struct unumber* in, unsigned i, int start_offset) {
    unsigned carry = i;
    int offset = start_offset;
    while(carry != 0) {
        if (offset >= in->cap) {
            unumber_enlarge(in, unumber_new_size(in));
        }
        if (offset >= in->len) {
            in->len++;
            in->data[offset] = carry;
            break;
        }
        unsigned long long temp = in->data[offset];
        temp += carry;
        unsigned low_end = temp&0xffffffff;
        unsigned high_end = temp >> 32;
        in->data[offset] = low_end;
        carry = high_end;
        offset++;
    }
}

static void unumber_sub(struct unumber* in, unsigned i) {
    unumber_fit_len(in);
    if (in->len == 1) {
        /* Normal subtraction */
        unsigned* data = in->data;
        if (*data >= i) {
            *data -= i;
            return;
        } else {
            unsigned abs_res = i - *data;
            in->negative = !in->negative;
            *data = abs_res;
            return;
        }
    }
    /* We can borrow stuff if we need to */
    unsigned* data = in->data;
    while(i > 0) {
        if (*data >= i) {
            *data -= i;
            return;
        } else {
            i -= 1;
            unsigned borrow = 0xffffffff;
            *data = borrow - i;
            i = 1;
            data++;
        }
    }
}

static void unumber_add_sub(struct unumber* in, int i) {
    if (i == INT_MIN) {
        die("handling INT_MIN is not supported");
    }
    unumber_check(in);
    if (i < 0 && in->negative) {
        unumber_add(in, -i, 0);
    } else if (i > 0 && !in->negative) {
        unumber_add(in, i, 0);
    } else if (i > 0 && in->negative) {
        unumber_sub(in, i);
    } else if( i < 0 && !in->negative) {
        unumber_sub(in, -i);
    }
}

void number_add(struct number* in, int i) {
    if (unlimited_numbers) {
        unumber_add_sub(in->uptr, i);
    } else {
        lnumber_add(in->lptr, i);
    }
}

static void lnumber_multiply(struct lnumber* in, int i) {
    in->val *= i;
    if (in->val < -127 || in->val > 127) {
        die("invalid result");
    }
}

static struct unumber* unumber_multiply(struct unumber* in, int i) {
    if (i == INT_MIN) {
        die("hundling INT_MIN is not supported");
    }
    unumber_check(in);
    struct unumber* out = unumber_from(0);
    if (i < 0) {
        out->negative = !in->negative;
        i = -i;
    }

    unsigned old_overflow = 0;
    for(int offset = 0; offset < in->len; offset++) {
        unumber_add(out, old_overflow, offset);
        unsigned long long full_mul = in->data[offset];
        full_mul *= i;
        unsigned low_end = full_mul & 0xffffffff;
        old_overflow = full_mul >> 32;
        unumber_add(out, low_end, offset);
        if (debug) {
            printf("This is in debug mode\n");
        }
    }
    if (old_overflow != 0)
        unumber_add(out, old_overflow, in->len);


    return out;
}

void number_multiply(struct number* in, int i) {
    if (unlimited_numbers) {
        struct unumber* res = unumber_multiply(in->uptr, i);
        unumber_destroy(in->uptr);
        in->uptr = res;
    } else {
        lnumber_multiply(in->lptr, i);
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

static void lnumber_divide(struct lnumber* in, int i) {
    in->val = divide_round_down(in->val, i);
}

// Does the real divide (round to 0), so that out * rhs + rem = in
// If someone needs something different, they can fix that afterwards :P
// Also does not copy in->negative to out.
static struct unumber* unumber_divide_rem(struct unumber* in, unsigned* rem, unsigned rhs) {

    unumber_check(in);

    if (in->len == 0) {
        die("We should not have a number with len 0");
    }

    struct unumber* out = unumber_from(0);
    unumber_enlarge(out, in->len);
    out->len = in->len;
    unsigned int carry = 0;
    for (size_t i = in->len; i > 0 ; i--) {
        if (carry > rhs) {
            die("BUG in unumber_div");
        }
        unsigned int lhs = in->data[i-1];
        unsigned long long true_lhs = carry;
        true_lhs <<= 32;
        true_lhs += lhs;
        unsigned long long div_res = true_lhs / rhs;
        if (div_res >> 32) {
            die("BUG in unumber_div: result should be at offset+1??");
        }
        out->data[i-1] = div_res & 0xffffffff;
        carry = true_lhs % rhs;
    }

    *rem = carry;
    return out;
}

static void unumber_divide(struct number* in, int i) {
    if (i == INT_MIN) {
        die("hundling INT_MIN is not supported");
    }
    if (i == 0) {
        die("Dividing by 0 is not allowed");
    }
    unumber_check(in->uptr);
    if (i < 0) {
        i = -i;
        in->uptr->negative = !in->uptr->negative;
    }

    unsigned rem;

    struct unumber* res = unumber_divide_rem(in->uptr, &rem, i);
    struct unumber* old = in->uptr;
    in->uptr = res;
    if (old->negative) {
        res->negative = 1;
        if (rem != 0) {
            unumber_add(res, 1, 0);
        }
    }

    unumber_destroy(old);
}

void number_divide(struct number* in, int i) {
    if (unlimited_numbers) {
        unumber_divide(in, i);
    } else {
        lnumber_divide(in->lptr, i);
    }
}
static void lnumber_remainder(struct lnumber* in, int i) {
    in->val %= i;
}

static void unumber_remainder(struct unumber* in, int i) {
    if (i <= 0) {
        die("remainder by negative numbers is not well defined");
    }
    unsigned int out;
    struct unumber* div = unumber_divide_rem(in, &out, i);
    unumber_destroy(div);
    in->data[0] = out;
    in->len = 1;
}

void number_remainder(struct number* in, int i) {
    if (unlimited_numbers) {
        unumber_remainder(in->uptr, i);
    } else {
        lnumber_remainder(in->lptr, i);
    }
}

static void lnumber_destroy(struct lnumber* in) {
    free(in);
}

static void unumber_destroy(struct unumber* in) {
    free(in->data);
    free(in);
}

void number_destroy(struct number* in) {
    if (unlimited_numbers) {
        unumber_destroy(in->uptr);
    } else {
        lnumber_destroy(in->lptr);
    }
    free(in);
}

static void unumber_print(struct unumber* in) {
    unumber_check(in);
    if (in->len == 1) {
        if (in->negative) {
            die("Printing negative numbers is not implemented"); /* TODO */
        }
        if (in->data[0] < 10) {
            printf("%u", in->data[0]);
            return;
        }
        if (in->data[0] != 10 && in->data[0] < 32) {
            return;
        }
    }
    if(printf("%lc", in->data[0] & 0xffff) < 0) {
        perror("Foo");
    }
}

static void lnumber_print(struct lnumber* in) {
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

void number_print(struct number* in) {
    if(unlimited_numbers) {
        unumber_print(in->uptr);
    } else {
        lnumber_print(in->lptr);
    }
}

static void lnumber_print_dbg(struct lnumber* in) {
    printf("%lld", in->val);
}

static void unumber_print_dbg(struct unumber* in) {
    printf("{ \n .cap = %lu,\n .len = %lu,\n .neg = %d\n .data = {", in->cap, in->len, in->negative);
    const char* sep = "";
    for (int i=0; i < in->len; ++i){
        printf("%s%u", sep, in->data[i]);
        sep = ", ";
    }
    printf("}}\n");
}

void number_print_dbg(struct number* in) {
    if (unlimited_numbers) {
        unumber_print_dbg(in->uptr);
    } else {
        lnumber_print_dbg(in->lptr);
    }
}

static int lnumber_compare(struct lnumber* lhs, struct lnumber* rhs) {
    return lhs->val - rhs->val;
}

static int unumber_compare(struct unumber* lhs, struct unumber* rhs) {
    unumber_check(lhs);
    unumber_check(rhs);
    unumber_fit_len(lhs);
    unumber_fit_len(rhs);
    // check -0 vs 0
    if (lhs->len == 1 && rhs->len == 1 && lhs->data[0] == 0 && rhs->data[0] == 0) {
        return 0;
    }

    // check signs
    if (lhs->negative && !rhs->negative) {
        return -1;
    }
    if (rhs->negative && !lhs->negative) {
        return 1;
    }

    // They have the same sign
    if (rhs->negative) {
        // if they are negative, their result are inverted:
        // 5 > 3, but -5 < -3.
        // To evaluate if -5 <=> -3, we can also evaluate 3 <=> 5 [both are -1]
        struct unumber* tmp = lhs;
        lhs = rhs;
        rhs = tmp;
    }

    if (lhs->len > rhs->len) {
        return 1;
    }
    if (rhs->len > lhs->len) {
        return -1;
    }

    // Their lengths are also equal
    for(int i = 0; i < rhs->len; ++i) {
        int offset = rhs->len - 1 - i;
        if (lhs->data[offset] < rhs->data[offset]) {
            return -1;
        }
        if (lhs->data[offset] > rhs->data[offset]) {
            return 1;
        }
    }

    // They are equal
    return 0;
}

int number_compare(struct number* lhs, struct number* rhs) {
    if (unlimited_numbers) {
        return unumber_compare(lhs->uptr, rhs->uptr);
    } else {
        return lnumber_compare(lhs->lptr, rhs->lptr);
    }
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

void naz_set_unlimited(int in) {
    unlimited_numbers = in;
}

void naz_set_debug(int in) {
    debug = in;
}


struct in_state {
    int data[10];
    int start;
    int end;
    int size;
} io = {.start = 0, .end = 0, .size = 0, .data[5] = 1};

#define ARRAYSZ(X) (sizeof(X) / sizeof(X[0]))

static void normalize_io_ptr(int* ptr) {
    int sz = ARRAYSZ(io.data);
    while (*ptr >= sz) {
        *ptr -= sz;
    }
    while(*ptr < 0) {
        *ptr += sz;
    }
}

static void push_in(int read) {
    if (io.size > 9 || io.size < 0) {
        die("Pushing IO with wrong sizes");
    }
    io.data[io.end++] = read;
    io.size++;
    normalize_io_ptr(&io.end);
}

static int pop_in() {
    if (io.size < 1) {
        die("Popping empty IO");
    }
    io.size--;
    int ret = io.data[io.start++];
    normalize_io_ptr(&io.start);
    return ret;
}

static int in_get_and_remove(int position) {
    int access = io.start + position;
    normalize_io_ptr(&access);
    int ret = io.data[access];
    if (position < 1.0 * io.size / 2) {
        // In the first half, i.e. easiest to move right
        if (access >= io.start) {
            for(;access > io.start;access--) {
                io.data[access] = io.data[access - 1];
            }
            pop_in();
            return ret;
        } else {
            for (; access > 0; access--) {
                io.data[access] = io.data[access - 1];
            }
            /* io.start can be the last item, but who cares */
            io.data[0] = io.data[ARRAYSZ(io.data)-1];
            for(access = ARRAYSZ(io.data)-1; access > io.start; access--) {
                io.data[access] = io.data[access-1];
            }
            io.start++;
            normalize_io_ptr(&io.start);
            io.size--;
            return ret;
        }
    } else {
        // In the second half, i.e. easiest to move left
        if (access < io.end) {
            for(;access < io.end - 1;access++) {
                io.data[access] = io.data[access + 1];
            }
            io.end--;
        } else {

            for(;access < ARRAYSZ(io.data) - 1; access++) {
                io.data[access] = io.data[access + 1];
            }
            if (0 < io.end) {
                io.data[ARRAYSZ(io.data) - 1] = io.data[0];
            }
            for(access = 0; access < io.end - 1; access++) {
                io.data[access] = io.data[access + 1];
            }
            io.end--;
            if (io.end < 0) {
                io.end += ARRAYSZ(io.data);
            }
        }
        io.size--;
        return ret;
    }
}

int read_by_offset(int position) {
    if (position < 1) {
        die("0r is not a valid command");
    }
    if (position <= io.size) {
        // We already did read this byte from stdin
        // We just have to report it correctly
        return in_get_and_remove(position - 1);
    }
    position -= io.size;
    for(;position > 1; position--) {
        push_in(getchar());
    }
    if (position != 1)
      die("R for anything else than 1st should not reach here");
    int res = getchar();
    return res;
}

void debug_io_state() {
    const char* sep = "{";

    for(int i=0; i < 10; ++i) {
        printf("%s.data[%d] = %d", sep, i, io.data[i]);
        sep = ", ";
    }
    printf(", .start = %d, .end = %d, .size = %d \n", io.start, io.end, io.size);
    printf("Interpreted: \n");
    sep = "";
    for(int i=0; i < io.size; ++i) {
        int acc = io.start + i;
        normalize_io_ptr(&acc);
        printf("%s[%d] = %d", sep, i, io.data[acc]);
        sep = ", ";
    }
    printf("\n");
}
