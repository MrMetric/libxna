#include <BinaryReader.hpp>
#include <BinaryWriter.hpp>
#include <sstream>
#include <iostream>
#include <XNB.hpp>
#include <Content.hpp>
#include <png++/png.hpp>

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
			png::image<png::rgba_pixel> image(width, height);
			for(uint_fast32_t x = 0; x < width; ++x)
			{
				for(uint_fast32_t y = 0; y < height; ++y)
				{
					uint_fast32_t i = (y * width + x) * 4;
					image.set_pixel(x, y, png::rgba_pixel(mip[i], mip[i + 1], mip[i + 2], mip[i + 3]));
				}
			}
			image.write(outname);
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