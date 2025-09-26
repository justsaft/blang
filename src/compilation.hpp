
#include "types.hpp"
#include "backend.hpp"

struct Compilation {
public:
    bool stop = false,
        has_entry = false,
        wants_executable = true;

    int errors = 0,
        warnings = 0;

    Returns state = Success;
    std::string target;
    std::string output;

public:
    LangMode GetLangMode(void) const
    {
        return lang_mode;
    }

    WordSize GetWordSize(void) const
    {
        return word_size;
    }

    IR_Output GetIROutput(void) const
    {
        return irout;
    }

    const std::string& GetTargetTriple(void) const
    {
        return target;
    }

    bool IsLangMode(LangMode lm) const
    {
        return lang_mode == lm;
    }

    bool IsWordSize(WordSize ws) const
    {
        return word_size == ws;
    }

protected:
    friend void parse_cli_arguments(int, char**, B_Files&, Compilation&);

    void SetLangMode(LangMode lm)
    {
        lang_mode = lm;
    }

    void SetWordSize(WordSize ws)
    {
        word_size = ws;
    }

    void SetIROutput(IR_Output iro)
    {
        irout = iro;
    }

private:
    LangMode lang_mode = Historical;
    WordSize word_size = SixteenBit;
    IR_Output irout = DeleteIrAfterCompile;
};