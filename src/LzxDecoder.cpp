/** This file uses code from LzxDecoder.cs in MonoGame. The following is from the beginning of the file: **/
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

#include "LzxDecoder.hpp"

#include <algorithm>	// std::copy_n, std::fill_n
#include <string>

#include "xna_exception.hpp"

LzxDecoder::LzxDecoder(const uint_fast16_t window_bits)
{
	// LZX supports window sizes of 2^15 (32 KiB) to 2^21 (2 MiB)
	if(window_bits < 15 || window_bits > 21)
	{
		throw lzx_error("LzxDecoder: unsupported window size exponent: " + std::to_string(window_bits));
	}

	this->state.window_size = 1 << window_bits;

	// let's initialize our state
	this->state.window = new uint8_t[this->state.window_size];
	std::fill_n(this->state.window, this->state.window_size, 0xDC);
	this->state.window_posn = 0;

	// initialize tables
	for(uint_fast32_t i = 0, j = 0; i <= 50; i += 2)
	{
		this->extra_bits[i] = this->extra_bits[i + 1] = static_cast<uint8_t>(j);
		if((i != 0) && (j < 17))
		{
			++j;
		}
	}
	for(uint_fast32_t i = 0, j = 0; i <= 50; ++i)
	{
		this->position_base[i] = j;
		j += 1 << this->extra_bits[i];
	}

	uint_fast32_t posn_slots;
	if(window_bits == 20)
	{
		posn_slots = 42;
	}
	else if(window_bits == 21)
	{
		// note for future me: this 50 is likely related to the 50*8 in the MAINTREE_MAXSYMBOLS definition (see posn_slots * 8 below)
		posn_slots = 50;
	}
	else
	{
		posn_slots = window_bits * 2;
	}

	// reset state
	this->state.R0 = this->state.R1 = this->state.R2 = 1;
	this->state.main_elements = static_cast<uint16_t>(NUM_CHARS + (posn_slots * 8));
	this->state.header_read = false;
	this->state.block_remaining = 0;
	this->state.block_type = BLOCKTYPE::INVALID;

	// initialize tables to 0 (because deltas will be applied to them)
	this->state.MAINTREE_len.fill(0);
	this->state.LENGTH_len.fill(0);
}

LzxDecoder::~LzxDecoder()
{
	delete[] this->state.window;
}

void copy_n_safe(uint8_t* buf, uint_fast32_t len, uint_fast32_t src, uint_fast32_t& dest)
{
	if(src == dest)
	{
		return;
	}

	uint8_t* bufsrc = buf + src;
	uint8_t* bufdest = buf + dest;
	if((dest > src) && (src + len >= dest))
	{
		uint_fast32_t distance = dest - src;
		uint_fast32_t copies = len / distance;
		uint_fast32_t leftover = len % distance;
		for(uint_fast32_t i = 0; i < copies; ++i)
		{
			std::copy_n(bufsrc, distance, bufdest);
			bufdest += distance;
		}
		std::copy_n(bufsrc, leftover, bufdest);
	}
	else // overlap does not matter
	{
		std::copy_n(bufsrc, len, bufdest);
	}
	dest += len;
}

