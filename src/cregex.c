#include "cregex.h"
#include <stdlib.h> // NULL
#include <stdio.h>
#include <ctype.h>

/*
Represents a range on the alphabet

start - first position in alphabet
finish - last position in alphabet

In normal case start < finish, in case of '^' finish < start.
*/
typedef struct range
{
    unsigned short start;
    unsigned short finish;
} range;

enum
{
    LAST,
    REGULAR,
    NONE,
    //    CLASS,
    RANGE,
    SYMBOL,
    DOT,
    NUMERIC,        // \d
    NONNUMERIC,     // \D
    SPACE,          // \s
    NONSPACE,       // \S
    ALPHANUMERIC,   // \w
    NONALPHANUMERIC // \W
};

/*
Part of the state

Can be either single element (including special symbol), class or range
*/
typedef struct symbol
{
    unsigned char type;
    union
    {
        unsigned char element;
        //unsigned char *elements; // CLASS
        range rng; // RANGE
    } value;
} symbol;

/*
State of the automata

symbols - nonzero array of symbols in state
min - minimal number of symbol repetitions
max - maximal number of symbol repetitions
*/
typedef struct state
{
    unsigned char type; //  REGULAR or NONE (NONE  in case of '^' prefix)
    symbol symbols[MAX_CLASS_LENGTH + 1];
    unsigned short min; // minimal number of elements in state
    unsigned short max; // maximal number of elements in state
} state;

typedef struct regex
{
    state *states;
    //int **nfa;
    //int **dfa;
} regex;

