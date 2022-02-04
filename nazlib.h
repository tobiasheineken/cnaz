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


/** INSTRUCTION POINTER */
struct instruction_pointer;
struct instruction_pointer* instruction_pointer_from_function(int function, int offset);
struct instruction_pointer* instruction_pointer_from_file(int offset);
struct instruction_pointer* instruction_pointer_with_offset(struct instruction_pointer*, int offset);

int instruction_pointer_is_in_function(struct instruction_pointer*);
int instruction_pointer_function_number(struct instruction_pointer*);
int instruction_pointer_offset(struct instruction_pointer*);

void instruction_pointer_delete(struct instruction_pointer*);

/** CALLSTACK */
struct callstack;
struct callstack* callstack_new_empty();

/* returns NULL if no further elements are on the callstack
 * returned element is in the ownership of the caller
 */
struct instruction_pointer* callstack_pop(struct callstack*);
/* Takes ownership of struct instruction_pointer */
void callstack_push(struct callstack*, struct instruction_pointer*);
void callstack_iterate(struct callstack*, void(*callback)(struct instruction_pointer*));

void callstack_destroy(struct callstack*);

/** NUMBERS */
struct number;
struct number* number_copy(struct number*);
struct number* number_invalid();
struct number* number_from(int i);

void number_add(struct number*, int);
void number_divide(struct number*, int);
void number_multiply(struct number*, int);
void number_remainder(struct number*, int);

void number_print(struct number*);
void number_print_dbg(struct number*);

int number_compare(struct number*, struct number*);


void number_destroy(struct number*);


/** VARIABLES */
/* returns a copy of the number stored under a given variable */
struct number* variable_get(int number);
struct number* accumulator_get();
/* Takes ownership of the struct number* */
void variable_set(int number, struct number*);
void accumulator_set(struct number*);

void variable_init();
void variable_cleanup();


/** FUNCTIONS */
const char* function_get(int number);
/* Does NOT take ownership of the string */
const char* function_set(int number, const char*);

void function_cleanup();

_Noreturn void die(const char msg[]);

/** -u */
/* Has to be called before variable_init() */
void naz_set_unlimited(int);
