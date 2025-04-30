
#include <stdint.h>
#include <stdio.h>

#include "state.hpp"
#include "types.hpp"
#include "gen_ir.hpp"

extern "C" {
#include "../3rd-party/nob.h"
#include "../3rd-party/stb_c_lexer.h"

#include "output.h"
	bool write_ll_file(const char* original_file, const char* ir, size_t ir_size);
}

// states.cpp
bool get_state(States& st);
bool get_file_state(States& st);
void update_state(States& st, const char* sym, const char* file_name, uint8_t fut);

// output.cpp
bool dispatch_clang(const CStrings& original_file, const std::string& output_file);
/* bool run_clang(const char* output_file); */

static void usage(const char* program_name)
{
	fprintf(stderr, "Usage: %s <Options>\n", program_name);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -h, --help       Show this help message\n");
	fprintf(stderr, "  -v, --version    Show version information\n");
	fprintf(stderr, "  -o, --output     Specify output file\n");
	fprintf(stderr, "  -t, --target     Specify the target triple, e.g. x86_64-pc-windows-msvc\n");
}

static void version()
{
	fprintf(stderr, "Version: 0.0.1\n");
	fprintf(stderr, "Build date: %s\n", __DATE__);
	fprintf(stderr, "Build time: %s\n", __TIME__);
}

// Parsing
void parse_function_body(stb_lexer&, std::string&, B_Function_Scope&, B_Stack&, const B_Function_Scope&);
void parse_function_parameters(stb_lexer&, B_Stack&, uint8_t& out_amount_params);
void parse_statement(stb_lexer&, B_Stack&, const B_Function_Scope&, const B_Function_Scope&);
void parse_variable_assignment(stb_lexer&, B_Stack&);

// Tokens
bool get_and_expect_token(stb_lexer&, int);
bool expect_semicolon(stb_lexer&, bool advance = false);
bool expect_token(stb_lexer&, long);

// Find
size_t find_function(const char* name, const B_Function_Scope& scope);
size_t find_variable(const char* name, const B_Stack& scope);

// Redef
bool detect_symbol_redef(const B_Function& sym, const B_Function_Scope& scope);
bool detect_var_redef(const B_Variable& sym, const B_Stack& scope);

// Util
void munch_parameters(stb_lexer&, uint8_t& out_amount_params);
void unexpected_eof(const char* last_parsed);
void step_lexer(stb_lexer& l);

// for handling input files
static uint8_t filei = 0;
static CStrings input_files;