re re_compile(const char *pattern)
{
    static regex reg;
    reg.states = (state *)malloc(MAX_PATTERN_LENGTH * sizeof(state));

    unsigned int i = 0; // index in pattern
    unsigned int j = 0; // index in reg

    while (pattern[i] != '\0' && (j + 1 < MAX_PATTERN_LENGTH))
    {
        char c = pattern[i];
        // ranges are not allowed in []-scope
        switch (c)
        {
        case '^':
            reg.states[j].type = NONE;
            ++i;
            continue; // save state
            break;
        case '.':
            if (!reg.states[j].type) // "^."
            {
                reg.states[j].type = REGULAR;
            }

            reg.states[j].symbols[0].value.element = pattern[i];
            reg.states[j].symbols[0].type = DOT;
            reg.states[j].symbols[1].type = LAST;
            reg.states[j].min = 1;
            reg.states[j].max = 1;
            break;
        case '\\':
            if (pattern[i + 1] != '\0')
            {
                ++i;
                if (!reg.states[j].type)
                {
                    reg.states[j].type = REGULAR;
                }
                reg.states[j].symbols[0].value.element = pattern[i];

                switch (pattern[i])
                {
                case 'd':
                    reg.states[j].symbols[0].type = NUMERIC;
                    break;
                case 'D':
                    reg.states[j].symbols[0].type = NONNUMERIC;
                    break;
                case 's':
                    reg.states[j].symbols[0].type = SPACE;
                    break;
                case 'S':
                    reg.states[j].symbols[0].type = NONSPACE;
                    break;
                case 'w':
                    reg.states[j].symbols[0].type = ALPHANUMERIC;
                    break;
                case 'W':
                    reg.states[j].symbols[0].type = NONALPHANUMERIC;
                    break;

                default:
                    break;
                }
                reg.states[j].symbols[1].type = LAST;
                reg.states[j].min = 1;
                reg.states[j].max = 1;
            }
            break;
        case '[':
            if (!reg.states[j].type)
            {
                reg.states[j].type = REGULAR;
            }
            int element = 0; // index in reg.states[j].symbols

            ++i;
            while (pattern[i] != ']' && pattern[i] != '\0')
            {
                // '.' and '^' doesn`t work`s as DOT in []-scope
                // only ranges, elements and specials

                // range
                if (pattern[i + 1] == '-')
                {
                    reg.states[j].symbols[element].value.rng.start = (int)pattern[i];
                    reg.states[j].symbols[element].value.rng.finish = (int)pattern[i + 2];
                    reg.states[j].symbols[element].type = RANGE;

                    i += 3;
                    ++element;
                    continue;
                }
                // elements and specials
                switch (pattern[i])
                {
                case '\\':
                    if (pattern[i])
                    {
                        ++i;
                        reg.states[j].symbols[element].value.element = pattern[i];
                        switch (pattern[i])
                        {
                        case 'd':
                            reg.states[j].symbols[element].type = NUMERIC;
                            break;
                        case 'D':
                            reg.states[j].symbols[element].type = NONNUMERIC;
                            break;
                        case 's':
                            reg.states[j].symbols[element].type = SPACE;
                            break;
                        case 'S':
                            reg.states[j].symbols[element].type = NONSPACE;
                            break;
                        case 'w':
                            reg.states[j].symbols[element].type = ALPHANUMERIC;
                            break;
                        case 'W':
                            reg.states[j].symbols[element].type = NONALPHANUMERIC;
                            break;

                        default:
                            reg.states[j].symbols[element].type = SYMBOL; // '-' stands for range, so you should to write '\-'
                            break;
                        }
                    }
                    break;

                default:
                    reg.states[j].symbols[element].value.element = pattern[i];
                    reg.states[j].symbols[element].type = SYMBOL;

                    break;
                }

                ++element;
                ++i;
            }
            reg.states[j].symbols[element].type = LAST;
            reg.states[j].min = 1;
            reg.states[j].max = 1;
            break;

            // loops
        case '+': // 1 .. inf
            reg.states[j - 1].min = 1;
            reg.states[j - 1].max = 0x3f3f; // infinity
            --j;
            break;
        case '*': // 0 .. inf
            reg.states[j - 1].min = 0;
            reg.states[j - 1].max = 0x3f3f; // infinity
            --j;
            break;
        case '?': // 0 .. 1
            reg.states[j - 1].min = 0;
            reg.states[j - 1].max = 1;
            --j;
            break;
        case '{':
        {
            int n = 0;
            int m = 0x3f3f;

            ++i;
            while (pattern[i] == ' ')
            {
                ++i;
            }
            while (isdigit(pattern[i]))
            {
                n = n * 10 + (pattern[i] - '0');
                ++i;
            }
            while (pattern[i] == ' ' || pattern[i] == ',')
            {
                ++i;
            }
            if (pattern[i] != '}')
            {
                m = 0;
                while (isdigit(pattern[i]))
                {
                    m = m * 10 + (pattern[i] - '0');
                    ++i;
                }
            }
            while (pattern[i] == '}')
            {
                ++i;
            }
            ++i;

            reg.states[j - 1].min = n;
            reg.states[j - 1].max = m;
            --j;
        }
        break;

        default:
            if (!reg.states[j].type)
            {
                reg.states[j].type = REGULAR;
            }
            reg.states[j].symbols[0].value.element = pattern[i];
            reg.states[j].symbols[0].type = SYMBOL;
            reg.states[j].symbols[1].type = LAST;
            reg.states[j].min = 1;
            reg.states[j].max = 1;
            break;
        }
        ++i;
        ++j;
    }
    reg.states[j].type = LAST;

    return (re)&reg;
}

void re_print(re *pattern)
{
    const char *types[] = {
        "LAST",
        "REGULAR",
        "NONE",
        "RANGE",
        "SYMBOL",
        "DOT",
        "NUMERIC",        // \d
        "NONNUMERIC",     // \D
        "SPACE",          // \s
        "NONSPACE",       // \S
        "ALPHANUMERIC",   // \w
        "NONALPHANUMERIC" // \W
    };

    int i = 0;
    while ((*pattern)->states[i].type != LAST)
    {
        printf("%d\n\ttype: %s\n", i, types[(*pattern)->states[i].type]);

        int j = 0;
        while ((*pattern)->states[i].symbols[j].type != LAST)
        {
            printf("\t%d\n\t\ttype: %s\n", j, types[(*pattern)->states[i].symbols[j].type]);
            if ((*pattern)->states[i].symbols[j].type == SYMBOL)
            {
                printf("\t\tvalue: %c\n", (*pattern)->states[i].symbols[j].value.element);
            }
            else //if ((*pattern)->states[i].symbols[j].type == RANGE)
            {
                printf("\t\tstart: %c\n", (*pattern)->states[i].symbols[j].value.rng.start);
                printf("\t\tfinish: %c\n", (*pattern)->states[i].symbols[j].value.rng.finish);
            }

            ++j;
        }
        printf("\tmin: %d\n", (*pattern)->states[i].min);
        printf("\tmax: %d\n", (*pattern)->states[i].max);
        ++i;
    }
}