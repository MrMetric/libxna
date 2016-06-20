#pragma once

#include <BinaryReader.hpp>
#include <BinaryWriter.hpp>
#include <vector>
#include <memory>

namespace XNA {
namespace Content {

class Texture2D;
class Sound;

class ContentBase
{
	public:
		std::string get_type_reader_name();

		static std::shared_ptr<ContentBase> Read(BinaryReader& reader, const std::string& type_reader_name);

	protected:
		std::string type_reader_name;
		ContentBase(){}
};

enum class Texture2D_SurfaceFormat : int32_t
{
	RGBA8888 = 0,
	BGR565 = 1,
	BGRA5551 = 2,
	BGRA4444 = 3,
	DXT1 = 4,
	DXT3 = 5,
	DXT5 = 6,
	NormalizedByte2 = 7,
	NormalizedByte4 = 8,
	RGBA1010102 = 9,
	RG32 = 10,
	RGBA64 = 11,
	Alpha8 = 12,
	Single = 13,
	Vector2 = 14,
	Vector4 = 15,
	HalfSingle = 16,
	HalfVector2 = 17,
	HalfVector4 = 18,
	HdrBlendable = 19,
};
std::string to_string(Texture2D_SurfaceFormat);

class Texture2D : public ContentBase
{
	public:
		explicit Texture2D(BinaryReader& reader);

		std::vector<uint8_t> get_mip_data(uint_fast32_t i);
		std::pair<uint32_t, uint32_t> get_mip_size(uint_fast32_t i);

		void write(BinaryWriter& writer);

	private:
		void read(BinaryReader& reader);

		std::vector<std::vector<uint8_t>> mips;
		uint32_t width;
		uint32_t height;
		Texture2D_SurfaceFormat surface_format;
};

enum class SoundFormat : uint16_t
{
	PCM = 1,
	ADPCM = 2,
};
std::string to_string(SoundFormat);

class Sound : public ContentBase
{
	public:
		explicit Sound(BinaryReader& reader);

		uint16_t channel_count;
		uint32_t sample_rate;
		uint32_t average_byte_rate;
		uint16_t block_align;
		uint16_t bits_per_sample;
		std::vector<uint8_t> data;
		SoundFormat format;
		uint32_t loop_start; // bytes
		uint32_t loop_length; // bytes
		uint32_t loop_duration; // milliseconds

		void write(BinaryWriter& writer);

	private:
		void read(BinaryReader& reader);
};

class SpriteFont : public ContentBase
{
	public:
		explicit SpriteFont(BinaryReader& reader);

	private:
		void read(BinaryReader& reader);
};

} // namespace Content
} // namespace XNA
