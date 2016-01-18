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

#include "../include/LzxDecoder.hpp"
#include <string.h> // for memcpy
#include <iostream>
#include <sstream>
#include <algorithm> // std::copy_n

#define MAKESTR(ss) static_cast<std::ostringstream&>(std::ostringstream().seekp(0) << ss).str()

uint32_t LzxDecoder::position_base[51];
uint8_t LzxDecoder::extra_bits[52];

LzxDecoder::LzxDecoder(const uint_fast16_t window_bits)
{
	if(window_bits < 15 || window_bits > 21)
	{
		throw MAKESTR("Unsupported window size range: " << window_bits);
	}

	this->state_window_size = 1 << window_bits;

	// let's initialise our state
	this->state_window = new uint8_t[this->state_window_size];
	for(uint_fast32_t i = 0; i < this->state_window_size; ++i)
	{
		this->state_window[i] = 0xDC;
	}
	this->state_window_posn = 0;

	// initialize static tables
	for(uint_fast32_t i = 0, j = 0; i <= 50; i += 2)
	{
		LzxDecoder::extra_bits[i] = LzxDecoder::extra_bits[i + 1] = static_cast<uint8_t>(j);
		if((i != 0) && (j < 17))
		{
			++j;
		}
	}
	for(uint_fast32_t i = 0, j = 0; i <= 50; ++i)
	{
		LzxDecoder::position_base[i] = static_cast<uint32_t>(j);
		j += 1 << LzxDecoder::extra_bits[i];
	}

	/* calculate required position slots */
	uint_fast32_t posn_slots;
	if(window_bits == 20)
	{
		posn_slots = 42;
	}
	else if(window_bits == 21)
	{
		posn_slots = 50;
	}
	else
	{
		posn_slots = window_bits << 1;
	}

	// reset state
	this->state_R0 = this->state_R1 = this->state_R2 = 1;
	this->state_main_elements = static_cast<uint16_t>(NUM_CHARS + (posn_slots << 3));
	this->state_header_read = false;
	this->state_block_remaining = 0;
	this->state_block_type = INVALID;

	// initialise tables to 0 (because deltas will be applied to them)
	for(uint_fast32_t i = 0; i < MAINTREE_MAXSYMBOLS; ++i)
	{
		this->state_MAINTREE_len[i] = 0;
	}
	for(uint_fast32_t i = 0; i < LENGTH_MAXSYMBOLS; ++i)
	{
		this->state_LENGTH_len[i] = 0;
	}
}

LzxDecoder::~LzxDecoder()
{
	delete[] this->state_window;
}