void LzxDecoder::Decompress(const uint8_t* inBuf, const uint_fast32_t inLen, uint8_t* outBuf, const uint_fast32_t outLen)
{
	BitBuffer bitbuf(inBuf, inLen);

	uint_fast32_t window_posn = this->state.window_posn;
	const uint_fast32_t window_size = this->state.window_size;
	uint_fast32_t R0 = this->state.R0;
	uint_fast32_t R1 = this->state.R1;
	uint_fast32_t R2 = this->state.R2;

	// read header if necessary
	if(!this->state.header_read)
	{
		const uint32_t intel = bitbuf.ReadBits(1);
		if(intel != 0)
		{
			throw lzx_error("LzxDecoder::Decompress: Intel E8 not supported");
		}
		this->state.header_read = true;
	}

	// main decoding loop
	uint_fast32_t togo = outLen;
	while(togo > 0)
	{
		// last block finished, new block expected
		if(this->state.block_remaining == 0)
		{
			this->state.block_type = static_cast<BLOCKTYPE>(bitbuf.ReadBits(3));

			const uint32_t hi = bitbuf.ReadBits(16);
			const uint32_t lo = bitbuf.ReadBits(8);
			this->state.block_remaining = this->state.block_length = static_cast<uint32_t>((hi << 8) | lo);

			switch(this->state.block_type)
			{
				case BLOCKTYPE::ALIGNED:
				{
					for(uint_fast32_t i = 0; i < 8; ++i)
					{
						this->state.ALIGNED_len[i] = static_cast<uint8_t>(bitbuf.ReadBits(3));
					}
					this->MakeDecodeTable(ALIGNED_MAXSYMBOLS, ALIGNED_TABLEBITS, this->state.ALIGNED_len.data(), this->state.ALIGNED_table.data());
					// rest of aligned header is same as verbatim
					#ifdef __clang__
					[[clang::fallthrough]];
					#endif
				}

				case BLOCKTYPE::VERBATIM:
				{
					this->ReadLengths(this->state.MAINTREE_len.data(), 0, 256, bitbuf);
					this->ReadLengths(this->state.MAINTREE_len.data(), 256, this->state.main_elements, bitbuf);
					this->MakeDecodeTable(MAINTREE_MAXSYMBOLS, MAINTREE_TABLEBITS, this->state.MAINTREE_len.data(), this->state.MAINTREE_table.data());

					this->ReadLengths(this->state.LENGTH_len.data(), 0, NUM_SECONDARY_LENGTHS, bitbuf);
					this->MakeDecodeTable(LENGTH_MAXSYMBOLS, LENGTH_TABLEBITS, this->state.LENGTH_len.data(), this->state.LENGTH_table.data());
					break;
				}

				case BLOCKTYPE::UNCOMPRESSED:
				{
					if(bitbuf.bitsleft == 0)
					{
						bitbuf.EnsureBits(16);
					}
					R0 = bitbuf.ReadUInt32();
					R1 = bitbuf.ReadUInt32();
					R2 = bitbuf.ReadUInt32();
					break;
				}

				case BLOCKTYPE::INVALID:
				default:
				{
					throw lzx_error("LzxDecoder::Decompress: invalid state block type:  " + to_string(this->state.block_type));
				}
			}
		}

		// buffer exhaustion check
		if(bitbuf.inpos > inLen)
		{
			/*
			it's possible to have a file where the next run is less than 16 bits in size. In this case, the READ_HUFFSYM() macro used in building
			the tables will exhaust the buffer, so we should allow for this, but not allow those accidentally read bits to be used
			(so we check that there are at least 16 bits remaining - in this boundary case they aren't really part of the compressed data)
			*/
			if(bitbuf.inpos > (inLen + 2) || bitbuf.bitsleft < 16)
			{
				throw lzx_error("LzxDecoder::Decompress: invalid data");
			}
		}

		uint_fast32_t this_run;
		while((this_run = this->state.block_remaining) > 0 && togo > 0)
		{
			if(this_run > togo)
			{
				this_run = togo;
			}
			togo -= this_run;
			this->state.block_remaining -= this_run;

			// apply 2^x-1 mask
			window_posn &= window_size - 1;
			// runs can't straddle the window wraparound
			if((window_posn + this_run) > window_size)
			{
				throw lzx_error("LzxDecoder::Decompress: invalid data (window position + this_run > window size)");
			}

			auto verbatim_or_aligned = [this, &this_run, &bitbuf, &window_size, &window_posn, &R0, &R1, &R2](
				std::function<void(uint_fast32_t&)> match_offset_more_than_2
			)
			{
				while(this_run > 0)
				{
					uint32_t main_element = this->ReadHuffSym(this->state.MAINTREE_table.data(), this->state.MAINTREE_len.data(), MAINTREE_MAXSYMBOLS, MAINTREE_TABLEBITS, bitbuf);

					if(main_element < NUM_CHARS)
					{
						// literal: 0 to NUM_CHARS-1
						this->state.window[window_posn++] = static_cast<uint8_t>(main_element);
						--this_run;
					}
					else
					{
						// match: NUM_CHARS + ((slot<<3) | length_header (3 bits))
						main_element -= NUM_CHARS;

						uint_fast32_t match_length = main_element & NUM_PRIMARY_LENGTHS;
						if(match_length == NUM_PRIMARY_LENGTHS)
						{
							uint_fast32_t length_footer = this->ReadHuffSym(this->state.LENGTH_table.data(), this->state.LENGTH_len.data(), LENGTH_MAXSYMBOLS, LENGTH_TABLEBITS, bitbuf);
							match_length += length_footer;
						}
						match_length += MIN_MATCH;

						uint_fast32_t match_offset = main_element >> 3;

						if(match_offset > 2)
						{
							// not repeated offset
							match_offset_more_than_2(match_offset);

							// update repeated offset LRU queue
							R2 = R1;
							R1 = R0;
							R0 = match_offset;
						}
						else if(match_offset == 0)
						{
							match_offset = R0;
						}
						else if(match_offset == 1)
						{
							match_offset = R1;
							R1 = R0;
							R0 = match_offset;
						}
						else // match_offset == 2
						{
							match_offset = R2;
							R2 = R0;
							R0 = match_offset;
						}

						uint_fast32_t runsrc;
						uint_fast32_t rundest = window_posn;

						if(match_length > this_run)
						{
							throw lzx_error("LzxDecoder::Decompress: match_length > this_run (" + std::to_string(match_length) + " > " + std::to_string(this_run) + ")");
						}
						this_run -= match_length;

						// copy any wrapped around source data
						if(window_posn >= match_offset)
						{
							// no wrap
							runsrc = rundest - match_offset;
						}
						else
						{
							runsrc = rundest + (window_size - match_offset);
							uint_fast32_t copy_length = match_offset - window_posn;
							if(copy_length < match_length)
							{
								match_length -= copy_length;
								window_posn += copy_length;
								copy_n_safe(this->state.window, copy_length, runsrc, rundest);
								runsrc = 0;
							}
						}
						window_posn += match_length;

						// copy match data
						copy_n_safe(this->state.window, match_length, runsrc, rundest);
					}
				}
			};

			switch(this->state.block_type)
			{
				case BLOCKTYPE::VERBATIM:
				{
					verbatim_or_aligned([this, &bitbuf](uint_fast32_t& match_offset)
					{
						if(match_offset != 3)
						{
							uint8_t extra = this->extra_bits[match_offset];
							uint_fast32_t verbatim_bits = bitbuf.ReadBits(extra);
							match_offset = this->position_base[match_offset] - 2 + verbatim_bits;
						}
						else
						{
							match_offset = 1;
						}
					});
					break;
				}

				case BLOCKTYPE::ALIGNED:
				{
					verbatim_or_aligned([this, &bitbuf](uint_fast32_t& match_offset)
					{
						uint8_t extra = this->extra_bits[match_offset];
						match_offset = this->position_base[match_offset] - 2;
						if(extra > 3)
						{
							// verbatim and aligned bits
							extra -= 3;
							uint_fast32_t verbatim_bits = bitbuf.ReadBits(extra);
							match_offset += (verbatim_bits << 3);

							uint_fast32_t aligned_bits = this->ReadHuffSym(this->state.ALIGNED_table.data(), this->state.ALIGNED_len.data(), ALIGNED_MAXSYMBOLS, ALIGNED_TABLEBITS, bitbuf);
							match_offset += aligned_bits;
						}
						else if(extra == 3)
						{
							// aligned bits only
							uint_fast32_t aligned_bits = this->ReadHuffSym(this->state.ALIGNED_table.data(), this->state.ALIGNED_len.data(), ALIGNED_MAXSYMBOLS, ALIGNED_TABLEBITS, bitbuf);
							match_offset += aligned_bits;
						}
						else if(extra > 0) // extra==1, extra==2
						{
							// verbatim bits only
							uint_fast32_t verbatim_bits = bitbuf.ReadBits(extra);
							match_offset += verbatim_bits;
						}
						else // extra == 0
						{
							// ???
							match_offset = 1;
						}
					});
					break;
				}

				case BLOCKTYPE::UNCOMPRESSED:
				{
					if((bitbuf.inpos + this_run) > inLen)
					{
						throw lzx_error("LzxDecoder::Decompress: invalid data (bitbuf.inpos + this_run > endpos)");
					}

					std::copy_n(inBuf + bitbuf.inpos, this_run, this->state.window + window_posn);
					bitbuf.inpos += this_run;
					window_posn += this_run;
					break;
				}

				case BLOCKTYPE::INVALID:
				default:
				{
					throw lzx_error("LzxDecoder::Decompress: invalid state block type: " + to_string(this->state.block_type));
				}
			}
		}
	}
	if(togo != 0)
	{
		throw lzx_error("LzxDecoder::Decompress: togo != 0\n");
	}

	uint_fast32_t start_window_pos = window_posn;
	if(start_window_pos == 0)
	{
		start_window_pos = window_size;
	}
	if(start_window_pos < outLen)
	{
		throw lzx_error("LzxDecoder::Decompress: invalid data (start_window_pos < outLen)");
	}
	start_window_pos -= outLen;
	std::copy_n(this->state.window + start_window_pos, outLen, outBuf);

	this->state.window_posn = window_posn;
	this->state.R0 = R0;
	this->state.R1 = R1;
	this->state.R2 = R2;
}

