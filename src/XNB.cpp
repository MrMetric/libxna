#include "../include/XNB.hpp"

#include <BinaryWriter.hpp>
#include "../include/LzxDecoder.hpp"

#include <algorithm> // std::copy_n

namespace XNA {
namespace XNB {

XNB::XNB(BinaryReader& reader)
{
	this->read(reader);
}

XNB::~XNB()
{
}

void XNB::read(BinaryReader& reader)
{
	if(reader.GetFileSize() < 14)
	{
		throw std::string("file is too small to be XNB format");
		// 3: magic
		// 1: platform
		// 1: XNA version
		// 1: flags
		// 4: file length
		// if compressed: 4: decompressed length
		// etc
		// TODO: determine higher minimum
	}

	std::string format = reader.ReadString(3);
	if(format != "XNB")
	{
		throw ("Invalid format: " + format);
	}

	int8_t platform = reader.ReadInt8();
	this->platform = static_cast<Platform>(platform);

	uint8_t xna_version = reader.ReadUInt8();
	// 5 = XNA Game Studio 4.0
	if(xna_version != 5)
	{
		throw ("Unhandled XNA version: " + std::to_string(xna_version));
	}

	uint8_t flags = reader.ReadUInt8();

	bool compressed = (flags & XNA::XNB::Flag::compressed) != 0;
	uint32_t file_length = reader.ReadUInt32();
	if(file_length != reader.GetFileSize())
	{
		throw ("File length mismatch: " + std::to_string(file_length) + " should be " + std::to_string(reader.GetFileSize()));
	}

	if(compressed)
	{
		uint_fast64_t read_length = file_length - 14;
		uint_fast64_t decompressed_size = reader.ReadUInt32();
		std::unique_ptr<uint8_t[]> compressed_data = reader.ReadBytes(read_length);
		std::unique_ptr<uint8_t[]> decompressed_data = XNB::decompress(std::move(compressed_data), read_length, decompressed_size);
		reader.ChangeFile(std::move(decompressed_data), decompressed_size);
	}
	else
	{
		uint_fast64_t read_length = file_length - 10;
		reader.ChangeFile(reader.ReadBytes(read_length), read_length);
	}

	uint_fast64_t type_count = reader.Read7BitEncodedInt();
	for(uint_fast64_t i = 0; i < type_count; ++i)
	{
		std::string type_reader_name = reader.ReadStringMS();
		int32_t type_reader_version = reader.ReadInt32();
		std::pair<std::string, int32_t> type_reader = std::make_pair(type_reader_name, type_reader_version);
		this->type_readers.push_back(type_reader);
	}

	uint_fast64_t shared_resource_count = reader.Read7BitEncodedInt();
	if(shared_resource_count > UINT32_MAX)
	{
		throw ("XNB::read: too many shared resources (" + std::to_string(shared_resource_count) + ")");
	}
	// there is 1 primary asset before the shared resources
	uint_fast64_t object_count = shared_resource_count + 1;

	for(uint_fast64_t i = 0; i < object_count; ++i)
	{
		this->objects.push_back(this->read_object(reader));
	}
}

std::shared_ptr<XNA::Content::ContentBase> XNB::read_object(BinaryReader& reader)
{
	// perhaps 64 bits is overkill, but conservative guesses tend to bite someone in the ass in the future
	uint_fast64_t type_id = reader.Read7BitEncodedInt();
	if(type_id == 0)
	{
		return nullptr;
	}

	if(type_id > this->type_readers.size())
	{
		throw ("type id is too high (" + std::to_string(type_id) + " > " + std::to_string(this->type_readers.size()) + ")");
	}

	type_id -= 1;

	std::pair<std::string, int32_t> type_reader_info = this->type_readers[type_id];
	std::string type_reader_qualified = type_reader_info.first;
	//int32_t type_reader_version = type_reader_info.second;
	std::string type_reader_name = type_reader_qualified.substr(0, type_reader_qualified.find(','));

	return XNA::Content::ContentBase::Read(reader, type_reader_name);
}

std::unique_ptr<uint8_t[]> XNB::decompress(std::unique_ptr<uint8_t[]> compressed, const uint_fast64_t compressed_size, const uint_fast64_t decompressed_size)
{
	BinaryReader reader(std::move(compressed), compressed_size);

	std::unique_ptr<uint8_t[]> xnbData(new uint8_t[decompressed_size]);
	uint_fast32_t out_position = 0;

	LzxDecoder dec(16); // window = 16 bits, window size = 65536 bytes
	uint_fast32_t pos = 0;
	while(pos < compressed_size)
	{
		uint_fast8_t hi = reader.ReadUInt8();
		uint_fast8_t lo = reader.ReadUInt8();
		uint_fast16_t frame_size;
		uint_fast16_t block_size;
		if(hi == 0xFF)
		{
			hi = lo;
			lo = reader.ReadUInt8();
			frame_size = static_cast<uint_fast16_t>((hi << 8) | lo);

			hi = reader.ReadUInt8();
			lo = reader.ReadUInt8();
			block_size = static_cast<uint_fast16_t>((hi << 8) | lo);

			pos += 5;
		}
		else
		{
			frame_size = 0x8000; // 32768
			block_size = static_cast<uint_fast16_t>((hi << 8) | lo);
			pos += 2;
		}

		if(block_size == 0 || frame_size == 0)
		{
			break;
		}
		if(frame_size > decompressed_size - out_position)
		{
			throw std::string("XNB::decompress: bad data (frame size > decompressed size - output position)");
		}

		std::unique_ptr<uint8_t[]> inBuf = reader.ReadBytes(block_size);
		dec.Decompress(inBuf.get(), block_size, xnbData.get() + out_position, frame_size);
		out_position += frame_size;

		pos += block_size;
	}

	if(out_position != decompressed_size)
	{
		throw ("XNB::decompress: final output position (" + std::to_string(out_position) + ") does not match expected size (" + std::to_string(decompressed_size) + ")");
	}

	return xnbData;
}

} // namespace XNB
} // namespace XNA