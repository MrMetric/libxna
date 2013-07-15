/*
This file uses code from MonoGame. The license is as follows:

Microsoft Public License (Ms-PL)
MonoGame - Copyright © 2009 The MonoGame Team

All rights reserved.

This license governs use of the accompanying software. If you use the software, you accept this license. If you do not
accept the license, do not use the software.

1. Definitions
The terms "reproduce," "reproduction," "derivative works," and "distribution" have the same meaning here as under
U.S. copyright law.

A "contribution" is the original software, or any additions or changes to the software.
A "contributor" is any person that distributes its contribution under this license.
"Licensed patents" are a contributor's patent claims that read directly on its contribution.

2. Grant of Rights
(A) Copyright Grant- Subject to the terms of this license, including the license conditions and limitations in section 3,
each contributor grants you a non-exclusive, worldwide, royalty-free copyright license to reproduce its contribution, prepare derivative works of its contribution, and distribute its contribution or any derivative works that you create.
(B) Patent Grant- Subject to the terms of this license, including the license conditions and limitations in section 3,
each contributor grants you a non-exclusive, worldwide, royalty-free license under its licensed patents to make, have made, use, sell, offer for sale, import, and/or otherwise dispose of its contribution in the software or derivative works of the contribution in the software.

3. Conditions and Limitations
(A) No Trademark License- This license does not grant you rights to use any contributors' name, logo, or trademarks.
(B) If you bring a patent claim against any contributor over patents that you claim are infringed by the software,
your patent license from such contributor to the software ends automatically.
(C) If you distribute any portion of the software, you must retain all copyright, patent, trademark, and attribution
notices that are present in the software.
(D) If you distribute any portion of the software in source code form, you may do so only under this license by including
a complete copy of this license with your distribution. If you distribute any portion of the software in compiled or object
code form, you may only do so under a license that complies with this license.
(E) The software is licensed "as-is." You bear the risk of using it. The contributors give no express warranties, guarantees
or conditions. You may have additional consumer rights under your local laws which this license cannot change. To the extent
permitted under your local laws, the contributors exclude the implied warranties of merchantability, fitness for a particular
purpose and non-infringement.
*/

#include "../include/XNAconverter.hpp"
#include "../include/LzxDecoder.hpp"
#include <string.h>
#include <stdint.h>
#include <malloc.h>
#include <string>
#include <iostream>
#include <png.h>
#include <BinaryWriter.hpp>
#include <sstream> // for std::stringstream

#define MAKESTR(ss) static_cast<std::ostringstream&>(std::ostringstream().seekp(0) << ss).str()

/** XNAconverter.cpp
	This class converts XNA-formatted files to a standard format
	Functions:
		readHeader	Reads the XNB header
		readXNB		Reads the XNB file. If the file is compressed, this function will decompress it.
		XNB2WAV		Converts an XNB sound to WAV
		XNB2PNG		Converts an XNB image to PNG
		getTypeName	Returns something like "Audio" or "Image" when given a type reader string
*/

/** readHeader
	Reads the XNB header

	@arg br			A BinaryReader instance
	@arg compressed	The compression flag is put in this variable
	@arg fileLength	The file length is put in this variable
*/
void XNAconverter::readHeader(BinaryReader *br, bool& compressed, uint32_t& fileLength)
{
	std::string format = br->ReadString(3);
	if(format.compare("XNB") != 0)
	{
		throw MAKESTR("Invalid format: " << format);
	}

	int8_t platform = br->ReadInt8();
	if(platform != 'w')
	{
		throw MAKESTR("Unhandled platform: " << (int)platform);
	}

	uint8_t xnaVersion = br->ReadUInt8();
	if(xnaVersion != 5)
	{
		throw MAKESTR("Unhandled version: " << (int)xnaVersion);
	}

	uint8_t flags = br->ReadUInt8();
	if(flags != 0x00 && flags != 0x80)
	{
		throw MAKESTR("Unknown flags byte: " << (int)flags);
	}

	compressed = (flags & 0x80) != 0;
	fileLength = br->ReadInt32();
	if(fileLength != br->fSize)
	{
		throw MAKESTR("File length mismatch: " << fileLength << " should be " << br->fSize);
	}
}

