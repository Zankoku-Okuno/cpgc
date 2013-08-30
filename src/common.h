#ifndef COMMON_H
#define COMMON_H

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#define CONTRIB_URL "github.com/Zankoku-Okuno/cpgc"

// ====== Types ======

/** Boolean type. */
typedef enum {false, true} bool;

/** Unsigned 8-bit. */
typedef unsigned char byte;
/** Shortcut for unsigned int. */
typedef unsigned int uint;

/** Unsigned integer that fills a machine word. */
typedef size_t word;
/** Signed integer that fills a machine word. */
typedef ptrdiff_t word_diff;

/** Type for NUL-terminated char*, as distinct from arbitrary char*. */
typedef char* cstring;

// ====== Simple Stuff ======

/** Cuts down on a few characters. Same as `else if`. */
#define elif else if

/** Explicit null statement. */
#define pass (void)0

// ====== Meta-macros ======

#define TOKENPASTE_IMPL(x, y) x ## y
#define TOKENPASTE(x, y) TOKENPASTE_IMPL(x, y)

/** Ensure that statement-y macros won't screw up the parser. */
#define MACRO_STATEMENT(...) \
    if (true) { __VA_ARGS__ } \
    else pass

/** Support new keywords that introduce blocks of code that can be broken out of. */
#define done while(0)

// ====== Errors ======

/** Print a message to stderr and die. */
#define error(msg, ...) MACRO_STATEMENT(\
    fprintf(stderr, msg, __VA_ARGS__);\
    abort();\
)

/** Mark unreachable/unimplemented sections of code cause loud failure. */
#define unreachable error("%s:%d -- unreachable code\n", __FILE__, __LINE__)

#define unimplemented error(\
    "Unimplemented: %s,%d.\n\tPlease help us by contributing at %s.\n",\
    __FILE__, __LINE__, CONTRIB_URL)

// ====== Loop Constructs ======

/** A loop that continues forever, or until broken. */
#define loop while(true)

/** Early exit from a loop. */
#define until(cond) if (!(cond)) break;

#define repeat(name, start, term) \
    for(int name = start, TOKENPASTE(_end, __LINE__) = term;\
        name < TOKENPASTE(_end, __LINE__);\
        ++name)

// ====== Goto Constructs ======

/** Cases that don't fall through.
    Between this, `otherwise` and `fallinto`, `case` and `default` are deprecated.
*/
#define match break; case
/** Flag for the human to realize that the next case can be fallen into.
    Between this and `fallinto`, bare `default` is deprecated.
*/
#define otherwise break; default
/** Flag for the human to realize that the next case can be fallen into.
    Between this and `match`, `case` is deprecated.
*/
#define fallinto case

/** Define an exitwhen block.
    Multiple exit was introduced by Charles Zahn in 1974, and has somehow not been introduced in
    any language I know of, despite its utility.
    In this implementation, any statements may be executed between `exitwhen`...`done`.
    A break in the body escapes from the block without performing any whenexit action.
*/
#define exitwhen do
/** Exit from an exitwhen to the given whenexit label. */
#define exitto(l) goto l
/** Introduce a section of code to execute on a particular exit state.
    There is no fallthrough from one whenexit to the next.
*/
#define whenexit(l) break; l:;


#endif