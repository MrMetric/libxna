#ifndef LZXDECODER_H
#define LZXDECODER_H

#include <BinaryReader.hpp>
#include <BinaryWriter.hpp>
#include "../include/BitBuffer.hpp"

#define MIN_MATCH					2
#define MAX_MATCH					257
#define NUM_CHARS					256
#define PRETREE_NUM_ELEMENTS		20
#define ALIGNED_NUM_ELEMENTS		8
#define NUM_PRIMARY_LENGTHS			7
#define NUM_SECONDARY_LENGTHS		249

#define PRETREE_MAXSYMBOLS			PRETREE_NUM_ELEMENTS
#define PRETREE_TABLEBITS			6
#define MAINTREE_MAXSYMBOLS			(NUM_CHARS+50*8)
#define MAINTREE_TABLEBITS			12
#define LENGTH_MAXSYMBOLS			(NUM_SECONDARY_LENGTHS+1)
#define LENGTH_TABLEBITS			12
#define ALIGNED_MAXSYMBOLS			ALIGNED_NUM_ELEMENTS
#define ALIGNED_TABLEBITS			7

#define LENTABLE_SAFETY				64

class LzxDecoder
{
	public:
		explicit LzxDecoder(const uint_fast16_t window_bits);
		~LzxDecoder();
		void Decompress(const uint8_t* inBuf, uint_fast32_t inLen, uint8_t* outBuf, uint_fast32_t outLen);

		static uint32_t position_base[51];
		static uint8_t extra_bits[52];

		static void ArrayCopy(const uint8_t* arrayIn, uint_fast32_t inStart, uint8_t* arrayOut, uint_fast32_t outStart, uint_fast32_t length);

	private:
		int32_t MakeDecodeTable(uint32_t nsyms, uint32_t nbits, uint8_t length[], uint16_t table[]);
		void ReadLengths(uint8_t lens[], uint32_t first, uint32_t last, BitBuffer *bitbuf);
		uint32_t ReadHuffSym(uint16_t* table, uint8_t* lengths, uint32_t nsyms, uint32_t nbits, BitBuffer* bitbuf);

		enum BLOCKTYPE
		{
			INVALID = 0,
			VERBATIM = 1,
			ALIGNED = 2,
			UNCOMPRESSED = 3
		};
		uint32_t			state_R0, state_R1, state_R2; 	// for the LRU offset system
		uint16_t			state_main_elements;			// number of main tree elements
		bool				state_header_read;				// have we started decoding at all yet?
		BLOCKTYPE			state_block_type;				// type of this block
		uint32_t			state_block_length;				// uncompressed length of this block
		uint32_t			state_block_remaining;			// uncompressed bytes still left to decode

		uint16_t			state_PRETREE_table[(1 << PRETREE_TABLEBITS) + (PRETREE_MAXSYMBOLS << 1)];
		uint8_t				state_PRETREE_len[PRETREE_MAXSYMBOLS + LENTABLE_SAFETY];
		uint16_t			state_MAINTREE_table[(1 << MAINTREE_TABLEBITS) + (MAINTREE_MAXSYMBOLS << 1)];
		uint8_t				state_MAINTREE_len[MAINTREE_MAXSYMBOLS + LENTABLE_SAFETY];
		uint16_t			state_LENGTH_table[(1 << LENGTH_TABLEBITS) + (LENGTH_MAXSYMBOLS << 1)];
		uint8_t				state_LENGTH_len[LENGTH_MAXSYMBOLS + LENTABLE_SAFETY];
		uint16_t			state_ALIGNED_table[(1 << ALIGNED_TABLEBITS) + (ALIGNED_MAXSYMBOLS << 1)];
		uint8_t				state_ALIGNED_len[ALIGNED_MAXSYMBOLS + LENTABLE_SAFETY];

		// NEEDED MEMBERS
		// CAB actualsize
		// CAB window
		// CAB window_size
		// CAB window_posn
		uint8_t*			state_window;
		uint32_t			state_window_size;
		uint32_t			state_window_posn;
};

#endif