/**
	readXNB
	Reads the XNB file. If the file is compressed, this function will decompress it.

	@arg br			A BinaryReader instance
	@arg type		The type reader string will be put in this variable
	@arg outputDec	The name of the decompressed XNB (if the input XNB is compressed). No file will be written if this is an empty string.
*/
void XNAconverter::readXNB(BinaryReader* br, std::string& type, std::string outputDec)
{
	bool compressed = false;
	uint32_t fileLength = 0;
	try
	{
		readHeader(br, compressed, fileLength);
	}
	catch(std::string e)
	{
		throw MAKESTR("Invalid XNB header: " << e);
	}

	uint8_t* xnbData;

	if(compressed) // TODO: decompress to memory
	{
		uint_fast32_t compressedSize = fileLength - 14; // 10 bytes (XNB header) + 4 bytes (decompressed size int)
		uint_fast32_t decompressedSize = br->ReadUInt32();
		BinaryWriter* bw = nullptr;
		bool writeDec = (outputDec.compare("") != 0);
		if(writeDec)
		{
			std::stringstream ss;
			ss << br->fname.substr(0, br->fname.length() - 4); // TODO: check if file name has an extension
			outputDec = ss.str() + "_dec.xnb";
			bw = new BinaryWriter(outputDec, true);
			bw->WriteString("XNBw"); // file format and platform
			bw->WriteUInt16(0x0005); // XNA version and flags
			bw->WriteUInt32(decompressedSize + 10); // final file size
		}

		xnbData = new uint8_t[decompressedSize];
		uint_fast32_t outPos = 0;

		LzxDecoder* dec = new LzxDecoder(16); // window = 16 bits, window size = 65536 bytes
		uint_fast32_t decodedBytes = 0;
		uint_fast32_t pos = 0;
		while(pos < compressedSize)
		{
			uint_fast8_t hi = br->ReadUInt8();
			uint_fast8_t lo = br->ReadUInt8();
			uint_fast16_t frame_size;
			uint_fast16_t block_size;
			if(hi == 0xFF)
			{
				hi = lo;
				lo = br->ReadUInt8();
				frame_size = (hi << 8) | lo;

				hi = br->ReadUInt8();
				lo = br->ReadUInt8();
				block_size = (hi << 8) | lo;

				pos += 5;
			}
			else
			{
				frame_size = 0x8000; // 32768
				block_size = (hi << 8) | lo;
				pos += 2;
			}

			if(block_size == 0 || frame_size == 0)
			{
				break;
			}
			if(block_size > 0x10000 || frame_size > 0x10000)
			{
				br->Close();
				delete br;

				if(writeDec)
				{
					bw->Close();
					delete bw;
				}

				delete dec;

				throw std::string("Error decompressing content data");
			}

			try
			{
				uint8_t* inBuf = new uint8_t[block_size];
				uint8_t* outBuf = new uint8_t[frame_size];

				// this gets the wrong data (possibly a problem with conversion from char to unsigned char
				//const char* inData = br->ReadString(block_size).c_str();
				//memcpy(inBuf, inData, block_size);

				// TODO: do this without a loop
				for(uint_fast32_t i = 0; i < block_size; ++i)
				{
					inBuf[i] = br->ReadUInt8();
				}
				dec->Decompress(inBuf, block_size, outBuf, frame_size);
				LzxDecoder::ArrayCopy(outBuf, 0, xnbData, outPos, frame_size);
				outPos += frame_size;

				delete inBuf;
				delete outBuf;
			}
			catch(std::string e)
			{
				std::string error = MAKESTR("Error while decompressing data (" << e << ")");

				delete dec;

				br->Close();
				delete br;

				bw->Close();
				delete bw;

				remove(outputDec.c_str());

				throw error;
			}

			pos += block_size;
			decodedBytes += frame_size;
		}

		delete dec;
		if(outPos != decompressedSize)
		{
			std::string error = MAKESTR("Decompression of " << br->fname << " failed: " << outPos << " should be " << decompressedSize + 10);

			if(writeDec)
			{
				bw->Close();
				delete bw;
			}

			remove(outputDec.c_str());

			throw error;
		}
		else if(writeDec)
		{
			bw->WriteBytes(xnbData, decompressedSize, decompressedSize);
			bw->Close();
			delete bw;
		}
		br->ChangeFile(xnbData, decompressedSize);
	}
	else
	{
		uint_fast32_t size = fileLength - 10;
		xnbData = br->ReadBytes(size);
		br->ChangeFile(xnbData, size);
	}

	uint32_t typeCount = br->Read7BitEncodedInt();

	type = br->ReadStringMS(); // get the first type reader
	br->pos += 4; // skip the version number

	// loop over the rest
	for(uint_fast32_t t = 1; t < typeCount; ++t)
	{
		std::string ts = br->ReadStringMS();
		//int32_t typeReaderVersion = br->ReadInt32();
		br->pos += 4;
	}

	uint32_t sharedResourcesCount = br->Read7BitEncodedInt();
	if(sharedResourcesCount != 0)
	{
		throw MAKESTR("Too many shared resources: " << sharedResourcesCount);
	}

	uint32_t unknown = br->Read7BitEncodedInt();
	if(unknown != 1)
	{
		throw MAKESTR("Unknown byte is not 1 (" << unknown << ")");
	}
}

