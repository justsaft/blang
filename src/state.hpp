
#include <stdint.h>

struct States {
	enum {
		HAS_NOTHING,
		FOUND_EMPTY_FUNC,
		FOUND_A_FUNC,
		FOUND_EMPTY_MAIN,
		HAS_MAIN,
		MULTIPLE_MAIN,
	};

	uint8_t global = HAS_NOTHING;
	uint8_t file = HAS_NOTHING;
};