int main(int argc, char** argv)
{
	if (argc < 2) {
		nob_log(NOB_ERROR, "Error: No arguments or input files. Use '-h' to get usage.");
		usage(argv[0]);
		exit(NO_ARGUMENTS);
	}

	std::string output_override;
	std::string target = "x86_64-pc-linux-gnu";

	for (uint8_t arg = 1; arg < argc; ++arg) {
		if (strcmp(argv[arg], "-v") == 0 || strcmp(argv[arg], "--version") == 0) { // -v flag
			version();
			exit(SUCCESS);
		}
		else if (strcmp(argv[arg], "-h") == 0 || strcmp(argv[arg], "--help") == 0) { // -h flag
			usage(argv[0]);
			exit(SUCCESS);
		}
		else if (strcmp(argv[arg], "-o") == 0 || strcmp(argv[arg], "--output") == 0) { // -o flag
			static bool duplicate_o = false;
			if (duplicate_o) {
				fprintf(stderr, "Error: There cannot be two outputs specified\n");
				exit(UNEXPECTED_ARGUMENTS);
			}
			++arg;
			output_override = argv[arg];
			duplicate_o = true;
		}
		else if (strcmp(argv[arg], "-t") == 0 || strcmp(argv[arg], "--target") == 0) { // -t flag
			static bool duplicate_t = false;
			if (duplicate_t) {
				fprintf(stderr, "Error: There cannot be two targets specified\n");
				exit(UNEXPECTED_ARGUMENTS);
			}
			if (!verify_target_triple(target)) {
				exit(INVALID_TARGET_TRIPLE);
			}
			++arg;
			duplicate_t = true;
			target = argv[arg];
		}
		else { // input files
			const char* file = argv[arg];
			if (!nob_file_exists(file)) {
				nob_log(NOB_ERROR, "File doesn't exist: %s", file);
				if (input_files.empty()) {
					exit(NO_FILES);
				}
				else break;
			}
			input_files.push_back(file);
		}
	}

	States st;
	Nob_String_Builder sb /*= { 0 }*/; // storage place for clex's input streams
	memset(&sb, 0, sb.capacity);
	std::vector<char> clex_buf = { 0 };
	clex_buf.resize(0x1000);
	B_Function_Scope functions; // all function symbols across passed files

	for (; filei < input_files.size(); ++filei, sb.count = 0, memset(&sb, 0, sb.capacity)) // For each file
	{
		const char* current_file = input_files[filei];
		nob_log(NOB_INFO, "Processing file: %s", current_file);
		std::string ir; // output ir representation of B syntax

		if (!nob_read_entire_file(current_file, &sb)) {
			nob_log(NOB_ERROR, "Could not read file %s", current_file);
			exit(ERROR_READ_INPUT);
		}

		gen_basic_ir_info(ir, target, input_files[filei]);

		stb_lexer l;
		stb_c_lexer_init(&l, sb.items, sb.items + sb.count, clex_buf.data(), clex_buf.capacity());

		while (stb_c_lexer_get_token(&l)) // For each line in this file
		{
			if (l.token < 256) {
				nob_log(NOB_ERROR, "Unexpected %c", (char)l.token);
				exit(INVALID_SYNTAX);
			}
			else if (l.token == CLEX_eof /*&& !retval*/) {
				// TODO: retval approach, "if (l.token == CLEX_eof" approach or while loop approach
				//if (file_state == States::HAS_NOTHING) {
				//	nob_log(NOB_INFO, "Unexpected EOF in %s. Expected to get any token or function definition.", input_files[filei]);
				//	// TODO: Check if the last token is missing
				//	// TODO: Check if we should use retval instead to end the loop
				//	//       No adjustment needed as diagnosis is already handled elsewhere.
				//	//       Use of do {} while () is also possible.
				//	//       Final implementation depends on which part of the code should print the error.
				//	//       This bit irrelevant if the while loops ends when the EOF is reached.
				//	//       The state HAS_NOTHING is irrelevant for file_state if we end the loop and print here.
				//	//       Since here I'm not expecting to get another specific token I have the choice
				//}
				break; // Advances to the next file
			}
			else if (l.token == CLEX_parse_error) {
				nob_log(NOB_ERROR, "CLEX parse error: there's probably an issue with the buffer");
				exit(EVERYTHING_COULD_BE_WRONG);
			}
			else if (l.token == CLEX_intlit) {
				nob_log(NOB_ERROR, "Unexpected int literal: %ld", (int64_t)l.int_number);
				exit(INVALID_SYNTAX);
			}
			else if (l.token == CLEX_floatlit) {
				nob_log(NOB_ERROR, "Unexpected float literal: %f.3", l.real_number);
				exit(INVALID_SYNTAX);
			}
			else if (l.token == CLEX_id) { // string literals without quotes
				const char* this_name = l.string;
				uint64_t where = (uint64_t)l.where_firstchar; // TODO: I'm not using where_firstchar correctly
				nob_log(NOB_INFO, "Found: clex id: %s at %ld", this_name, where);

				if (strcmp(this_name, "auto") == 0) {
					NOB_TODO("Variable declarations in global scope?");
				}
				else if (strcmp(this_name, "extrn") == 0) {
					NOB_TODO("Extrn in global scope?");
				}
				else if (get_and_expect_token(l, '(')) {
					B_Function_Scope extrns; // Externals
					B_Stack locals; // Local variables
					uint8_t amount_params = 0;

					if (!get_and_expect_token(l, ')')) { // Must parse parameters next
						parse_function_parameters(l, locals, amount_params);
					}

					B_Function f = std::make_tuple(this_name, amount_params, where, filei);
					detect_symbol_redef(f, functions);
					functions.push_back(f); // TODO: This allows for recursion, check if B is supposed to have it

					if (!get_and_expect_token(l, '{')) {
						nob_log(NOB_ERROR, "Expected function body to start with '{'");
						exit(INVALID_SYNTAX);
					}

					gen_function_def(ir, f);

					if (!get_and_expect_token(l, '}')) { // Check if the body ends early
						parse_function_body(l, ir, extrns, locals, functions);
						update_state(st, this_name, input_files[filei], States::FOUND_FUNC);
					}
					else {
						nob_log(NOB_WARNING, "%s:%ld:%d: Warning: encountered empty function: %s", input_files[filei], where, 1, this_name);
						update_state(st, this_name, input_files[filei], States::FOUND_EMPTY_FUNC);
					}

					gen_function_def_end(ir);
				}
				else if (expect_token(l, '=')) {
					NOB_TODO("Variable assignment to be implemented");
				}
				else if (expect_token(l, ';')) {
					; // literally do nothing when encountering ';'
				}
			}
			else if (l.token == CLEX_dqstring) {
				nob_log(NOB_ERROR, "Unexpected double-quoted string literal: %s", l.string);
				exit(UNSUPPORTED_DATA);
			}
			else if (l.token == CLEX_sqstring) {
				nob_log(NOB_ERROR, "Unexpected single-quoted string literal: %s", l.string);
				exit(UNSUPPORTED_DATA);
			}
			else if (l.token == CLEX_charlit) {
				nob_log(NOB_ERROR, "Unexpected char literal: %s", l.string);
				exit(UNSUPPORTED_DATA);
			}
			else {
				nob_log(NOB_WARNING, "Token not grabbed: %ld", l.token);
			}
		}

		// Once there are no more tokens...
		if (!get_file_state(st)) {
			exit(EVERYTHING_COULD_BE_WRONG);
		}
		else if (!write_ll_file(input_files[filei], ir.data(), ir.size())) {
			nob_log(NOB_WARNING, "Did not generate any function specific IR");
		}
		else if (!dispatch_clang(input_files, output_override)) {
			nob_log(NOB_ERROR, "Did not generate any function specific IR");
		}
	}

	return SUCCESS;
}

