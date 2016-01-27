#pragma once

#include <stdexcept>
#include <string>

class xna_error : public std::runtime_error
{
	public:
		explicit xna_error(const std::string& what_arg);
		explicit xna_error(const char* what_arg);
};

class lzx_error : public xna_error
{
	public:
		explicit lzx_error(const std::string& what_arg);
		explicit lzx_error(const char* what_arg);
};