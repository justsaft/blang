
extern "C" {
#include "../3rd-party/nob.h"
#include "../3rd-party/stb_c_lexer.h"
}

#include "types.hpp"

void unexpected_eof(const char* last_parsed)
{
    nob_log(NOB_ERROR, "Encountered unexpected EOF while last working on >%s<", last_parsed);
    static_assert("Unexpected end of file.");
}

void step_lexer(stb_lexer& l)
{
    if (!stb_c_lexer_get_token(&l))
    {
        unexpected_eof("step_lexer");
    }
}

void get_lexer_location(stb_lexer& l, stb_lex_location& lo)
{
    stb_c_lexer_get_location(&l, l.where_firstchar, &lo);
    lo.line_offset += 1;
}

long get_next_token(stb_lexer& l)
{
    step_lexer(l);
    return l.token;
}

bool get_and_expect_token(stb_lexer& l, long token)
{
    if (get_next_token(l) == token)
    {
        return true;
    }
    return false;
}

bool get_and_expect_semicolon(stb_lexer& l, const char* filename, const bool advance_lexer)
{
    stb_lex_location pos;
    get_lexer_location(l, pos);

    long token = l.token;

    /* if (!stb_c_lexer_get_token(&l))
    {
        nob_log(NOB_ERROR, "Unexpected EOF: expected to see semicolon.");
        compilation_error(ExpectedSemicolon);
        return false; // Fail
    } */

    switch (token)
    {
    case ';':
        if (advance_lexer)
            step_lexer(l);
        return true;

    default:
        //NOB_TODO("Unwind after missing semicolon");
        nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: expected semicolon.", filename, pos.line_number, pos.line_offset);
        return false; // Didn't get a semicolon
    }
}

bool expect_token(stb_lexer& l, long token/* , bool silent */)
{
    if (l.token == token)
    {
        return 1;
    }
    /* else if (!silent) {
        stb_lex_location lo;
        get_lexer_location(l, lo);

        nob_log(NOB_ERROR, "%s:%d:%d: unexpected token:", input_files[filei], lo.line_number, lo.line_offset);
        if (l.token > 256) nob_log(NOB_ERROR, "  -> found token: %ld,", l.token);
        else nob_log(NOB_ERROR, "  -> found token '%c',", (char)l.token);
        if (token > 256) nob_log(NOB_ERROR, "  -> expected token: %ld", l.token);
        else nob_log(NOB_ERROR, "  -> expected token: '%c'", (char)token);
    } */
    return 0;
}