void parse_function_body(
	stb_lexer& l,
	std::string& ir,
	B_Function_Scope& extrns,
	B_Stack& local_vars,
	const B_Function_Scope& local_functions
)
{
	do {
		if (l.token == CLEX_id) { // Note: Assumes the lexer has already been stepped past the opening curly brace '{' of the function body
			uint64_t where = (uint64_t)l.where_firstchar;

			if (strcmp(l.string, "extrn") == 0) { // extrn keyword
				if (!get_and_expect_token(l, CLEX_id)) {
					nob_log(NOB_ERROR, "Syntax error: expected a name after `extrn`");
					exit(INVALID_SYNTAX);
				}

				const char* name = l.string;
				uint8_t amount_params = 0;

				if (get_and_expect_token(l, '(')) {
					if (!get_and_expect_token(l, ')')) { // Must parse parameters next
						munch_parameters(l, amount_params);
					}
				}

				B_Function e = std::make_tuple(name, amount_params, where, filei);
				detect_symbol_redef(e, extrns);
				extrns.push_back(e);

				gen_keyword(ir, Extrn, local_vars);

				if (!expect_semicolon(l, false)) {
					nob_log(NOB_ERROR, "Syntax error: expected semicolon after `extrn` definition");
					exit(INVALID_SYNTAX);
				}
			}
			else if (strcmp(l.string, "auto") == 0) { // auto keyword
				step_lexer(l);
				parse_variable_assignment(l, local_vars);

				// TODO: Generate IR
				//gen_keyword(ir, Auto, local_vars);
			}
			else if (strcmp(l.string, "return") == 0) { // return keyword
				if (!get_and_expect_token(l, CLEX_intlit)) {
					nob_log(NOB_ERROR, "%s:%ld:%d: Syntax error: expected int literal for return keyword", input_files[filei], (uint64_t)l.where_firstchar, 1);
					exit(INVALID_SYNTAX);
				}

				B_Variable retval = std::make_tuple("___return", (uint64_t)l.int_number, where, filei);
				detect_var_redef(retval, local_vars);
				local_vars.push_back(retval);

				gen_keyword(ir, Return, local_vars);

				if (!expect_semicolon(l, false)) {
					nob_log(NOB_ERROR, "Syntax error: expected semicolon after `return` int literal");
					exit(INVALID_SYNTAX);
				}
			}

			// TODO: Add keywords here

			else { // could be var assignment, function call, >>=, etc.
				parse_statement(l, local_vars, local_functions, extrns);
			}
		}
	}
	while (stb_c_lexer_get_token(&l) && (l.token != '}'));
}

bool detect_symbol_redef(const B_Function& sym, const B_Function_Scope& scope)
{
	const char* filename = input_files[filei];
	const char* name = std::get<FunctionSymbolIdx::NAME>(sym);
	for (auto& i : scope) {
		if (strcmp(std::get<FunctionSymbolIdx::NAME>(i), name) == 0) {
			nob_log(NOB_ERROR, "%s:%ld:%d: Variable redefinition: attempted to redefine %s", filename, std::get<FunctionSymbolIdx::WHERE>(sym), 1, name);
			nob_log(NOB_ERROR, "%s:%ld:%d: <--- already defined here", input_files[std::get<FunctionSymbolIdx::FILE>(i)], std::get<FunctionSymbolIdx::WHERE>(i), 1);
			// TODO: Handle overloads, if B has them, otherwise keep the same.
			return 0; // Returns 0 if exists
		}
	}
	return 1; // Returns 1 if doesn't exist
}

