#pragma once

#include <BinaryReader.hpp>
#include <BinaryWriter.hpp>
#include "BitBuffer.hpp"

#define MIN_MATCH					2
#define MAX_MATCH					257
#define NUM_CHARS					256
#define PRETREE_NUM_ELEMENTS		20
#define ALIGNED_NUM_ELEMENTS		8
#define NUM_PRIMARY_LENGTHS			7
#define NUM_SECONDARY_LENGTHS		249

const uint16_t PRETREE_MAXSYMBOLS = PRETREE_NUM_ELEMENTS;
const uint8_t PRETREE_TABLEBITS = 6;
const uint16_t MAINTREE_MAXSYMBOLS = NUM_CHARS + 50*8;
const uint8_t MAINTREE_TABLEBITS = 12;
const uint16_t LENGTH_MAXSYMBOLS = NUM_SECONDARY_LENGTHS + 1;
const uint8_t LENGTH_TABLEBITS = 12;
const uint16_t ALIGNED_MAXSYMBOLS = ALIGNED_NUM_ELEMENTS;
const uint8_t ALIGNED_TABLEBITS = 7;

class LzxDecoder
{
	public:
		explicit LzxDecoder(const uint_fast16_t window_bits);
		LzxDecoder(const LzxDecoder&) = delete;
		~LzxDecoder();
		void Decompress(const uint8_t* inBuf, const uint_fast32_t inLen, uint8_t* outBuf, const uint_fast32_t outLen);

		static uint32_t position_base[51];
		static uint8_t extra_bits[52];

	private:
		void MakeDecodeTable(uint16_t nsyms, uint8_t nbits, uint8_t length[], uint16_t table[]);
		void ReadLengths(uint8_t lens[], uint_fast32_t first, uint_fast32_t last, BitBuffer& bitbuf);
		uint32_t ReadHuffSym(uint16_t* table, uint8_t* lengths, uint32_t nsyms, uint8_t nbits, BitBuffer& bitbuf);

		enum class BLOCKTYPE : uint8_t
		{
			INVALID = 0,
			VERBATIM = 1,
			ALIGNED = 2,
			UNCOMPRESSED = 3,
		};
		uint_fast32_t		state_R0, state_R1, state_R2; 	// for the LRU offset system
		uint16_t			state_main_elements;			// number of main tree elements
		bool				state_header_read;				// have we started decoding at all yet?
		BLOCKTYPE			state_block_type;				// type of this block
		uint32_t			state_block_length;				// uncompressed length of this block
		uint32_t			state_block_remaining;			// uncompressed bytes still left to decode

		uint16_t			state_PRETREE_table[(1 << PRETREE_TABLEBITS) + (PRETREE_MAXSYMBOLS << 1)];
		uint8_t				state_PRETREE_len[PRETREE_MAXSYMBOLS];
		uint16_t			state_MAINTREE_table[(1 << MAINTREE_TABLEBITS) + (MAINTREE_MAXSYMBOLS << 1)];
		uint8_t				state_MAINTREE_len[MAINTREE_MAXSYMBOLS];
		uint16_t			state_LENGTH_table[(1 << LENGTH_TABLEBITS) + (LENGTH_MAXSYMBOLS << 1)];
		uint8_t				state_LENGTH_len[LENGTH_MAXSYMBOLS];
		uint16_t			state_ALIGNED_table[(1 << ALIGNED_TABLEBITS) + (ALIGNED_MAXSYMBOLS << 1)];
		uint8_t				state_ALIGNED_len[ALIGNED_MAXSYMBOLS];

		uint8_t*			state_window;
		uint_fast32_t		state_window_size;
		uint_fast32_t		state_window_posn;
};