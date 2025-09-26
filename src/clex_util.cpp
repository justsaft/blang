
extern "C" {
#include "../3rd-party/nob.h"
#include "../3rd-party/stb_c_lexer.h"
}

#include "types.hpp"
#include "common.hpp"

void get_lexer_location(const stb_lexer& l, stb_lex_location& lo)
{
    stb_c_lexer_get_location(&l, l.where_firstchar, &lo);
    lo.line_offset += 1;
}

void unexpected_eof(const char* filename, const stb_lexer& l, stb_lex_location& lo)
{
    size_t len = strlen(l.input_stream);

    if (len == 0) {
        nob_log(NOB_ERROR, "   %d:%d: Unexpected EOF: Input stream was empty.",
                lo.line_number, lo.line_offset);
    } else {
        nob_log(NOB_ERROR, "%s:%d:%d: Unexpected EOF:\n %s<EOF>",
                filename, lo.line_number, lo.line_offset,
                len > 12 ? &l.where_lastchar[-(signed)12] : &l.where_lastchar[-(signed)len]);
    }
}

void unexpected_eof(const char* filename, const stb_lexer& l)
{
    stb_lex_location lo;
    get_lexer_location(l, lo);

    size_t len = strlen(l.input_stream);

    if (len == 0) {
        nob_log(NOB_ERROR, "   %d:%d: Unexpected EOF: Input stream was empty.",
                lo.line_number, lo.line_offset);
    } else {
        nob_log(NOB_ERROR, "%s:%d:%d: Unexpected EOF:\n %s<EOF>",
                filename, lo.line_number, lo.line_offset,
                len > 12 ? &l.where_lastchar[-(signed)12] : &l.where_lastchar[-(signed)len]);
    }

    throw;
}

void step_lexer(stb_lexer& l)
{
    if (!stb_c_lexer_get_token(&l))
        unexpected_eof(nullptr, l);
}

long get_next_token(stb_lexer& l)
{
    step_lexer(l);
    return l.token;
}

long peak_next_token(stb_lexer& l)
{
    stb_lexer fork = l;
    step_lexer(fork);
    return fork.token;
}

//const char* token_or_char(long token)
//{
//    return std::to_string(token < 256 ? (char)token : token).c_str();
//}

bool get_and_expect_token(stb_lexer& l, const long token)
{
    return get_next_token(l) == token;
}

uint8_t semicolon_next(stb_lexer& l, const char* filename, bool advance_pre, bool advance_post)
{
    stb_lex_location pos;

    if (advance_pre)
        step_lexer(l);

    get_lexer_location(l, pos);

    switch (l.token) {
        case ';':
            if (advance_post)
                step_lexer(l);
            return Success;

        default:
            //NOB_TODO("Unwind after missing semicolon");
            nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: expected semicolon.", filename, pos.line_number, pos.line_offset);
            // Compilation_error(ExpectedSemicolon);
            return ExpectedSemicolon; // Didn't get a semicolon
    }
}

bool expect_token(stb_lexer& l, const long token/* , bool silent */)
{
    return l.token == token;
    /* else if (!silent) {
        stb_lex_location lo;
        get_lexer_location(l, lo);

        nob_log(NOB_ERROR, "%s:%d:%d: unexpected token:", input_files[filei], lo.line_number, lo.line_offset);
        if (l.token > 256) nob_log(NOB_ERROR, "  -> found token: %ld,", l.token);
        else nob_log(NOB_ERROR, "  -> found token '%c',", (char)l.token);
        if (token > 256) nob_log(NOB_ERROR, "  -> expected token: %ld", l.token);
        else nob_log(NOB_ERROR, "  -> expected token: '%c'", (char)token);
    } */
}