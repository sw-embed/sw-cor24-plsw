/* token.h -- Token types and keyword table for PL/SW lexer */

#ifndef TOKEN_H
#define TOKEN_H

#include "str.h"

/* Token type codes */
#define TOK_DCL       1
#define TOK_DECLARE   2
#define TOK_PROC      3
#define TOK_IF        4
#define TOK_THEN      5
#define TOK_ELSE      6
#define TOK_DO        7
#define TOK_WHILE     8
#define TOK_END       9
#define TOK_CALL      10
#define TOK_RETURN    11
#define TOK_ASM       12
#define TOK_STATIC    13
#define TOK_AUTO      14
#define TOK_EXTERNAL  15
#define TOK_INT       16
#define TOK_BYTE      17
#define TOK_CHAR      18
#define TOK_PTR       19
#define TOK_WORD      20
#define TOK_BIT       21
#define TOK_OPTIONS   22
#define TOK_RETURNS   23
#define TOK_TO        24
#define TOK_ADDR      25
#define TOK_BASED     26
#define TOK_NAKED     27

#define TOK_IDENT     40
#define TOK_NUM       41
#define TOK_STRING    42

#define TOK_PLUS      50
#define TOK_MINUS     51
#define TOK_STAR      52
#define TOK_SLASH     53
#define TOK_AMP       54
#define TOK_PIPE      55
#define TOK_CARET     56
#define TOK_TILDE     57
#define TOK_EQ        58
#define TOK_NE        59
#define TOK_LT        60
#define TOK_GT        61
#define TOK_LE        62
#define TOK_GE        63
#define TOK_SHL       64
#define TOK_SHR       65
#define TOK_ARROW     66

#define TOK_LPAREN    70
#define TOK_RPAREN    71
#define TOK_COMMA     72
#define TOK_SEMI      73
#define TOK_DOT       74
#define TOK_ASSIGN    75
#define TOK_QUESTION  76
#define TOK_PERCENT   77

#define TOK_EOF       99

/* Token structure */
#define TOK_TEXT_MAX  64

struct token {
    int type;
    int ival;
    char text[TOK_TEXT_MAX];
};

/* Keyword table entry */
struct keyword {
    char *name;
    int type;
};

/* Keyword table -- searched linearly, case-insensitive */
#define KW_COUNT 27

struct keyword kw_table[KW_COUNT];

void kw_init(void) {
    kw_table[0].name = "DCL";       kw_table[0].type = TOK_DCL;
    kw_table[1].name = "DECLARE";   kw_table[1].type = TOK_DECLARE;
    kw_table[2].name = "PROC";      kw_table[2].type = TOK_PROC;
    kw_table[3].name = "IF";        kw_table[3].type = TOK_IF;
    kw_table[4].name = "THEN";      kw_table[4].type = TOK_THEN;
    kw_table[5].name = "ELSE";      kw_table[5].type = TOK_ELSE;
    kw_table[6].name = "DO";        kw_table[6].type = TOK_DO;
    kw_table[7].name = "WHILE";     kw_table[7].type = TOK_WHILE;
    kw_table[8].name = "END";       kw_table[8].type = TOK_END;
    kw_table[9].name = "CALL";      kw_table[9].type = TOK_CALL;
    kw_table[10].name = "RETURN";   kw_table[10].type = TOK_RETURN;
    kw_table[11].name = "ASM";      kw_table[11].type = TOK_ASM;
    kw_table[12].name = "STATIC";   kw_table[12].type = TOK_STATIC;
    kw_table[13].name = "AUTOMATIC"; kw_table[13].type = TOK_AUTO;
    kw_table[14].name = "EXTERNAL"; kw_table[14].type = TOK_EXTERNAL;
    kw_table[15].name = "INT";      kw_table[15].type = TOK_INT;
    kw_table[16].name = "BYTE";     kw_table[16].type = TOK_BYTE;
    kw_table[17].name = "CHAR";     kw_table[17].type = TOK_CHAR;
    kw_table[18].name = "PTR";      kw_table[18].type = TOK_PTR;
    kw_table[19].name = "WORD";     kw_table[19].type = TOK_WORD;
    kw_table[20].name = "BIT";      kw_table[20].type = TOK_BIT;
    kw_table[21].name = "OPTIONS";  kw_table[21].type = TOK_OPTIONS;
    kw_table[22].name = "RETURNS";  kw_table[22].type = TOK_RETURNS;
    kw_table[23].name = "TO";       kw_table[23].type = TOK_TO;
    kw_table[24].name = "ADDR";     kw_table[24].type = TOK_ADDR;
    kw_table[25].name = "BASED";    kw_table[25].type = TOK_BASED;
    kw_table[26].name = "NAKED";    kw_table[26].type = TOK_NAKED;
}