void LzxDecoder::MakeDecodeTable(uint16_t nsyms, uint8_t nbits, uint8_t* length, uint16_t* table)
{
	uint_fast32_t leaf;
	uint8_t bit_num = 1;
	uint_fast32_t pos		= 0; // the current position in the decode table
	// note: nbits is at most 12
	uint32_t table_mask		= 1 << nbits;

	// bit_mask never exceeds 15 bits
	uint16_t bit_mask		= static_cast<uint16_t>(table_mask >> 1); // don't do 0 length codes

	uint16_t next_symbol	= bit_mask; // base of allocation for long codes

	// fill entries for codes short enough for a direct mapping
	while(bit_num <= nbits)
	{
		for(uint16_t sym = 0; sym < nsyms; ++sym)
		{
			if(length[sym] == bit_num)
			{
				leaf = pos;

				if((pos += bit_mask) > table_mask)
				{
					throw lzx_error("LzxDecoder::MakeDecodeTable: table overrun (1)");
				}

				// fill all possible lookups of this symbol with the symbol itself
				std::fill_n(table + leaf, bit_mask, sym);
			}
		}
		bit_mask >>= 1;
		++bit_num;
	}

	// if there are any codes longer than nbits
	if(pos != table_mask)
	{
		// clear the remainder of the table
		std::fill_n(table + pos, table_mask - pos, 0);

		// give ourselves room for codes to grow by up to 16 more bits
		pos <<= 16;
		table_mask <<= 16;
		bit_mask = 1 << 15;

		while(bit_num <= 16)
		{
			for(uint16_t sym = 0; sym < nsyms; ++sym)
			{
				if(length[sym] == bit_num)
				{
					leaf = pos >> 16;
					for(uint_fast32_t fill = 0; fill < bit_num - nbits; ++fill)
					{
						// if this path hasn't been taken yet, 'allocate' two entries
						if(table[leaf] == 0)
						{
							table[(next_symbol << 1)] = 0;
							table[(next_symbol << 1) + 1] = 0;
							table[leaf] = (next_symbol++);
						}
						// follow the path and select either left or right for next bit
						leaf = static_cast<uint_fast32_t>(table[leaf] << 1);
						if(((pos >> (15 - fill)) & 1) == 1)
						{
							++leaf;
						}
					}
					table[leaf] = sym;

					if((pos += bit_mask) > table_mask)
					{
						throw lzx_error("LzxDecoder::MakeDecodeTable: table overrun (2)");
					}
				}
			}
			bit_mask >>= 1;
			++bit_num;
		}
	}

	// full table?
	if(pos == table_mask)
	{
		return;
	}

	// either erroneous table, or all elements are 0 - let's find out.
	for(uint_fast16_t sym = 0; sym < nsyms; ++sym)
	{
		if(length[sym] != 0)
		{
			throw lzx_error("LzxDecoder::MakeDecodeTable: erroneous table");
		}
	}
}

