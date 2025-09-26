
#ifndef _CLEX_WRAPPER_HPP
#define _CLEX_WRAPPER_HPP

extern "C" {
#include "../3rd-party/stb_c_lexer.h"
#include "../3rd-party/nob.h"
}

#include "types.hpp"
#include "common.hpp"
#include "string.h"

constexpr int CLEX_BUFFER_DEFAULT_SIZE = 0x1000;

void unexpected_eof(const char* filename, const stb_lexer&, stb_lex_location&);
void unexpected_eof(const char* filename, const stb_lexer&);


enum CLEX_expansion {
    CLEX_funccall = -260,
    CLEX_funccall_open_paren,
    CLEX_funccall_close_paren,
    CLEX_funccall_param,
};


// typedef struct FatToken {
// 	int token;
// 	const char* data;
// } FatToken;


// FatToken make_fat_token(int token, const char* data)
// {
//     return FatToken { token, strdup(data) };
// }


// typedef struct LexedStatement {
// 	bool assigns;
// 	bool has_funccall;
// 	std::vector<FatToken> tokens;
// 	// Returns error = Success;
// } LexedStatement;


typedef struct Lexer {

public:
    const char* const& filename = m_FileName;
    const stb_lex_location& location = m_Location;
    const long& token = m_Lexer.token;
    char* const& string = m_Lexer.string;
    // char* const& int_number = m_Lexer.int_number;
    // char* const& content = m_Lexer.input_stream;
    // char* const& cursor = m_Lexer.parse_point;

public:
    Lexer()
    {
        m_ClexBuffer.reserve(CLEX_BUFFER_DEFAULT_SIZE);
    }

    ~Lexer() = default;


public: // copy constructor
    Lexer(const Lexer& other) :
        m_FileName(other.m_FileName),
        m_Lexer(other.m_Lexer),
        m_Location(other.m_Location),
        m_ClexBuffer(other.m_ClexBuffer),
        m_ClexInputStream(other.m_ClexInputStream)
    { }


public:
    stb_lexer& GetLexer(void)
    {
        return m_Lexer;
    }

    stb_lexer CopyLexer(void) const
    {
        return m_Lexer;
    }

    //LexedStatement LexStatement(void) const
    //{
    //	constexpr FatToken LastInStatement = { ';', nullptr };
    //	LexedStatement result;
    //	for (; m_Lexer.token != LastInStatement.token;) {
    //		result.tokens.push_back(FatToken { (int)m_Lexer.token, strdup(m_Lexer.string) });
    //	}
    //	return result;
    //}

    //FatToken GetFatToken(void) const
    //{
    //	// const char* new_data = nullptr;
    //	// switch (m_Lexer.token) {
    //	// case 260:
    //	//     new_data = m_Lexer.string;
    //	// default:
    //	//     NOB_UNREACHABLE("Switch on token for FatToken data");
    //	// }
    //	return FatToken { (int)m_Lexer.token, strdup(m_Lexer.string) };
    //}

    stb_lex_location GetLocation(void) const
    {
        return m_Location;
    }

    // int GetLine(void) const
    // {
    //     return m_Location.line_number;
    // }

    // int GetLineOffset(void) const
    // {
    //     return m_Location.line_offset;
    // }

    void Locate(void)
    {
        stb_c_lexer_get_location(&m_Lexer, m_Lexer.where_firstchar, &m_Location);
        m_Location.line_number += 1;
    }

    uint8_t Semicolon(bool advance_pre = false, bool advance_post = true)
    {
        if (advance_pre)
            Step();

        Locate();

        switch (m_Lexer.token) {
            case ';':
                if (advance_post)
                    Step();

                return Success;

            default:
                nob_log(NOB_ERROR, "%s:%d:%d: Invalid syntax: expected semicolon.",
                        m_FileName, m_Location.line_number, m_Location.line_offset);
                return ExpectedSemicolon; // Didn't get a semicolon
        }
    }

    // char* GetString(void)
    // {
        // return m_Lexer.string;
    // }

    std::string GetTokenForText(void) const
    {
        return (m_Lexer.token < 256)
            ? std::string(1, static_cast<char>(m_Lexer.token))
            : std::to_string(m_Lexer.token);
    }

    int GetToken(void) const
    {
        return (int)m_Lexer.token;
    }

    char GetTokenAsChar(void) const
    {
        return (char)m_Lexer.token;
    }

    int GetNextToken(void)
    {
        Step();
        return (int)m_Lexer.token;
    }

    int GetNextTokenChecked(void)
    {
        StepChecked();
        return (int)m_Lexer.token;
    }

    int PeakNextToken(void)
    {
        stb_lexer fork = m_Lexer;

        if (!stb_c_lexer_get_token(&fork)) {
            stb_lex_location fork_location;
            stb_c_lexer_get_location(&fork, fork.where_firstchar, &fork_location);
            unexpected_eof(m_FileName, fork, fork_location);
        }

        return (int)fork.token;
    }

    bool PeakNextTokenAndExpect(const int token)
    {
        stb_lexer fork = m_Lexer;

        if (!stb_c_lexer_get_token(&fork)) {
            stb_lex_location fork_location;
            stb_c_lexer_get_location(&fork, fork.where_firstchar, &fork_location);
            unexpected_eof(m_FileName, fork, fork_location);
        }

        return fork.token == token;
    }

    bool StepAndExpect(const int token)
    {
        return GetNextToken() == token;
    }

    bool Expect(const int token) const
    {
        return ((int)m_Lexer.token) == token;
    }

    void Step(int steps = 1)
    {
        for (int i = 0; i < steps; ++i)
            stb_c_lexer_get_token(&m_Lexer);
    }

    bool StepChecked(int steps = 1)
    {
        for (int i = 0; i < steps; ++i) if (!stb_c_lexer_get_token(&m_Lexer)) {
            unexpected_eof(m_FileName, m_Lexer, m_Location);
            return false;
        }
        return true;
    }

    void IncreaseBufferSize(size_t size)
    {
        size += m_ClexBuffer.capacity();
        m_ClexBuffer.resize(size);
    }

    void IncreaseBufferSizeMult(size_t size)
    {
        size *= m_ClexBuffer.capacity();
        m_ClexBuffer.resize(size);
    }

    Returns InitAndLoadFile(const char* filename)
    {
        if (m_ClexInputStream.count > m_ClexBuffer.capacity())
            m_ClexBuffer.resize(m_ClexInputStream.count);

        m_ClexInputStream.count = 0;
        m_ClexBuffer.clear();

        if (m_FileName)
            free((void*)m_FileName);

        m_FileName = strdup(filename);

        if (!nob_read_entire_file(m_FileName, &m_ClexInputStream)) {
            nob_log(NOB_ERROR, "Could not load file %s.", m_FileName);
            return ErrorReadInput;
        }

        stb_c_lexer_init(&m_Lexer, m_ClexInputStream.items, m_ClexInputStream.items + m_ClexInputStream.count, m_ClexBuffer.data(), (int)m_ClexBuffer.capacity());

        if (!stb_c_lexer_get_token(&m_Lexer)) {
            nob_log(NOB_ERROR, "File %s is empty.", m_FileName);
            return FileEmpty;
        }

        switch (m_Lexer.token) {
            case CLEX_eof:
                NOB_UNREACHABLE("Reached end-of-file sanity check unreachable.");

            case CLEX_parse_error:
                nob_log(NOB_ERROR, "CLEX parse error: likely an issue with the buffer");
                return EverythingCouldBeWrong; // Shutup the compiler

            default:
                break; // We're good
        }

        return Success;
    }

private:
    const char* m_FileName = NULL;
    stb_lexer m_Lexer { };
    stb_lex_location m_Location { };
    std::vector<char> m_ClexBuffer { };
    Nob_String_Builder m_ClexInputStream { };
} Lexer;

#endif