bool detect_var_redef(const B_Variable& sym, const B_Stack& scope)
{
	const bool allow_shadowed = false; // TODO: find out if B can have shadowed values, otherwise remove this
	const char* filename = input_files[filei];
	const char* name = std::get<StackVarIdx::NAME>(sym);
	for (auto& i : scope) {
		if (strcmp(std::get<StackVarIdx::NAME>(i), name) == 0) {
			if (allow_shadowed) {
				NOB_TODO("replace original value");
			}
			else {
				nob_log(NOB_ERROR, "%s:%ld:%d: Variable redefinition: attempted to redefine %s", filename, std::get<StackVarIdx::WHERE>(sym), 1, name);
				nob_log(NOB_ERROR, "%s:%ld:%d: <--- already defined here", input_files[std::get<StackVarIdx::FILE>(i)], std::get<StackVarIdx::WHERE>(i), 1);
				return 0; // Returns 0 if exists
				// TODO: Handle overloads, if B has them, otherwise keep the same.
			}
		}
	}
	return 1; // Returns 1 if doesn't exist
}

void step_lexer(stb_lexer& l)
{
	if (!stb_c_lexer_get_token(&l)) {
		unexpected_eof("asd");
	}
}

void parse_function_parameters(stb_lexer& l, B_Stack& params, uint8_t& out_amount_params)
{
	if (l.token == ')') { // We're already past '('
		return;
	}
	else if (l.token == ',') {
		if (get_and_expect_token(l, CLEX_id)) {
			parse_function_parameters(l, params, out_amount_params);
		}
		else {
			nob_log(NOB_ERROR, "Syntax error: expected name while counting function parameter list");
			exit(INVALID_SYNTAX);
		}
	}
	else if (l.token == CLEX_id) {
		const char* name = l.string;
		uint64_t where = (uint64_t)l.where_firstchar;
		int64_t value = 0;

		if (get_and_expect_token(l, '=')) {
			if (get_and_expect_token(l, CLEX_intlit)) {
				value = l.int_number;
				step_lexer(l);
			}
			else {
				nob_log(NOB_ERROR, "%s:%ld:%d: Syntax error: expected default value assignment for %s", input_files[filei], where, 1, name);
				exit(INVALID_SYNTAX);
				// TODO: print accosiated funtion name
			}
		}
		params.push_back(std::make_tuple(name, value, where, filei));
		++out_amount_params;
	}
	else {
		nob_log(NOB_ERROR, "Syntax error: unexpected token while parsing function parameter list");
		exit(INVALID_SYNTAX);
	}
	// TODO: What about a do-while loop? Better? Worse?
	// It's gonna be nessesary when I overflow the stack
}

void munch_parameters(stb_lexer& l, uint8_t& out_amount_params)
{
	if (l.token == ')') {
		return;
	}
	else if (l.token == ',') {
		if (get_and_expect_token(l, CLEX_id)) {
			munch_parameters(l, out_amount_params);
		}
		else {
			nob_log(NOB_ERROR, "Syntax error: expected name while counting function parameter list");
			exit(INVALID_SYNTAX);
		}
	}
	else if (l.token == CLEX_id) {
		++out_amount_params;
		step_lexer(l);
	}
	else {
		nob_log(NOB_ERROR, "Syntax error: unexpected token while counting function parameter list");
		exit(INVALID_SYNTAX);
	}
}

void unexpected_eof(const char* last_parsed)
{
	nob_log(NOB_ERROR, "Encountered unexpected EOF while last working on >%s<", last_parsed);
	exit(UNEXPECTED_EOF);
}

bool get_and_expect_token(stb_lexer& l, int token)
{
	if (!stb_c_lexer_get_token(&l)) {
		unexpected_eof("get_and_expect_token");
	}

	if (l.token == token) {
		return true;
	}
	return false;
}

bool expect_semicolon(stb_lexer& l, bool advance)
{
	if (stb_c_lexer_get_token(&l)) {
		switch (l.token) {
		case ';':
			if (advance) step_lexer(l);
			return 1;

		default:
			//NOB_TODO("Unwind after missing semicolon");
			nob_log(NOB_ERROR, "Expected semicolon");
			return 0;
		}
	}
	else {
		nob_log(NOB_ERROR, "Unexpected EOF while expected to see a semicolon.");
		exit(UNEXPECTED_EOF);
	}
}

