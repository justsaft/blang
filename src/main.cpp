
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
void parse_function_body(stb_lexer&, std::string&, B_Function_Scope&, B_Variable_Scope&, const B_Function_Scope&);
void parse_function_parameters(stb_lexer&, B_Variable_Scope&, uint8_t& out_amount_params);
void parse_statement(stb_lexer&, B_Variable_Scope&, const B_Function_Scope&, const B_Function_Scope&);
void parse_variable_assignment(stb_lexer&, B_Variable_Scope&);
void parse_extrn_keyword(stb_lexer&, B_Function_Scope& extrns);

// Tokens
bool get_and_expect_token(stb_lexer&, int);
bool expect_semicolon(stb_lexer&, bool advance = false);
bool expect_token(stb_lexer&, long);

// Find
size_t find_function(const char* name, const B_Function_Scope& scope);
size_t find_variable(const char* name, const B_Variable_Scope& scope);

// Redef
bool detect_symbol_redef(const B_Function& sym, const B_Function_Scope& scope);
bool detect_var_redef(const B_Variable& sym, const B_Variable_Scope& scope);

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
	B_Function_Scope external_storage; // all extrns

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
			else if (l.token == CLEX_id) { // string literals without quotes
				stb_lex_location location;
				stb_c_lexer_get_location(&l, l.where_firstchar, &location);

				const char* this_name = l.string;

				nob_log(NOB_INFO, "%s:%d:%d: Found: clex id: %s", input_files[filei], location.line_number, location.line_offset, this_name);

				if (strcmp(this_name, "auto") == 0) {
					NOB_TODO("Variable declarations in global scope?");
				}
				else if (strcmp(this_name, "extrn") == 0) {
					parse_extrn_keyword(l, external_storage);
				}
				else if (get_and_expect_token(l, '(')) {
					B_Variable_Scope locals; // Local variables
					uint8_t amount_params = 0;

					if (!get_and_expect_token(l, ')')) { // Must parse parameters next
						parse_function_parameters(l, locals, amount_params);
					}

					B_Function f { this_name, amount_params, location.line_number, location.line_offset, filei };
					detect_symbol_redef(f, functions);
					functions.push_back(f);

					if (!get_and_expect_token(l, '{')) {
						nob_log(NOB_ERROR, "Expected function body to start with '{'");
						exit(INVALID_SYNTAX);
					}

					gen_function_def(ir, f);

					if (!get_and_expect_token(l, '}')) { // Check if the body ends early
						parse_function_body(l, ir, external_storage, locals, functions);
						update_state(st, this_name, input_files[filei], States::FOUND_FUNC);
					}
					else {
						nob_log(NOB_WARNING, "%s:%d:%d: Warning: encountered empty function: %s", input_files[filei], location.line_number, location.line_offset, this_name);
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
	B_Variable_Scope& local_vars,
	const B_Function_Scope& local_functions
)
{
	do {
		if (l.token == CLEX_id) { // Note: Assumes the lexer has already been stepped past the opening curly brace '{' of the function body
			stb_lex_location location;
			stb_c_lexer_get_location(&l, l.where_firstchar, &location);

			if (strcmp(l.string, "extrn") == 0) { // extrn keyword
				NOB_TODO("Extrn keyword");
				parse_extrn_keyword(l, extrns);
			}
			else if (strcmp(l.string, "auto") == 0) { // auto keyword
				step_lexer(l);
				parse_variable_assignment(l, local_vars);

				// TODO: Generate IR
				//gen_keyword(ir, Auto, local_vars);
			}
			else if (strcmp(l.string, "return") == 0) { // return keyword
				if (!get_and_expect_token(l, CLEX_intlit)) {
					nob_log(NOB_ERROR, "%s:%d:%d: Syntax error: expected int literal for return keyword", input_files[filei], location.line_number, location.line_offset);
					exit(INVALID_SYNTAX);
				}

				local_vars[0] = B_Variable { "___returnval", std::to_string(l.int_number), location.line_number, location.line_offset, filei, Int };

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

bool detect_symbol_redef(const B_Function& s, const B_Function_Scope& scope)
{
	const char* filename = input_files[filei];

	for (auto& i : scope) {
		if (strcmp(i.name, s.name) == 0) {
			nob_log(NOB_ERROR, "%s:%d:%d: Variable redefinition: attempted to redefine %s", filename, s.line_number, s.char_offset, s.name);
			nob_log(NOB_ERROR, "%s:%d:%d: <--- %s is already defined here", input_files[i.filei], i.line_number, i.char_offset, i.name);
			return 0; // Returns 0 if exists
		}
	}
	return 1; // Returns 1 if doesn't exist
}

bool detect_var_redef(const B_Variable& s, const B_Variable_Scope& scope)
{
	const char* filename = input_files[filei];

	for (auto& i : scope) {
		if (strcmp(i.name, s.name) == 0) {
			nob_log(NOB_ERROR, "%s:%d:%d: Variable redefinition: attempted to redefine %s", filename, s.line_number, s.char_offset, s.name);
			nob_log(NOB_ERROR, "%s:%d:%d: <--- %s is already defined here", input_files[i.file], i.line_number, i.char_offset, i.name);
			return 0; // Returns 0 if exists
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

void parse_function_parameters(stb_lexer& l, B_Variable_Scope& scope, uint8_t& out_amount_params)
{
	do {
		if (l.token == ')') { // We're already past '('
			break;
		}
		else if (l.token == ',') {
			step_lexer(l);
		}
		else if (l.token == CLEX_id) {
			stb_lex_location location;
			stb_c_lexer_get_location(&l, l.where_firstchar, &location);

			if (strcmp(l.string, "auto") != 0) {
				nob_log(NOB_ERROR, "Syntax error: expected variable type declaration");
				exit(INVALID_SYNTAX);
			}

			if (!get_and_expect_token(l, CLEX_id)) {
				;
			}

			const char* name = l.string;

			if (get_and_expect_token(l, '=')) {
				nob_log(NOB_ERROR, "%s:%d:%d: Syntax error: no default assignment in function parameter list is supported, but used for %s", input_files[filei], location.line_number, location.line_offset, name);
				exit(INVALID_SYNTAX);
			}

			scope.push_back(B_Variable { name, "0", location.line_number, location.line_offset, filei, AutoVar });
			++out_amount_params;
		}
		else {
			nob_log(NOB_ERROR, "Syntax error: unexpected token while parsing function parameter list");
			exit(INVALID_SYNTAX);
		}
	}
	while (l.token != ',');
}

void munch_parameters(stb_lexer& l, uint8_t& out_amount_params)
{
	do {
		if (l.token == ')') { // We're already past '('
			break;
		}
		else if (l.token == ',') {
			step_lexer(l);
		}
		else if (l.token == CLEX_id) {
			stb_lex_location location;
			stb_c_lexer_get_location(&l, l.where_firstchar, &location);

			if (!get_and_expect_token(l, CLEX_id)) {
				;
			}

			const char* name = l.string;

			if (get_and_expect_token(l, '=')) {
				nob_log(NOB_ERROR, "%s:%d:%d: Syntax error: no default assignment in function parameter list is supported, but used for %s", input_files[filei], location.line_number, location.line_offset, name);
				exit(INVALID_SYNTAX);
			}

			++out_amount_params;
		}
		else {
			nob_log(NOB_ERROR, "Syntax error: unexpected token while parsing function parameter list");
			exit(INVALID_SYNTAX);
		}
	}
	while (l.token != ',');
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

void parse_statement(stb_lexer& l, B_Variable_Scope& params, const B_Function_Scope& syms, const B_Function_Scope& extrns)
{
	NOB_UNUSED(l);
	NOB_UNUSED(params);
	NOB_UNUSED(syms);
	NOB_UNUSED(extrns);
	NOB_TODO("Parse statement");
}

void parse_variable_assignment(stb_lexer& l, B_Variable_Scope& scope)
{
	if (!get_and_expect_token(l, CLEX_id)) {
		nob_log(NOB_ERROR, "Syntax error: expected variable name next");
		exit(INVALID_SYNTAX);
	}

	stb_lex_location location;
	stb_c_lexer_get_location(&l, l.where_firstchar, &location);

	const char* name = l.string;
	std::string value;
	Value_Type type;

	if (get_and_expect_token(l, '=')) {
		if (get_and_expect_token(l, CLEX_intlit)) {
			value = l.int_number;
			type = Int;
		}
		else if (expect_token(l, CLEX_floatlit)) {
			value = l.real_number;
			type = Float;
		}
	}
	else if (l.token != ';') {
		nob_log(NOB_ERROR, "Syntax error: expected `;` or variable assignment for %s next", name);
		exit(INVALID_SYNTAX);
	}

	B_Variable variable { name, value, location.line_number, location.line_offset, filei, type };

	if (!detect_var_redef(variable, scope)) {
		nob_log(NOB_ERROR, "%s:%d:%d: Variable redefinition: attempted to redefine `%s`", input_files[filei], location.line_number, location.line_offset, name);
		exit(VARIABLE_REDEF);
	}

	scope.push_back(variable);
	return;
}

void parse_extrn_keyword(stb_lexer&, B_Function_Scope& extrns)
{
	/* if (!get_and_expect_token(l, CLEX_id)) {
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

				B_Function e { name, amount_params, where, filei };
				detect_symbol_redef(e, extrns);
				extrns.push_back(e);

				if (!expect_semicolon(l, false)) {
					nob_log(NOB_ERROR, "Syntax error: expected semicolon after `extrn` definition");
					exit(INVALID_SYNTAX);
				} */
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

size_t find_variable(const char* name, const B_Variable_Scope& scope)
{
	for (size_t i = 1; i < scope.size(); ++i ) {
		if (strcmp(scope[i].name, name) == 0) {
			return i;
		}
	}
	return -1; // Not found
}

size_t find_function(const char* name, const B_Function_Scope& scope)
{
	for (size_t i = 1; i < scope.size(); ++i) {
		if (strcmp(scope[i].name, name) == 0) {
			return i;
		}
	}
	return -1; // Not found
}