void LzxDecoder::ReadLengths(uint8_t* lens, const uint_fast32_t first, const uint_fast32_t last, BitBuffer& bitbuf)
{
	// hufftbl pointer here?

	for(uint_fast32_t x = 0; x < PRETREE_MAXSYMBOLS; ++x)
	{
		this->state.PRETREE_len[x] = static_cast<uint8_t>(bitbuf.ReadBits(4));
	}
	this->MakeDecodeTable(PRETREE_MAXSYMBOLS, PRETREE_TABLEBITS, this->state.PRETREE_len.data(), this->state.PRETREE_table.data());

	for(uint_fast32_t x = first; x < last; )
	{
		int_fast32_t z = this->ReadHuffSym(this->state.PRETREE_table.data(), this->state.PRETREE_len.data(), PRETREE_MAXSYMBOLS, PRETREE_TABLEBITS, bitbuf);
		if(z == 17)
		{
			uint_fast32_t y = bitbuf.ReadBits(4);
			y += 4;
			std::fill_n(lens + x, y, 0);
			x += y;
		}
		else if(z == 18)
		{
			uint_fast32_t y = bitbuf.ReadBits(5);
			y += 20;
			std::fill_n(lens + x, y, 0);
			x += y;
		}
		else if(z == 19)
		{
			uint_fast32_t y = bitbuf.ReadBits(1);
			y += 4;
			z = ReadHuffSym(this->state.PRETREE_table.data(), this->state.PRETREE_len.data(), PRETREE_MAXSYMBOLS, PRETREE_TABLEBITS, bitbuf);
			z = lens[x] - z;
			if(z < 0)
			{
				z += 17;
			}
			std::fill_n(lens + x, y, static_cast<uint8_t>(z));
			x += y;
		}
		else
		{
			z = lens[x] - z;
			if(z < 0)
			{
				z += 17;
			}
			lens[x++] = static_cast<uint8_t>(z);
		}
	}
}

