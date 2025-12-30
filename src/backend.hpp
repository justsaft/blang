
#ifndef _BACKEND_HPP
#define _BACKEND_HPP

#include <string>

enum Backends : uint8_t {
    CLANG,
    LLC,
    TotalAmountOfBackends,
    NoBackendInstalled,
    Autofind,
};

const char* backend2str(Backends b);
bool is_backend_installed(Backends b);
std::string get_target_triple(Backends b);
std::string get_target_triple_clang(void);
std::string get_target_triple_llc(void);


struct Compilation;


struct Backend {
public:
    Backend()
    {
        RunAutofind();
    }

    Backend(Backends b)
    {
        SetAndCheck(b);
    }

    ~Backend() = default;


public:
    bool Check(void) const;
    bool Call() const;

#if BACKEND_OLD_COMMAND_GETS == ENABLED
    const char** GetCommand(void) const;
#else
    const std::vector<std::string>& GetCommand(void) const;
#endif

    inline const char* GetName(void) const
    {
        return backend2str(backend);
    }

    inline std::string GetTargetTriple(void) const
    {
        return get_target_triple(backend);
    }

    inline Backends Get(void) const
    {
        return backend;
    }

    inline void CmdAppendOptionalFlags(const char* flag)
    {
        cmd_additional.push_back(flag);
    }

    inline void CmdAppendOptionalFlags(const std::string& flag)
    {
        cmd_additional.push_back(flag);
    }

    inline const std::string* GetCmdOptionalFlag(void) const
    {
        return cmd_additional.data();
    }

    inline const std::vector<std::string>& GetCmdOptionalFlags(void) const
    {
        return cmd_additional;
    }

    inline const std::string& GetCmdOptionalFlag(size_t idx) const
    {
        return cmd_additional.at(idx);
    }

protected:
    friend void parse_cli_arguments(int, char**, B_Files&, Compilation&);
    // bool SetFromCli(const char*);

    inline void Set(Backends b)
    {
        backend = b;
    }

    inline bool SetAndCheck(Backends b)
    {
        backend = b;
        RunAutofind();
        return Check();
    }

private:
    void RunAutofind(void);

    std::vector<std::string> cmd_additional;
    Backends backend = Autofind;
};


#endif