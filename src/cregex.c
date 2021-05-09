#include "cregex.h"
#include <stdlib.h> // NULL

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
    symbol *symbols;
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
            if (reg.states[j].type == NULL) // "^."
            {
                reg.states[j].type = REGULAR;
            }

            reg.states[j].symbols = (symbol *)malloc(sizeof(symbol));
            reg.states[j].symbols[0].value.element = pattern[i];
            reg.states[j].symbols[0].type = DOT;
            break;
        case '\\':
            if (pattern[i + 1] != '\0')
            {
                ++i;
                if (reg.states[j].type == NULL)
                {
                    reg.states[j].type = REGULAR;
                }
                reg.states[j].symbols = (symbol *)malloc(sizeof(symbol));
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
            }
            break;
        case '[':
            if (reg.states[j].type == NULL)
            {
                reg.states[j].type = REGULAR;
            }
            reg.states[j].symbols = (symbol *)malloc(MAX_CLASS_LENGTH * sizeof(symbol));
            int element = 0; // index in reg.states[j].symbols

            ++i;
            while (pattern[i] != 0 && pattern[i] != '\0')
            {
                // '.' and '^' doesn`t work`s as DOT in []-scope
                // only ranges, elements and specials

                if (pattern[i + 1] == '-')
                {
                    reg.states[j].symbols[element].value.rng.start = (int)pattern[i];
                    reg.states[j].symbols[element].value.rng.finish = (int)pattern[i + 2];
                    reg.states[j].symbols[element].type = RANGE;

                    i += 3;
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
                            reg.states[j].symbols[element].type = SYMBOL; // '-' stands for range, so tou should to write '\-'
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
            break;

        default:
            if (reg.states[j].type == NULL)
            {
                reg.states[j].type = REGULAR;
            }
            reg.states[j].symbols = (symbol *)malloc(sizeof(symbol));
            reg.states[j].symbols[0].value.element = pattern[i];
            reg.states[j].symbols[0].type = SYMBOL;
            break;
        }

        ++i;
        ++j;
    }
    reg.states[j].type = LAST;

    return *(re)reg;
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
        i++;
    }
}