/** XNB2WAV
	Derived from http://www.terrariaonline.com/threads/86509/

	@arg xnb The XNB file to read
	@arg wav The WAV file to create
*/
void XNAconverter::XNB2WAV(std::string xnb, std::string wav)
{
	if(BINARYLIB_VERSION != BinaryLibUtil::version())
	{
		throw std::string("BinaryLib header version does not match compiled library version");
	}

	BinaryReader *br = new BinaryReader(xnb);
	std::string type = "";
	try
	{
		readXNB(br, type);
	}
	catch(std::string e)
	{
		throw MAKESTR("Error reading \"" << xnb << "\" (" << e << ")");
	}

	if(type.compare("Microsoft.Xna.Framework.Content.SoundEffectReader") != 0)
	{
		throw MAKESTR("Unhandled type reader: " << type);
	}

	uint32_t formatChunkSize = br->ReadUInt32();
	if(formatChunkSize != 18)
	{
		throw MAKESTR("Wrong format chunk size: " << formatChunkSize);
	}

	uint16_t wFormatTag = br->ReadUInt16();
	if(wFormatTag != 1)
	{
		throw MAKESTR("Unhandled wav codec (must be PCM): " << wFormatTag);
	}

	uint16_t nChannels = br->ReadUInt16();
	uint32_t nSamplesPerSec = br->ReadInt32();
	uint32_t nAvgBytesPerSec = br->ReadInt32();
	uint16_t nBlockAlign = br->ReadInt16();
	uint16_t wBitsPerSample = br->ReadInt16();
	if(nAvgBytesPerSec != (nSamplesPerSec * nChannels * (wBitsPerSample / 8)))
	{
		throw std::string("Average bytes per second number incorrect");
	}

	if(nBlockAlign != (nChannels * (wBitsPerSample / 8)))
	{
		throw std::string("Block align number incorrect");
	}

	br->ReadInt16();
	int_fast32_t dataChunkSize = br->ReadInt32();

	BinaryWriter *bw = new BinaryWriter(wav, false);
	bw->WriteString("RIFF");
	bw->WriteInt32(dataChunkSize + 36);
	bw->WriteString("WAVEfmt ");
	bw->WriteInt32(16);
	bw->WriteUInt16(wFormatTag);
	bw->WriteUInt16(nChannels);
	bw->WriteUInt32(nSamplesPerSec);
	bw->WriteUInt32(nAvgBytesPerSec);
	bw->WriteUInt16(nBlockAlign);
	bw->WriteUInt16(wBitsPerSample);
	bw->WriteString("data");
	bw->WriteInt32(dataChunkSize);

	std::string dataChunk = br->ReadString(dataChunkSize);
	bw->WriteString(dataChunk);

	br->Close();
	delete br;

	bw->Close();
	delete bw;
}

/** XNB2PNG

	@arg xnb	The XNB file to read
	@arg wav	The PNG file to create
*/
void XNAconverter::XNB2PNG(std::string xnb, std::string png)
{
	uint32_t tmp1;
	uint32_t tmp2;
	XNAconverter::XNB2PNG(xnb, png, tmp1, tmp2);
}