/* Convert a character to uppercase */
int to_upper(int c) {
    if (c >= 97 && c <= 122) {
        return c - 32;
    }
    return c;
}

/* Case-insensitive string comparison */
int str_eq_nocase(char *a, char *b) {
    while (*a && *b) {
        if (to_upper(*a) != to_upper(*b)) {
            return 0;
        }
        a = a + 1;
        b = b + 1;
    }
    return *a == *b;
}

/* Look up an identifier in the keyword table.
   Returns the keyword token type, or TOK_IDENT if not a keyword. */
int kw_lookup(char *name) {
    int i = 0;
    while (i < KW_COUNT) {
        if (str_eq_nocase(name, kw_table[i].name)) {
            return kw_table[i].type;
        }
        i = i + 1;
    }
    return TOK_IDENT;
}

/* Return a human-readable name for a token type */
char *tok_name(int type) {
    if (type == TOK_DCL) return "DCL";
    if (type == TOK_DECLARE) return "DECLARE";
    if (type == TOK_PROC) return "PROC";
    if (type == TOK_IF) return "IF";
    if (type == TOK_THEN) return "THEN";
    if (type == TOK_ELSE) return "ELSE";
    if (type == TOK_DO) return "DO";
    if (type == TOK_WHILE) return "WHILE";
    if (type == TOK_END) return "END";
    if (type == TOK_CALL) return "CALL";
    if (type == TOK_RETURN) return "RETURN";
    if (type == TOK_ASM) return "ASM";
    if (type == TOK_STATIC) return "STATIC";
    if (type == TOK_AUTO) return "AUTOMATIC";
    if (type == TOK_EXTERNAL) return "EXTERNAL";
    if (type == TOK_INT) return "INT";
    if (type == TOK_BYTE) return "BYTE";
    if (type == TOK_CHAR) return "CHAR";
    if (type == TOK_PTR) return "PTR";
    if (type == TOK_WORD) return "WORD";
    if (type == TOK_BIT) return "BIT";
    if (type == TOK_OPTIONS) return "OPTIONS";
    if (type == TOK_RETURNS) return "RETURNS";
    if (type == TOK_TO) return "TO";
    if (type == TOK_ADDR) return "ADDR";
    if (type == TOK_BASED) return "BASED";
    if (type == TOK_NAKED) return "NAKED";
    if (type == TOK_IDENT) return "IDENT";
    if (type == TOK_NUM) return "NUM";
    if (type == TOK_STRING) return "STRING";
    if (type == TOK_PLUS) return "+";
    if (type == TOK_MINUS) return "-";
    if (type == TOK_STAR) return "*";
    if (type == TOK_SLASH) return "/";
    if (type == TOK_AMP) return "&";
    if (type == TOK_PIPE) return "|";
    if (type == TOK_CARET) return "^";
    if (type == TOK_TILDE) return "~";
    if (type == TOK_EQ) return "=";
    if (type == TOK_NE) return "!=";
    if (type == TOK_LT) return "<";
    if (type == TOK_GT) return ">";
    if (type == TOK_LE) return "<=";
    if (type == TOK_GE) return ">=";
    if (type == TOK_SHL) return "<<";
    if (type == TOK_SHR) return ">>";
    if (type == TOK_ARROW) return "->";
    if (type == TOK_LPAREN) return "(";
    if (type == TOK_RPAREN) return ")";
    if (type == TOK_COMMA) return ",";
    if (type == TOK_SEMI) return ";";
    if (type == TOK_DOT) return ".";
    if (type == TOK_ASSIGN) return "=";
    if (type == TOK_QUESTION) return "?";
    if (type == TOK_PERCENT) return "%";
    if (type == TOK_EOF) return "EOF";
    return "???";
}

#endif
