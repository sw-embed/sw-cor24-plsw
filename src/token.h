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
#define TOK_COLON     78

#define TOK_EOF       99

/* Token structure */
#define TOK_TEXT_MAX  64

struct token {
    int type;
    int ival;
    char text[TOK_TEXT_MAX];
};

/* Keyword table -- parallel arrays (avoids tc24r struct member confusion) */
#define KW_COUNT 27

char *kw_names[KW_COUNT];
int   kw_types[KW_COUNT];

void kw_init(void) {
    kw_names[0]  = "DCL";       kw_types[0]  = TOK_DCL;
    kw_names[1]  = "DECLARE";   kw_types[1]  = TOK_DECLARE;
    kw_names[2]  = "PROC";      kw_types[2]  = TOK_PROC;
    kw_names[3]  = "IF";        kw_types[3]  = TOK_IF;
    kw_names[4]  = "THEN";      kw_types[4]  = TOK_THEN;
    kw_names[5]  = "ELSE";      kw_types[5]  = TOK_ELSE;
    kw_names[6]  = "DO";        kw_types[6]  = TOK_DO;
    kw_names[7]  = "WHILE";     kw_types[7]  = TOK_WHILE;
    kw_names[8]  = "END";       kw_types[8]  = TOK_END;
    kw_names[9]  = "CALL";      kw_types[9]  = TOK_CALL;
    kw_names[10] = "RETURN";    kw_types[10] = TOK_RETURN;
    kw_names[11] = "ASM";       kw_types[11] = TOK_ASM;
    kw_names[12] = "STATIC";    kw_types[12] = TOK_STATIC;
    kw_names[13] = "AUTOMATIC"; kw_types[13] = TOK_AUTO;
    kw_names[14] = "EXTERNAL";  kw_types[14] = TOK_EXTERNAL;
    kw_names[15] = "INT";       kw_types[15] = TOK_INT;
    kw_names[16] = "BYTE";      kw_types[16] = TOK_BYTE;
    kw_names[17] = "CHAR";      kw_types[17] = TOK_CHAR;
    kw_names[18] = "PTR";       kw_types[18] = TOK_PTR;
    kw_names[19] = "WORD";      kw_types[19] = TOK_WORD;
    kw_names[20] = "BIT";       kw_types[20] = TOK_BIT;
    kw_names[21] = "OPTIONS";   kw_types[21] = TOK_OPTIONS;
    kw_names[22] = "RETURNS";   kw_types[22] = TOK_RETURNS;
    kw_names[23] = "TO";        kw_types[23] = TOK_TO;
    kw_names[24] = "ADDR";      kw_types[24] = TOK_ADDR;
    kw_names[25] = "BASED";     kw_types[25] = TOK_BASED;
    kw_names[26] = "NAKED";     kw_types[26] = TOK_NAKED;
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
        if (str_eq_nocase(name, kw_names[i])) {
            return kw_types[i];
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
    if (type == TOK_COLON) return ":";
    if (type == TOK_EOF) return "EOF";
    return "???";
}

#endif
