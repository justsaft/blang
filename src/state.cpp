
#include <stdint.h>

extern "C" {
#include "../3rd-party/nob.h"
}

#include "state.hpp"

void update_state(States& st, const char* sym, const char* file_name, uint8_t fut)
{
	st.file = 0; // Reset file because update will only ever be called once per file

	if (strcmp(sym, "main") == 0) {
		if (st.file > States::FOUND_A_FUNC) {
			switch (st.file - States::FOUND_A_FUNC) {
			case 0: // empty main
				nob_log(NOB_WARNING, "Multiple `main`s: found an empty main in %s", file_name); break;
			case 1: // main
				nob_log(NOB_WARNING, "Multiple `main`s: found a main in %s", file_name); break;
			default: // don't do anything on MULTIPLE_MAIN
				break;
			}
			st.file = States::MULTIPLE_MAIN;
		}
		else if (fut > st.file) {
			st.file = fut + 2;
		}

		if (st.global > States::FOUND_A_FUNC) {
			switch (st.global - States::FOUND_A_FUNC) {
			case 0: // empty main
				nob_log(NOB_WARNING, "Multiple `main`s: multiple empty mains found"); break;
			case 1: // main
				nob_log(NOB_WARNING, "Multiple `main`s: "); break;
			default: // don't do anything on MULTIPLE_MAIN
				break;
			}
			st.global = States::MULTIPLE_MAIN;
		}
		else if (fut > st.global) {
			st.global = fut + 2;
		}
	}
	else {
		if (fut > st.file) {
			st.file = fut;
		}

		if (fut > st.global) {
			st.global = fut;
		}
	}
}

bool get_file_state(States& st)
{
	switch (st.file) {
	case States::HAS_NOTHING: nob_log(NOB_ERROR, "No function definitions found in any passed file. Define main as `main() { ... }`"); break;
	case States::FOUND_EMPTY_MAIN: nob_log(NOB_ERROR, "Encountered main function definition, but it was empty."); break;
	case States::MULTIPLE_MAIN: nob_log(NOB_ERROR, "Encountered multiple definitions of `main`"); break;
	default: return true; // Good state
	}
	return false;
}

bool get_state(States& st)
{
	switch (st.global) {
	case States::HAS_NOTHING: nob_log(NOB_ERROR, "No function definitions found in any passed file. Define main as `main() { ... }`"); break;
	case States::FOUND_A_FUNC: nob_log(NOB_ERROR, "A function was found, but there was no main function. Define main as `main() { ... }`"); break;
	case States::FOUND_EMPTY_MAIN: nob_log(NOB_ERROR, "Encountered main function definition, but it was empty."); break;
	case States::MULTIPLE_MAIN: nob_log(NOB_ERROR, "Encountered multiple definitions of `main`"); break;
	default: return true; // Good state
	}
	return false; // Bad state
}