uint32_t LzxDecoder::ReadHuffSym(const uint16_t* table, const uint8_t* lengths, const uint32_t nsyms, const uint8_t nbits, BitBuffer& bitbuf)
{
	uint32_t i, j;
	bitbuf.EnsureBits(16);
	if((i = table[bitbuf.PeekBits(nbits)]) >= nsyms)
	{
		j = static_cast<uint32_t>(1 << ((sizeof(uint32_t) * 8) - nbits));
		do
		{
			j >>= 1; i <<= 1; i |= (bitbuf.buffer & j) != 0 ? 1 : 0;
			if(j == 0)
			{
				throw lzx_error("LzxDecoder::ReadHuffSym: j == 0 in ReadHuffSym");
			}
		}
		while((i = table[i]) >= nsyms);
	}
	j = lengths[i];
	bitbuf.RemoveBits(static_cast<uint8_t>(j));

	return i;
}

std::string LzxDecoder::to_string(const BLOCKTYPE type)
{
	switch(type)
	{
		case BLOCKTYPE::INVALID:		return "invalid";
		case BLOCKTYPE::VERBATIM:		return "verbatim";
		case BLOCKTYPE::ALIGNED:		return "aligned";
		case BLOCKTYPE::UNCOMPRESSED:	return "uncompressed";
		default: return std::to_string(static_cast<uint8_t>(type));
	}
}