void LzxDecoder::Decompress(const uint8_t* inBuf, uint_fast32_t inLen, uint8_t* outBuf, uint_fast32_t outLen)
{
	if(outLen < 1)
	{
		throw ("LzxDecoder: outLen is < 1 (" + std::to_string(outLen) + ")");
	}

	BitBuffer bitbuf(inBuf, inLen);

	uint_fast32_t window_posn = this->state_window_posn;
	uint_fast32_t window_size = this->state_window_size;
	uint_fast32_t R0 = this->state_R0;
	uint_fast32_t R1 = this->state_R1;
	uint_fast32_t R2 = this->state_R2;
	uint_fast32_t i, j;

	uint_fast32_t togo = outLen;
	uint_fast32_t main_element;
	uint_fast32_t match_length;
	uint_fast32_t match_offset;
	uint_fast32_t length_footer;
	uint_fast32_t extra;
	uint_fast32_t verbatim_bits;
	uint_fast32_t rundest, runsrc, copy_length, aligned_bits;

	// read header if necessary
	if(!this->state_header_read)
	{
		uint32_t intel = bitbuf.ReadBits(1);
		if(intel != 0)
		{
			throw std::string("LzxDecoder: Intel E8 not supported");
		}
		this->state_header_read = true;
	}

	// main decoding loop
	while(togo > 0)
	{
		// last block finished, new block expected
		if(this->state_block_remaining == 0)
		{
			this->state_block_type = static_cast<BLOCKTYPE>(bitbuf.ReadBits(3));
			i = bitbuf.ReadBits(16);
			j = bitbuf.ReadBits(8);
			this->state_block_remaining = this->state_block_length = static_cast<uint32_t>((i << 8) | j);

			switch(this->state_block_type)
			{
				case ALIGNED:
				{
					for(i = 0, j = 0; i < 8; ++i)
					{
						j = bitbuf.ReadBits(3);
						this->state_ALIGNED_len[i] = static_cast<uint8_t>(j);
					}
					this->MakeDecodeTable(ALIGNED_MAXSYMBOLS, ALIGNED_TABLEBITS, this->state_ALIGNED_len, this->state_ALIGNED_table);
					// rest of aligned header is same as verbatim
					#ifdef __clang__
					[[clang::fallthrough]];
					#endif
				}

				case VERBATIM:
				{
					this->ReadLengths(this->state_MAINTREE_len, 0, 256, bitbuf);
					this->ReadLengths(this->state_MAINTREE_len, 256, this->state_main_elements, bitbuf);
					this->MakeDecodeTable(MAINTREE_MAXSYMBOLS, MAINTREE_TABLEBITS, this->state_MAINTREE_len, this->state_MAINTREE_table);

					this->ReadLengths(this->state_LENGTH_len, 0, NUM_SECONDARY_LENGTHS, bitbuf);
					this->MakeDecodeTable(LENGTH_MAXSYMBOLS, LENGTH_TABLEBITS, this->state_LENGTH_len, this->state_LENGTH_table);
					break;
				}

				case UNCOMPRESSED:
				{
					bitbuf.EnsureBits(16); // get up to 16 pad bits into the buffer
					if(bitbuf.bitsleft > 16)
					{
						bitbuf.inpos -= 2; // align the bitstream
					}
					R0 = bitbuf.ReadUInt32();
					R1 = bitbuf.ReadUInt32();
					R2 = bitbuf.ReadUInt32();
					break;
				}

				case INVALID:
				default:
				{
					throw MAKESTR("Invalid state block type:  " << this->state_block_type);
				}
			}
		}

		/* buffer exhaustion check */
		if(bitbuf.inpos > inLen)
		{
			/*it's possible to have a file where the next run is less than 16 bits in size. In this case, the READ_HUFFSYM() macro used in building
			the tables will exhaust the buffer, so we should allow for this, but not allow those accidentally read bits to be used
			(so we check that there are at least 16 bits remaining - in this boundary case they aren't really part of the compressed data)*/
			std::cerr << "WTF: pos > startpos + inLen\n";
			if(bitbuf.inpos > (inLen + 2) || bitbuf.bitsleft < 16)
			{
				throw "Invalid data";
			}
		}

		uint_fast32_t this_run;
		while((this_run = this->state_block_remaining) > 0 && togo > 0)
		{
			if(this_run > togo)
			{
				this_run = togo;
			}
			togo -= this_run;
			this->state_block_remaining -= this_run;

			// apply 2^x-1 mask
			window_posn &= window_size - 1;
			// runs can't straddle the window wraparound
			if((window_posn + this_run) > window_size)
			{
				throw std::string("Window position + Run > Window Size");
			}

			switch(this->state_block_type)
			{
				case VERBATIM:
				{
					while(this_run > 0)
					{
						main_element = this->ReadHuffSym(this->state_MAINTREE_table, this->state_MAINTREE_len, MAINTREE_MAXSYMBOLS, MAINTREE_TABLEBITS, bitbuf);
						if(main_element < NUM_CHARS)
						{
							/* literal: 0 to NUM_CHARS-1 */
							this->state_window[window_posn++] = static_cast<uint8_t>(main_element);
							--this_run;
						}
						else
						{
							/* match: NUM_CHARS + ((slot<<3) | length_header (3 bits)) */
							main_element -= NUM_CHARS;

							match_length = main_element & NUM_PRIMARY_LENGTHS;
							if(match_length == NUM_PRIMARY_LENGTHS)
							{
								length_footer = this->ReadHuffSym(this->state_LENGTH_table, this->state_LENGTH_len, LENGTH_MAXSYMBOLS, LENGTH_TABLEBITS, bitbuf);
								match_length += length_footer;
							}
							match_length += MIN_MATCH;

							match_offset = main_element >> 3;

							if(match_offset > 2)
							{
								// not repeated offset
								if(match_offset != 3)
								{
									extra = extra_bits[match_offset];
									verbatim_bits = (int32_t)bitbuf.ReadBits((uint8_t)extra);
									match_offset = (int32_t)position_base[match_offset] - 2 + verbatim_bits;
								}
								else
								{
									match_offset = 1;
								}

								/* update repeated offset LRU queue */
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

							rundest = window_posn;
							this_run -= match_length;

							/* copy any wrapped around source data */
							if(window_posn >= match_offset)
							{
								// no wrap
								runsrc = rundest - match_offset;
							}
							else
							{
								runsrc = rundest + ((int32_t)window_size - match_offset);
								copy_length = match_offset - (int32_t)window_posn;
								if(copy_length < match_length)
								{
									match_length -= copy_length;
									window_posn += (uint32_t)copy_length;
									while(copy_length-- > 0)
									{
										this->state_window[rundest++] = this->state_window[runsrc++];
									}
									runsrc = 0;
								}
							}
							window_posn += (uint32_t)match_length;

							/* copy match data - no worries about destination wraps */
							while(match_length-- > 0)
							{
								this->state_window[rundest++] = this->state_window[runsrc++];
							}
						}
					}
					break;
				}

				case ALIGNED:
				{
					while(this_run > 0)
					{
						main_element = (int32_t)this->ReadHuffSym(this->state_MAINTREE_table, this->state_MAINTREE_len, MAINTREE_MAXSYMBOLS, MAINTREE_TABLEBITS, bitbuf);

						if(main_element < NUM_CHARS)
						{
							// literal 0 to NUM_CHARS-1
							this->state_window[window_posn++] = (uint8_t)main_element;
							--this_run;
						}
						else
						{
							/* match: NUM_CHARS + ((slot<<3) | length_header (3 bits)) */
							main_element -= NUM_CHARS;

							match_length = main_element & NUM_PRIMARY_LENGTHS;
							if(match_length == NUM_PRIMARY_LENGTHS)
							{
								length_footer = (int32_t)this->ReadHuffSym(this->state_LENGTH_table, this->state_LENGTH_len, LENGTH_MAXSYMBOLS, LENGTH_TABLEBITS, bitbuf);
								match_length += length_footer;
							}
							match_length += MIN_MATCH;

							match_offset = main_element >> 3;

							if(match_offset > 2)
							{
								/* not repeated offset */
								extra = extra_bits[match_offset];
								match_offset = (int32_t)position_base[match_offset] - 2;
								if(extra > 3)
								{
									/* verbatim and aligned bits */
									extra -= 3;
									verbatim_bits = (int32_t)bitbuf.ReadBits((uint8_t)extra);
									match_offset += (verbatim_bits << 3);
									aligned_bits = (int32_t)this->ReadHuffSym(this->state_ALIGNED_table, this->state_ALIGNED_len, ALIGNED_MAXSYMBOLS, ALIGNED_TABLEBITS, bitbuf);
									match_offset += aligned_bits;
								}
								else if(extra == 3)
								{
									/* aligned bits only */
									aligned_bits = (int32_t)this->ReadHuffSym(this->state_ALIGNED_table, this->state_ALIGNED_len, ALIGNED_MAXSYMBOLS, ALIGNED_TABLEBITS, bitbuf);
									match_offset += aligned_bits;
								}
								else if(extra > 0) /* extra==1, extra==2 */
								{
									/* verbatim bits only */
									verbatim_bits = (int32_t)bitbuf.ReadBits((uint8_t)extra);
									match_offset += verbatim_bits;
								}
								else /* extra == 0 */
								{
									/* ??? */
									match_offset = 1;
								}

								/* update repeated offset LRU queue */
								R2 = R1;
								R1 = R0;
								R0 = (uint32_t)match_offset;
							}
							else if(match_offset == 0)
							{
								match_offset = (int32_t)R0;
							}
							else if(match_offset == 1)
							{
								match_offset = (int32_t)R1;
								R1 = R0;
								R0 = (uint32_t)match_offset;
							}
							else /* match_offset == 2 */
							{
								match_offset = (int32_t)R2;
								R2 = R0;
								R0 = (uint32_t)match_offset;
							}

							rundest = (int32_t)window_posn;
							this_run -= match_length;

							// copy any wrapped around source data
							if(window_posn >= match_offset)
							{
								// no wrap
								runsrc = rundest - match_offset;
							}
							else
							{
								runsrc = rundest + ((int32_t)window_size - match_offset);
								copy_length = match_offset - (int32_t)window_posn;
								if(copy_length < match_length)
								{
									match_length -= copy_length;
									window_posn += (uint32_t)copy_length;
									while(copy_length-- > 0)
									{
										this->state_window[rundest++] = this->state_window[runsrc++];
									}
									runsrc = 0;
								}
							}
							window_posn += (uint32_t)match_length;

							// copy match data - no worries about destination wraps
							while(match_length-- > 0)
							{
								this->state_window[rundest++] = this->state_window[runsrc++];
							}
						}
					}
					break;
				}

				case UNCOMPRESSED:
				{
					if((bitbuf.inpos + this_run) > inLen)
					{
						throw std::string("WTF: (bitbuf.inpos + this_run) > endpos");
					}

					std::copy_n(inBuf + bitbuf.inpos, this_run, this->state_window + window_posn);
					bitbuf.inpos += this_run;
					window_posn += (uint32_t)this_run;
					break;
				}

				case INVALID:
				default:
				{
					throw MAKESTR("Invalid state block type: " << this->state_block_type << "\n");
				}
			}
		}
	}
	if(togo != 0)
	{
		throw std::string("togo != 0\n");
	}

	int start_window_pos = (int32_t)window_posn;
	if(start_window_pos == 0)
	{
		start_window_pos = (int32_t)window_size;
	}
	start_window_pos -= outLen;
	std::copy_n(this->state_window + start_window_pos, outLen, outBuf);

	this->state_window_posn = window_posn;
	this->state_R0 = R0;
	this->state_R1 = R1;
	this->state_R2 = R2;
}

// TODO make returns throw exceptions
int LzxDecoder::MakeDecodeTable(uint32_t nsyms, uint32_t nbits, uint8_t length[], uint16_t table[])
{
	uint16_t sym;
	uint32_t leaf;
	uint8_t bit_num = 1;
	uint32_t fill;
	uint32_t pos			= 0; // the current position in the decode table
	uint32_t table_mask		= 1 << nbits;
	uint32_t bit_mask		= table_mask >> 1; // don't do 0 length codes
	uint32_t next_symbol	= bit_mask; // base of allocation for long codes

	/* fill entries for codes short enough for a direct mapping */
	while(bit_num <= nbits)
	{
		for(sym = 0; sym < nsyms; ++sym)
		{
			if(length[sym] == bit_num)
			{
				leaf = pos;

				if((pos += bit_mask) > table_mask)
				{
					return 1; /* table overrun */
				}

				/* fill all possible lookups of this symbol with the symbol itself */
				fill = bit_mask;
				while(fill-- > 0)
				{
					table[leaf++] = sym;
				}
			}
		}
		bit_mask >>= 1;
		++bit_num;
	}

	/* if there are any codes longer than nbits */
	if(pos != table_mask)
	{
		/* clear the remainder of the table */
		for(sym = (uint16_t)pos; sym < table_mask; ++sym)
		{
			table[sym] = 0;
		}

		/* give ourselves room for codes to grow by up to 16 more bits */
		pos <<= 16;
		table_mask <<= 16;
		bit_mask = 1 << 15;

		while(bit_num <= 16)
		{
			for(sym = 0; sym < nsyms; ++sym)
			{
				if(length[sym] == bit_num)
				{
					leaf = pos >> 16;
					for(fill = 0; fill < bit_num - nbits; ++fill)
					{
						// if this path hasn't been taken yet, 'allocate' two entries
						if(table[leaf] == 0)
						{
							table[(next_symbol << 1)] = 0;
							table[(next_symbol << 1) + 1] = 0;
							table[leaf] = (uint16_t)(next_symbol++);
						}
						// follow the path and select either left or right for next bit
						leaf = (uint32_t)(table[leaf] << 1);
						if(((pos >> (int32_t)(15 - fill)) & 1) == 1)
						{
							++leaf;
						}
					}
					table[leaf] = sym;

					if((pos += bit_mask) > table_mask)
					{
						return 1;
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
		return 0;
	}

	// either erroneous table, or all elements are 0 - let's find out.
	for(sym = 0; sym < nsyms; ++sym)
	{
		if(length[sym] != 0)
		{
			return 1;
		}
	}
	return 0;
}

// TODO throw exceptions instead of returns
void LzxDecoder::ReadLengths(uint8_t lens[], uint32_t first, uint32_t last, BitBuffer& bitbuf)
{
	uint32_t x, y;

	// hufftbl pointer here?

	for(x = 0; x < 20; ++x)
	{
		y = bitbuf.ReadBits(4);
		this->state_PRETREE_len[x] = (uint8_t)y;
	}
	MakeDecodeTable(PRETREE_MAXSYMBOLS, PRETREE_TABLEBITS, this->state_PRETREE_len, this->state_PRETREE_table);

	for(x = first; x < last;)
	{
		int z = (int32_t)ReadHuffSym(this->state_PRETREE_table, this->state_PRETREE_len, PRETREE_MAXSYMBOLS, PRETREE_TABLEBITS, bitbuf);
		if(z == 17)
		{
			y = bitbuf.ReadBits(4); y += 4;
			while(y-- != 0) lens[x++] = 0;
		}
		else if(z == 18)
		{
			y = bitbuf.ReadBits(5); y += 20;
			while(y-- != 0) lens[x++] = 0;
		}
		else if(z == 19)
		{
			y = bitbuf.ReadBits(1); y += 4;
			z = (int32_t)ReadHuffSym(this->state_PRETREE_table, this->state_PRETREE_len, PRETREE_MAXSYMBOLS, PRETREE_TABLEBITS, bitbuf);
			z = lens[x] - z; if(z < 0) z += 17;
			while(y-- != 0)
			{
				lens[x++] = static_cast<uint8_t>(z);
			}
		}
		else
		{
			z = lens[x] - z; if(z < 0) z += 17;
			lens[x++] = static_cast<uint8_t>(z);
		}
	}
}

uint32_t LzxDecoder::ReadHuffSym(uint16_t* table, uint8_t* lengths, uint32_t nsyms, uint8_t nbits, BitBuffer& bitbuf)
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
				throw "LzxDecoder: j == 0 in ReadHuffSym";
			}
		}
		while((i = table[i]) >= nsyms);
	}
	j = lengths[i];
	bitbuf.RemoveBits(static_cast<uint8_t>(j));

	return i;
}