bool expect_token(stb_lexer& l, long token)
{
	if (l.token == token) {
		return 1;
	}
	else {
		nob_log(NOB_ERROR, "Unexpected token %ld, expected %ld", l.token, token);
		return 0;
	}
}

void parse_statement(stb_lexer& l, B_Stack& params, const B_Function_Scope& syms, const B_Function_Scope& extrns)
{
	NOB_UNUSED(l);
	NOB_UNUSED(params);
	NOB_UNUSED(syms);
	NOB_UNUSED(extrns);
	NOB_TODO("Parse statement");
}

void parse_variable_assignment(stb_lexer& l, B_Stack& scope)
{
	if (!get_and_expect_token(l, CLEX_id)) {
		nob_log(NOB_ERROR, "Syntax error: expected variable name next");
		exit(INVALID_SYNTAX);
	}

	const char* name = l.string;
	uint64_t where = (uint64_t)l.where_firstchar;
	int64_t value = 0;
	// TODO: Where-Position needs to be calculated with an offset when files/functions gets longer
	// We need to save the line number the buffer started at, then allocate more buffers

	if (get_and_expect_token(l, '=')) {
		if (get_and_expect_token(l, CLEX_intlit)) {
			value = l.int_number;
		}
	}
	else if (l.token != ';') {
		nob_log(NOB_ERROR, "Syntax error: expected `;` or variable assignment for %s next", name);
		exit(INVALID_SYNTAX);
	}

	B_Variable variable = std::make_tuple(name, value, where, filei); // Could've used cliteral but didn't
	if (!detect_var_redef(variable, scope)) {
		nob_log(NOB_ERROR, "%s:%ld:%d: Variable redefinition: attempted to redefine `%s`", input_files[filei], where, 1, name);
		exit(VARIABLE_REDEF);
	}

	scope.push_back(variable);
	return /*1*/;
}

bool dispatch_clang(const CStrings& input_files, const std::string& output_file)
{
	bool override = !output_file.empty();

	for (uint8_t i = 0; i < input_files.size(); ++i) {
		const char* ll_file = swap_extension(input_files[i], "ll");
		const char* output_file_type = "";

		if (override) {
			output_file_type = strrchr(output_file.c_str(), '.');
			nob_log(NOB_INFO, "Overridden file name: %s", output_file.c_str());
		}

		if ((strcmp(output_file_type, ".ll") == 0)) {
			nob_log(NOB_INFO, "Outputting LLVM-IR only");
			nob_log(NOB_INFO, "Note: LLVM-IR is outputted regardless of what you're compiling to.");
			return true;
		}
		else if ((strcmp(output_file_type, ".obj") == 0) || (strcmp(output_file_type, ".o") == 0)) { // .obj file
			return run_clang(swap_extension(input_files[i], "o"), ll_file, COMPILE);
		}
		else if (strcmp(output_file_type, ".exe") == 0) { // .exe file
			return run_clang(override ? output_file.data() : swap_extension(input_files[i], "exe"), ll_file, COMPILE);
		}

		// TODO: implement other outputs above
		else { // Assume it is NULL and assume executable without file extension (e.g. Linux)
			return run_clang(override ? chop_extension(output_file.c_str()) : chop_extension(input_files[i]), ll_file, COMPILE);
		}
	}

	if (input_files.size() > 1) {
		std::string files_to_link;

		for (uint8_t i = 0; i < input_files.size(); ++i) {
			if (i != 0) {
				files_to_link += swap_extension(input_files[i], "exe");
			}
		}

		nob_log(NOB_INFO, "Linking multiple files into %s", output_file.c_str());
		return run_clang(output_file.data(), files_to_link.data(), LINK);
	}

	return false;
}

size_t find_variable(const char* name, const B_Stack& scope)
{
	for (size_t i = 0; i < scope.size(); ++i) {
		if (strcmp(std::get<StackVarIdx::NAME>(scope[i]), name) == 0) {
			return i;
		}
	}
	return -1; // Not found
}

size_t find_function(const char* name, const B_Function_Scope& scope)
{
	for (size_t i = 0; i < scope.size(); ++i) {
		if (strcmp(std::get<FunctionSymbolIdx::NAME>(scope[i]), name) == 0) {
			return i;
		}
	}
	return -1; // Not found
}