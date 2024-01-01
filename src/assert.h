//🫖ketl
#ifndef ketl_assert_h
#define ketl_assert_h

#include "logging.h"

// CHECK_VOE - Verify Or Error 
// CHECK_VOEM - Verify Or Error Message

#ifndef NDEBUG

#define KETL_CHECK_VOE(x) (!(x) && (KETL_ERROR("Assert: %s", #x), true))

#define KETL_CHECK_VOEM(x, string_literal_message, ...) (!(x) && (KETL_ERROR("Assert: "string_literal_message, __VA_ARGS__), true))

#else

#define KETL_CHECK_VOE(x) (!(x))

#define KETL_CHECK_VOEM(x, message) (!(x))


#endif // NDEBUG

#endif // ketl_assert_h
