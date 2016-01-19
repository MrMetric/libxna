#pragma once

#include <vector>
#include <cstdint>
#include <memory>
#include <BinaryReader.hpp>
#include "../include/Content.hpp"

namespace XNA {
namespace XNB {

enum class Flag : uint8_t
{
	hidef = 0x01, // content is for HiDef profile (otherwise Reach)
	compressed = 0x80,
};

using Flag_type = std::underlying_type<Flag>::type;
inline Flag_type operator&(Flag_type i, Flag flag)
{
	return i & static_cast<Flag_type>(flag);
}

enum class Platform : char
{
	Microsoft_Windows = 'w',
	Windows_Phone_7 = 'm',
	Xbox_360 = 'x',
};

enum class Profile : uint8_t
{
	Reach = 0,
	HiDef = 1,
};

class XNB
{
	public:
		explicit XNB(BinaryReader& br);
		~XNB();

		std::vector<std::pair<std::string, int32_t>> type_readers;
		std::vector<std::shared_ptr<Content::ContentBase>> objects;
		Platform platform;

	private:
		void read(BinaryReader& reader);
		std::shared_ptr<Content::ContentBase> read_object(BinaryReader& reader);
		static std::unique_ptr<uint8_t[]> decompress(std::unique_ptr<uint8_t[]> compressed, const uint_fast64_t compressed_size, const uint_fast64_t decompressed_size);
};

} // namespace XNB
} // namespace XNA