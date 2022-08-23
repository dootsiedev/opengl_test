#include "../3rdparty/utfcpp/core.hpp"

#include <string>

const char* cpputf_get_error(utf8::internal::utf_error err_code);
bool cpputf_append_string(std::string& str, char32_t cp);