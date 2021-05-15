#ifndef C_REGEX
#define C_REGEX

#include <stdbool.h>
#include <locale.h>

/*
RULES:
. - any symbol
\d - numeric
\D - nonnumeric
\s - any space
\S - nonspace
\w - alphanumeric
\W - nonalphanumeric

[] - class
() - group, nested groups are not allowed

+ - 1..inf
* - 0..inf
? - 0..1
{n} - n
{n,} - n..inf
{n,m} - n..m
*/
typedef struct regex *re;

#define MAX_PATTERN_LENGTH 100 // maximum number of states in automata
#define MAX_CLASS_LENGTH 10    // maximum number of elements per class (except of range)
#define MAX_GROUP_LENGTH 10    // maximum number of elements in group ()

/*
Compiles the regular expression

pattern - the regular expression, that corresponds to defined rules
*/
re re_compile(const char *pattern);

void re_print(re *pattern);

/*
Check if input string fully matches the regular expression

string - string to be checked
pattern - compiled regular expression
*/
bool re_match(re *pattern, const char *string);

/*
Check if input string fully matches the regular expression

string - string to be checked
pattern - the regular expression, that corresponds to defined rules
*/
bool re_matchp(const char *pattern, const char *string);

/*
Finds substring in string that corresponds to the regular expression

string - string to be processed
pattern - compiled regular expression
*/
int re_find(re *pattern, const char *string);

/*
Finds substring in string that corresponds to the regular expression

string - string to be processed
pattern - the regular expression, that corresponds to defined rules
*/
int re_findp(const char *pattern, const char *string);

#endif