/**
	XNB2PNG

	@arg xnb	The XNB file to read
	@arg wav	The PNG file to create
	@arg width	The image width is put in this variable
	@arg height	The image height is put in this variable
*/
void XNAconverter::XNB2PNG(std::string xnb, std::string png, uint32_t& width, uint32_t& height)
{
	if(BINARYLIB_VERSION != BinaryLibUtil::version())
	{
		throw std::string("BinaryLib header version does not match compiled library version");
	}

	BinaryReader *br = new BinaryReader(xnb);
	std::string type = "";
	try
	{
		readXNB(br, type);
	}
	catch(std::string e)
	{
		throw MAKESTR("Error reading \"" << xnb << "\" (" << e << ")");
	}

	if(type.compare("Microsoft.Xna.Framework.Content.Texture2DReader, Microsoft.Xna.Framework.Graphics, Version=4.0.0.0, Culture=neutral, PublicKeyToken=842cf8be1de50553") != 0)
	{
		throw MAKESTR("Unhandled type reader: " << type);
	}

	int_fast32_t surfaceFormat = br->ReadInt32();
	switch(surfaceFormat) // TODO: convert other formats to RGBA8888
	{
		case SURFACEFORMAT_RGBA8888:
		{
			break;
		}
		case SURFACEFORMAT_BGR565:
		{
			throw std::string("Unsupported image format: BGR565");
		}
		case SURFACEFORMAT_BGRA5551:
		{
			throw std::string("Unsupported image format: BGRA5551");
		}
		case SURFACEFORMAT_BGRA4444:
		{
			throw std::string("Unsupported image format: BGRA4444");
		}
		case SURFACEFORMAT_DXT1:
		{
			throw std::string("Unsupported image format: DXT1");
		}
		case SURFACEFORMAT_DXT3:
		{
			throw std::string("Unsupported image format: DXT3");
		}
		case SURFACEFORMAT_DXT5:
		{
			throw std::string("Unsupported image format: DXT5");
		}
		default:
		{
			throw MAKESTR("Unknown image format: " << surfaceFormat);
		}
	}

	width = br->ReadUInt32();
	height = br->ReadUInt32();

	// What is this?
	uint32_t levelCount = br->ReadUInt32();
	if(levelCount != 1)
	{
		throw MAKESTR("Unhandled level count: " << levelCount);
	}

	uint32_t imageLength = br->ReadUInt32();
	char imageData[imageLength];
	for(uint_fast32_t i = 0; i < imageLength; ++i)
	{
		imageData[i] = br->ReadUInt8();
	}
	br->Close();
	delete br;

	// from http://www.labbookpages.co.uk/software/imgProc/libPNG.html
	FILE *fp = fopen(png.c_str(), "wb");
	if(fp == NULL)
	{
		throw MAKESTR("Error opening " << png << " for writing");
	}
	png_structp pngptr;
	png_infop pnginfo;
	pngptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if(pngptr == NULL)
	{
		fclose(fp);
		throw std::string("Error creating PNG Write Struct");
	}
	pnginfo = png_create_info_struct(pngptr);
	if(pnginfo == NULL)
	{
		fclose(fp);
		png_free_data(pngptr, pnginfo, PNG_FREE_ALL, -1);
		png_destroy_write_struct(&pngptr, (png_infopp)NULL);
		throw std::string("Error creating PNG Info Struct");
	}
	png_init_io(pngptr, fp);
	png_set_IHDR(pngptr, pnginfo, width, height, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
	png_write_info(pngptr, pnginfo);
	uint_fast32_t rowsize = 4 * width * sizeof(png_byte);
	png_bytep row = (png_bytep)malloc(rowsize);
	uint_fast32_t offset = 0;
	for(uint_fast32_t y = 0; y < height; ++y)
	{
		for(uint_fast32_t x = 0; x < width; ++x)
		{
			png_byte *ptr = &row[x * 4];
			for(uint_fast8_t o = 0; o < 4; ++o)
			{
				ptr[o] = imageData[offset];
				//std::string val = (o == 0?"R":(o == 1?"G":(o == 2?"B":(o == 3?"A":"?"))));
				++offset;
			}
		}
		png_write_row(pngptr, row);
	}
	png_write_end(pngptr, NULL);
	fclose(fp);
	png_free_data(pngptr, pnginfo, PNG_FREE_ALL, -1);
	png_destroy_write_struct(&pngptr, (png_infopp)NULL);
	free(row);
}

std::string XNAconverter::getTypeName(std::string type)
{
	if(type.compare("Microsoft.Xna.Framework.Content.SoundEffectReader") == 0)
	{
		return "Audio";
	}
	if(type.compare("Microsoft.Xna.Framework.Content.Texture2DReader, Microsoft.Xna.Framework.Graphics, Version=4.0.0.0, Culture=neutral, PublicKeyToken=842cf8be1de50553") == 0)
	{
		return "Image";
	}
	if(type.compare("Microsoft.Xna.Framework.Content.SpriteFontReader, Microsoft.Xna.Framework.Graphics, Version=4.0.0.0, Culture=neutral, PublicKeyToken=842cf8be1de50553") == 0)
	{
		return "Font";
	}
	return type;
}
