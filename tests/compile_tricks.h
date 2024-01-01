/*🍲Ketl🍲*/
#ifndef ketl_compile_tricks_h
#define ketl_compile_tricks_h

#define BEFORE_MAIN_ACTION(action) \
namespace {\
	static bool success = action(); \
}

#endif // ketl_compile_tricks_h