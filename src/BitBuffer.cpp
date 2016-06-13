// This file uses code from LzxDecoder.cs in MonoGame. The following is from the beginning of the file:
//#region HEADER
/* This file was derived from libmspack
 * (C) 2003-2004 Stuart Caie.
 * (C) 2011 Ali Scissons.
 *
 * The LZX method was created by Jonathan Forbes and Tomi Poutanen, adapted
 * by Microsoft Corporation.
 *
 * This source file is Dual licensed; meaning the end-user of this source file
 * may redistribute/modify it under the LGPL 2.1 or MS-PL licenses.
 */
//#region LGPL License
/* GNU LESSER GENERAL PUBLIC LICENSE version 2.1
 * LzxDecoder is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License (LGPL) version 2.1
 */
//#endregion
//#region MS-PL License
/*
 * MICROSOFT PUBLIC LICENSE
 * This source code is subject to the terms of the Microsoft Public License (Ms-PL).
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * is permitted provided that redistributions of the source code retain the above
 * copyright notices and this file header.
 *
 * Additional copyright notices should be appended to the list above.
 *
 * For details, see <http://www.opensource.org/licenses/ms-pl.html>.
 */
//#endregion
/*
 * This derived work is recognized by Stuart Caie and is authorized to adapt
 * any changes made to lzxd.c in his libmspack library and will still retain
 * this dual licensing scheme. Big thanks to Stuart Caie!
 *
 * DETAILS
 * This file is a pure C# port of the lzxd.c file from libmspack, with minor
 * changes towards the decompression of XNB files. The original decompression
 * software of LZX encoded data was written by Suart Caie in his
 * libmspack/cabextract projects, which can be located at
 * http://http://www.cabextract.org.uk/
 */
//#endregion

#include "BitBuffer.hpp"

#include <algorithm>

BitBuffer::BitBuffer(const uint8_t* inBuf, uint_fast32_t inlen)
{
	this->buffer = 0;
	this->bitsleft = 0;
	this->inpos = 0;

	this->inBuf = inBuf;
	this->inlen = inlen;
}

void BitBuffer::EnsureBits(uint8_t bits)
{
	while(this->bitsleft < bits)
	{
		uint_fast16_t i = this->ReadUInt16();
		uint_fast8_t amount2shift = sizeof(uint32_t)*8 - 16 - bitsleft;
		this->buffer |= static_cast<uint32_t>(i << amount2shift);
		this->bitsleft += 16;
	}
}

uint32_t BitBuffer::PeekBits(uint8_t bits) const
{
	return (this->buffer >> ((sizeof(uint32_t)*8) - bits));
}

void BitBuffer::RemoveBits(uint8_t bits)
{
	this->buffer <<= bits;
	this->bitsleft -= bits;
}

uint32_t BitBuffer::ReadBits(uint8_t bits)
{
	uint32_t ret = 0;

	if(bits > 0)
	{
		this->EnsureBits(bits);
		ret = this->PeekBits(bits);
		this->RemoveBits(bits);
	}

	return ret;
}

uint16_t BitBuffer::ReadUInt16()
{
	return this->ReadType<uint16_t>();
}

uint32_t BitBuffer::ReadUInt32()
{
	return this->ReadType<uint32_t>();
}

//#include <iostream>
template <class type> type BitBuffer::ReadType()
{
	uint_fast32_t maxpos = this->inpos + sizeof(type) - 1;
	//std::cout << "read " << sizeof(type) << " bytes at " << this->inpos << " with bufsize " << this->inlen << "\n";
	if(maxpos < this->inlen)
	{
		uint8_t buf[sizeof(type)];
		std::copy_n(this->inBuf + this->inpos, sizeof(type), buf);
		this->inpos += sizeof(type);
		return *reinterpret_cast<type*>(buf);
	}
	//throw std::string("tried to read " + std::to_string(sizeof(type) + this->inlen - this->inpos) + " past end of buffer");
	return 0;
}
