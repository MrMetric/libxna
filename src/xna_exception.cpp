#include "../include/xna_exception.hpp"

#include <string>
using std::string;

xna_error::xna_error(const string& what_arg) : runtime_error(what_arg) {}
xna_error::xna_error(const char* what_arg) : runtime_error(what_arg) {}

lzx_error::lzx_error(const string& what_arg) : xna_error(what_arg) {}
lzx_error::lzx_error(const char* what_arg) : xna_error(what_arg) {}
