#include <BinaryReader.hpp>
#include <BinaryWriter.hpp>
#include <sstream>
#include <iostream>
#include <XNB.hpp>
#include <Content.hpp>
#include <png.h>

void write_png_RGBA(const char* filename, uint8_t* buf, uint_fast32_t width, uint_fast32_t height)
{
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	if(png_ptr == nullptr)
	{
		throw std::string("png_create_write_struct returned null");
	}
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if(info_ptr == nullptr)
	{
		png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
		png_destroy_write_struct(&png_ptr, nullptr);
		throw std::string("png_create_info_struct returned null");
	}
	FILE* fp = fopen(filename, "wb");
	if(fp == nullptr)
	{
		throw std::string("error opening png file: ") + strerror(errno);
	}
	png_init_io(png_ptr, fp);
	const int bit_depth = 8;
	png_set_IHDR(png_ptr, info_ptr, width, height, bit_depth, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
	png_write_info(png_ptr, info_ptr);
	uint_fast32_t rowsize = 4 * width * sizeof(png_byte);

	/*
	for(uint_fast32_t y = 0; y < height; ++y)
	{
		png_write_row(png_ptr, buf + y * rowsize);
	}
	*/

	png_byte** row_pointers = new png_byte*[height];
	for(int y = 0; y < height; ++y)
	{
		row_pointers[y] = buf + y * rowsize;
	}
	png_write_image(png_ptr, row_pointers);
	delete[] row_pointers;

	png_write_end(png_ptr, info_ptr);
	fclose(fp);
	png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
	png_destroy_write_struct(&png_ptr, &info_ptr);
}

int main(int argc, char** argv)
{
	if(argc <= 1)
	{
		std::cout << "usage: " << argv[0] << " <input file> [output file]\n";
		return EXIT_FAILURE;
	}

	std::string filename(argv[1]);
	std::string outname(argc > 2 ? argv[2] : "");
	try
	{
		BinaryReader reader(filename);
		XNA::XNB::XNB xnb(reader);
		std::shared_ptr<XNA::Content::ContentBase> content = xnb.objects[0];

		std::string type_reader_name = content->get_type_reader_name();
		if(type_reader_name == "Microsoft.Xna.Framework.Content.Texture2DReader")
		{
			std::shared_ptr<XNA::Content::Texture2D> tex = std::static_pointer_cast<XNA::Content::Texture2D>(content);
			if(outname == "")
			{
				outname = filename + ".png";
			}

			std::vector<uint8_t> mip = tex->get_mip_data(0);
			std::pair<uint32_t, uint32_t> mip_size = tex->get_mip_size(0);
			uint32_t width = mip_size.first;
			uint32_t height = mip_size.second;
			write_png_RGBA(outname.c_str(), &mip[0], width, height);
		}
		else if(type_reader_name == "Microsoft.Xna.Framework.Content.SoundEffectReader")
		{
			std::shared_ptr<XNA::Content::Sound> sound = std::static_pointer_cast<XNA::Content::Sound>(content);
			if(outname == "")
			{
				outname = filename + ".wav";
			}

			BinaryWriter writer(outname);
			writer.WriteString("RIFF");
			writer.WriteUInt32(sound->data.size() + 36);
			writer.WriteString("WAVEfmt ");
			writer.WriteUInt32(16);
			writer.WriteUInt16(static_cast<uint16_t>(sound->format));
			writer.WriteUInt16(sound->channel_count);
			writer.WriteUInt32(sound->sample_rate);
			writer.WriteUInt32(sound->average_byte_rate);
			writer.WriteUInt16(sound->block_align);
			writer.WriteUInt16(sound->bits_per_sample);
			writer.WriteString("data");
			writer.WriteUInt32(sound->data.size());
			writer.WriteBytes(sound->data);
		}
		else
		{
			throw ("unhandled type reader name: " + type_reader_name);
		}
		std::cout << filename << ": wrote " << outname << "\n";
	}
	catch(const std::string& e)
	{
		std::cerr << filename << ": " << e << "\n";
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}