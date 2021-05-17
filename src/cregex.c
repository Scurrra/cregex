#include "cregex.h"
#include <stdlib.h> // NULL
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#define min(a, b) (a) < (b) ? a : b

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
    FIRST,
    LAST,
    REGULAR,
    NONE,
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
    bool nfa[MAX_PATTERN_LENGTH][MAX_PATTERN_LENGTH];
    int size;
} regex;

bool matchState(state *st, const char c);

re re_compile(const char *pattern)
{
    static regex reg;
    reg.states = (state *)malloc((MAX_PATTERN_LENGTH + 1) * sizeof(state));
    reg.states[0].type = FIRST; // flag for beginning

    unsigned int i = 0; // index in pattern
    unsigned int j = 1; // index in reg

    int lastGroupElements[MAX_GROUP_LENGTH], lastGroupElement = 0, lastGroupInsideBracket = 0;
    int groupLastElements[MAX_PATTERN_LENGTH], groupLastElement = 0;
    int groupFirstElements[MAX_PATTERN_LENGTH], groupFirstElement = 0;
    int lastOutput[MAX_PATTERN_LENGTH], lastOutputLength = 0;
    bool isVariation = false, variationBeforeGroup = false, neighbourVariation = false;

    for (size_t i = 0; i < MAX_GROUP_LENGTH; i++)
    {
        lastGroupElements[i] = -1;
    }
    for (size_t i = 0; i < MAX_PATTERN_LENGTH; i++)
    {
        groupLastElements[i] = -1;
    }

    while (pattern[i] != '\0' && (j + 1 < MAX_PATTERN_LENGTH))
    {
        // ranges are not allowed in []-scope
        switch (pattern[i])
        {
        case '^':
            if (pattern[i + 1] == '\0' || pattern[i + 1] == '|' || pattern[i + 1] == '(')
            {
                return 0; // this rules are not allowed
            }

            reg.states[j].type = NONE;
            ++i;
            continue; // save state
            break;
        case '.':
            if (lastGroupElement != -1)
            {
                lastGroupElements[lastGroupElement] = j;
                ++lastGroupElement;
            }

            // added && lastGroupElement == -1
            // first element in variation
            if (!isVariation && lastGroupElement == -1 && pattern[i + 1] == '|')
            {
                isVariation = true;
                groupFirstElements[groupFirstElement] = j;
                ++groupFirstElement;
            }
            // last element in variation
            if (isVariation && lastGroupElement == -1 && pattern[i + 1] != '|' && pattern[i + 1] != '*' && pattern[i + 1] != '+' && pattern[i + 1] != '?' && pattern[i + 1] != '{') //  && pattern[i - 1] == '|'
            {
                isVariation = false;
                groupLastElements[groupLastElement] = j;
                ++groupLastElement;
            }

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
                if (lastGroupElement != -1)
                {
                    lastGroupElements[lastGroupElement] = j;
                    ++lastGroupElement;
                }

                // added && lastGroupElement == -1
                // first element in variation
                if (!isVariation && lastGroupElement == -1 && pattern[i + 2] == '|')
                {
                    isVariation = true;
                    groupFirstElements[groupFirstElement] = j;
                    ++groupFirstElement;
                }
                // last element in variation
                if (isVariation && lastGroupElement == -1 && pattern[i + 2] != '|' && pattern[i + 2] != '*' && pattern[i + 2] != '+' && pattern[i + 2] != '?' && pattern[i + 2] != '{')
                {
                    isVariation = false;
                    groupLastElements[groupLastElement] = j;
                    ++groupLastElement;
                }

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
        {
            //bool flag = i != 0 && pattern[i - 1] == '|' ? true : false;

            if (lastGroupElement != -1)
            {
                lastGroupElements[lastGroupElement] = j;
                ++lastGroupElement;
            }

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

            // added && lastGroupElement == -1
            // first element in variation
            if (!isVariation && lastGroupElement == -1 && pattern[i + 1] == '|')
            {
                isVariation = true;
                groupFirstElements[groupFirstElement] = j;
                ++groupFirstElement;
            }
            // last element in variation //  && flag
            if (isVariation && lastGroupElement == -1 && pattern[i + 1] != '|' && pattern[i + 1] != '*' && pattern[i + 1] != '+' && pattern[i + 1] != '?' && pattern[i + 1] != '{')
            {
                isVariation = false;
                groupLastElements[groupLastElement] = j;
                ++groupLastElement;
            }

            reg.states[j].symbols[element].type = LAST;
            reg.states[j].min = 1;
            reg.states[j].max = 1;

            break;
        }
        // loops
        case '+': // 1 .. inf
            if (i == 0 || (i != 0 && pattern[i - 1] == '|'))
            {
                return 0;
            }

            --j;
            // added && lastGroupElement == -1
            // first element in variation
            if (!isVariation && lastGroupElement == -1 && pattern[i + 1] == '|')
            {
                isVariation = true;
                groupFirstElements[groupFirstElement] = j;
                ++groupFirstElement;
            }
            // last element in variation
            if (isVariation && lastGroupElement == -1 && pattern[i + 1] != '|') //  && pattern[i - 1] == '|'
            {
                isVariation = false;
                groupLastElements[groupLastElement] = j;
                ++groupLastElement;
            }

            reg.states[j].min = 1;
            reg.states[j].max = 0x3f3f; // infinity
            break;
        case '*': // 0 .. inf
            if (i == 0 || (i != 0 && pattern[i - 1] == '|'))
            {
                return 0;
            }

            --j;
            // added && lastGroupElement == -1
            // first element in variation
            if (!isVariation && lastGroupElement == -1 && pattern[i + 1] == '|')
            {
                isVariation = true;
                groupFirstElements[groupFirstElement] = j;
                ++groupFirstElement;
            }
            // last element in variation
            if (isVariation && lastGroupElement == -1 && pattern[i + 1] != '|') //  && pattern[i - 1] == '|'
            {
                isVariation = false;
                groupLastElements[groupLastElement] = j;
                ++groupLastElement;
            }

            reg.states[j].min = 0;
            reg.states[j].max = 0x3f3f; // infinity
            break;
        case '?': // 0 .. 1
            if (i == 0 || (i != 0 && pattern[i - 1] == '|'))
            {
                return 0;
            }

            --j;
            // added && lastGroupElement == -1
            // first element in variation
            if (!isVariation && lastGroupElement == -1 && pattern[i + 1] == '|')
            {
                isVariation = true;
                groupFirstElements[groupFirstElement] = j;
                ++groupFirstElement;
            }
            // last element in variation
            if (isVariation && lastGroupElement == -1 && pattern[i + 1] != '|') //  && pattern[i - 1] == '|'
            {
                isVariation = false;
                groupLastElements[groupLastElement] = j;
                ++groupLastElement;
            }

            reg.states[j].min = 0;
            reg.states[j].max = 1;
            break;
        case '{':
        {
            if (i == 0 || (i != 0 && pattern[i - 1] == '|'))
            {
                return 0;
            }

            int n = 0;
            int m;

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
            while (pattern[i] == ' ')
            {
                ++i;
            }
            if (pattern[i] == ',')
            {
                m = 0x3f3f;
                ++i;
            }
            else
            {
                m = n;
            }
            while (pattern[i] == ' ')
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
            while (pattern[i] != '}')
            {
                ++i;
            }

            --j;
            // added && lastGroupElement == -1
            // first element in variation
            if (!isVariation && lastGroupElement == -1 && pattern[i + 1] == '|')
            {
                isVariation = true;
                groupFirstElements[groupFirstElement] = j;
                ++groupFirstElement;
            }
            // last element in variation
            if (isVariation && lastGroupElement == -1 && pattern[i + 1] != '|') //  && pattern[i - 1] == '|'
            {
                isVariation = false;
                groupLastElements[groupLastElement] = j;
                ++groupLastElement;
            }
            reg.states[j].min = n;
            reg.states[j].max = m;
        }
        break;

        // group
        case '(':
            if (lastGroupInsideBracket != 0) // inside a group
            {
                if (!reg.states[j].type)
                {
                    reg.states[j].type = REGULAR;
                }
                reg.states[j].symbols[0].value.element = pattern[i];
                reg.states[j].symbols[0].type = SYMBOL;
                reg.states[j].symbols[1].type = LAST;
                reg.states[j].min = 1;
                reg.states[j].max = 1;

                ++lastGroupInsideBracket;

                lastGroupElements[lastGroupElement] = j;
                ++lastGroupElement;
            }
            else // open a group
            {
                variationBeforeGroup = i != 0 && pattern[i - 1] == '|' ? true : false;

                lastGroupElement = 0;
                ++lastGroupInsideBracket; // open bracket
                ++i;
                continue;
            }
            break;
        case ')':
            if (lastGroupInsideBracket != 1) // doesn't close the group: if equals to 0 it's outside, otherwise inside
            {
                if (!reg.states[j].type)
                {
                    reg.states[j].type = REGULAR;
                }
                reg.states[j].symbols[0].value.element = pattern[i];
                reg.states[j].symbols[0].type = SYMBOL;
                reg.states[j].symbols[1].type = LAST;
                reg.states[j].min = 1;
                reg.states[j].max = 1;

                if (lastGroupInsideBracket != 0)
                {
                    --lastGroupInsideBracket;

                    lastGroupElements[lastGroupElement] = j;
                    ++lastGroupElement;
                }
            }
            else // close a group
            {
                // loop for group
                switch (pattern[i + 1])
                {
                case '+': // 1 .. inf
                    ++i;
                    for (size_t k = 0; k < lastGroupElement; k++)
                    {
                        reg.states[lastGroupElements[k]].min = 1;
                        reg.states[lastGroupElements[k]].max = 0x3f3f;
                    }
                    break;
                case '*': // 0 .. inf
                    ++i;
                    for (size_t k = 0; k < lastGroupElement; k++)
                    {
                        reg.states[lastGroupElements[k]].min = 0;
                        reg.states[lastGroupElements[k]].max = 0x3f3f;
                    }
                    break;
                case '?': // 0 .. 1
                    ++i;
                    for (size_t k = 0; k < lastGroupElement; k++)
                    {
                        reg.states[lastGroupElements[k]].min = 0;
                        reg.states[lastGroupElements[k]].max = 1;
                    }
                    break;
                case '{':
                {
                    int n = 0;
                    int m;

                    ++i;
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
                    while (pattern[i] == ' ')
                    {
                        ++i;
                    }
                    if (pattern[i] == ',')
                    {
                        m = 0x3f3f;
                        ++i;
                    }
                    else
                    {
                        m = n;
                    }
                    while (pattern[i] == ' ')
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
                    while (pattern[i] != '}')
                    {
                        ++i;
                    }
                    //++i;

                    for (size_t k = 0; k < lastGroupElement; k++)
                    {
                        reg.states[lastGroupElements[k]].min *= n;
                        reg.states[lastGroupElements[k]].max = reg.states[lastGroupElements[k]].max == 0x3f3f ? reg.states[lastGroupElements[k]].max : reg.states[lastGroupElements[k]].max * m;
                    }
                }
                break;

                default:
                    break;
                }

                // first element in variation
                if (!isVariation && pattern[i + 1] == '|')
                {
                    isVariation = true;
                    groupFirstElements[groupFirstElement] = lastGroupElements[0];
                    ++groupFirstElement;
                }
                // last element in variation
                if (isVariation && pattern[i + 1] != '|' && variationBeforeGroup)
                {
                    isVariation = false;
                    groupLastElements[groupLastElement] = lastGroupElements[lastGroupElement - 1];
                    ++groupLastElement;
                }

                --lastGroupInsideBracket;

                // group straight case
                for (size_t k = 1; k < lastGroupElement; k++)
                {
                    reg.nfa[lastGroupElements[k - 1]][lastGroupElements[k]] = 1;
                }
                neighbourVariation = neighbourVariation && lastGroupElement != -1;

                // case, when group is the last variable in variation
                if (groupLastElement != 0 && !isVariation)
                {
                    for (size_t k = 0; k < groupLastElement; k++)
                    {
                        reg.nfa[groupLastElements[k]][groupLastElements[groupLastElement - 1] + 1] = 1; // column

                        if (!neighbourVariation)
                        {
                            reg.nfa[groupFirstElements[0] - 1][groupFirstElements[k]] = 1; // row
                        }
                        else
                        {
                            for (size_t l = 0; l < lastOutputLength; l++)
                            {
                                reg.nfa[lastOutput[l]][groupFirstElements[k]] = 1;
                            }
                        }
                    }

                    for (size_t k = 0; k < groupLastElement; k++)
                    {
                        lastOutput[k] = groupLastElements[k];
                        neighbourVariation = true;
                    }
                    lastOutputLength = groupLastElement != 0 ? groupLastElement : 0;

                    groupFirstElement = 0;
                    groupLastElement = 0;
                }

                ++i;
                lastGroupElement = -1;
                continue;
            }
            break;

        // variation
        case '|':
            if (lastGroupInsideBracket == 0) // only outside a group
            {
                // except of the last element
                if (i != 0) // both in case of element and group
                {
                    groupLastElements[groupLastElement] = j - 1;
                    ++groupLastElement;
                    groupFirstElements[groupFirstElement] = j;
                    ++groupFirstElement;
                    --j;
                }

                if (pattern[i + 1] == '\0')
                {
                    printf("'|' is not allowed at the end of re!\n");
                    return 0;
                }
            }

            break;

        default:
            if (lastGroupElement != -1)
            {
                lastGroupElements[lastGroupElement] = j;
                ++lastGroupElement;
            }

            // added && lastGroupElement == -1
            // first element in variation
            if (!isVariation && lastGroupElement == -1 && pattern[i + 1] == '|')
            {
                isVariation = true;
                groupFirstElements[groupFirstElement] = j;
                ++groupFirstElement;
            }
            // last element in variation
            if (isVariation && lastGroupElement == -1 && pattern[i + 1] != '|' && pattern[i + 1] != '*' && pattern[i + 1] != '+' && pattern[i + 1] != '?' && pattern[i + 1] != '{') //  && pattern[i - 1] == '|'
            {
                isVariation = false;
                groupLastElements[groupLastElement] = j;
                ++groupLastElement;
            }

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

        // case when group is not the last element in variation?
        if (groupLastElement != 0 && !isVariation)
        {
            for (size_t k = 0; k < groupLastElement; k++)
            {
                reg.nfa[groupLastElements[k]][groupLastElements[groupLastElement - 1] + 1] = 1; // column

                if (!neighbourVariation)
                {
                    reg.nfa[groupFirstElements[0] - 1][groupFirstElements[k]] = 1; // row
                }
                else
                {
                    for (size_t l = 0; l < lastOutputLength; l++)
                    {
                        reg.nfa[lastOutput[l]][groupFirstElements[k]] = 1;
                    }
                }
            }

            for (size_t k = 0; k < groupLastElement; k++)
            {
                lastOutput[k] = groupLastElements[k];
                neighbourVariation = true;
            }
            lastOutputLength = groupLastElement != 0 ? groupLastElement : 0;

            groupFirstElement = 0;
            groupLastElement = 0;
        }
        else if (!isVariation && groupFirstElement == 0) // non-group straight case
        {
            reg.nfa[j - 1][j] = 1;
        }
        neighbourVariation = neighbourVariation && lastGroupElement != -1;

        ++i;
        ++j;
    }

    reg.size = j - 1;

    reg.states[j].type = LAST;

    return (re)&reg;
}

void re_print(re *pattern)
{
    // print nfa
    printf("\tNFA:\n");
    for (size_t i = 0; i < (*pattern)->size + 1; i++)
    {
        printf("%ld:\t", i);
        for (size_t k = 0; k < (*pattern)->size + 1; k++)
        {
            printf("%d ", (*pattern)->nfa[i][k]);
        }
        printf("\n");
    }

    const char *types[] = {
        "FIRST",
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

    int i = 1;
    printf("\n\tStates:\n");
    while ((*pattern)->states[i].type != LAST)
    {
        printf("%d\n\ttype: %s\n", i, types[(*pattern)->states[i].type]);
        printf("\tElements in class:\n");

        int j = 0;
        while ((*pattern)->states[i].symbols[j].type != LAST)
        {
            printf("\ti: %d\n\t\ttype: %s\n", j, types[(*pattern)->states[i].symbols[j].type]);
            if ((*pattern)->states[i].symbols[j].type == SYMBOL || (*pattern)->states[i].symbols[j].type == DOT || (*pattern)->states[i].symbols[j].type == SPACE || (*pattern)->states[i].symbols[j].type == NONSPACE || (*pattern)->states[i].symbols[j].type == NUMERIC || (*pattern)->states[i].symbols[j].type == NONNUMERIC || (*pattern)->states[i].symbols[j].type == ALPHANUMERIC || (*pattern)->states[i].symbols[j].type == NONALPHANUMERIC)
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

bool re_match(re *pattern, const char *string)
{
    int i = 0;            // index in string
    int j = 0, prevj = 0; // row in nfa

    bool matches = true;
    while (matches && j < (*pattern)->size && i < strlen(string))
    {
        for (int k = j + 1; k < (*pattern)->size + 1; k++)
        {
            if ((*pattern)->nfa[j][k])
            {
                int s = 0;
                // minimal number of state entries
                if ((*pattern)->states[k].min > 0)
                {
                    while (s < (*pattern)->states[k].min && matchState(&(*pattern)->states[k], string[i]))
                    {
                        ++s;
                        ++i;
                    }
                }
                if (s < (*pattern)->states[k].min)
                {
                    continue;
                }

                while (s < (*pattern)->states[k].max && string[i] != '\0' && matchState(&(*pattern)->states[k], string[i])) // && i < strlen(string)
                {
                    ++s;
                    ++i;
                }

                if (s != 0)
                {
                    prevj = j;
                    j = k;
                    break;
                }
            }

            if (k == (*pattern)->size)
            {
                return false;
            }
        }

        if (prevj == j)
        {
            return false;
        }
    }

    return i == strlen(string) && j == (*pattern)->size;
}
bool re_matchp(const char *pattern, const char *string)
{
    re p = re_compile(pattern);
    // re_print(&p);

    return re_match(&p, string);
}

int re_find(re *pattern, const char *string)
{
}
int re_findp(const char *pattern, const char *string)
{
    re p = re_compile(pattern);
    return re_find(&p, string);
}

bool matchState(state *st, const char c)
{
    bool matches = false;

    int i = 0;
    while (!matches && st->symbols[i].type != LAST)
    {

        if (st->symbols[i].type == DOT)
        {
            matches = isascii(c);
        }
        else if (st->symbols[i].type == SYMBOL)
        {
            matches = c == (*st).symbols[i].value.element;
        }
        else if (st->symbols[i].type == RANGE)
        {
            matches = (*st).symbols[i].value.rng.start <= c && (*st).symbols[i].value.rng.finish >= c;
        }
        else
        {
            switch ((*st).symbols[i].type)
            {
            case NUMERIC:
                matches = isdigit(c);
                break;
            case NONNUMERIC:
                matches = !isdigit(c);
                break;
            case SPACE:
                matches = isspace(c);
                break;
            case NONSPACE:
                matches = !isspace(c);
                break;
            case ALPHANUMERIC:
                matches = isalnum(c);
                break;
            case NONALPHANUMERIC:
                matches = !isalnum(c);
                break;

            default:
                break;
            }
        }

        ++i;
    }

    return ((*st).type == REGULAR) == matches;
}
