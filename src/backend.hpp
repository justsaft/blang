
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
    const char* GetCommand(void) const;

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


protected:
    friend void parse_cli_arguments(int, char**, B_Files&, Compilation&);
    bool SetFromCli(const char*);

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

    Backends backend = Autofind;
};


#endif