
#ifndef _BACKEND_HPP
#define _BACKEND_HPP

#include "types.hpp"

const char* backend2str(Backend b);
bool is_backend_installed(void);
bool is_backend_installed(const Backend);
std::string get_target_triple(const Backend);
std::string get_target_triple(void);
// bool is_any_backend_installed(void);

#endif