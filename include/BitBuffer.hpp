#pragma once

#include <stdint.h>

class BitBuffer
{
	public:
		BitBuffer(const uint8_t* inBuf, uint_fast32_t inlen);
		void EnsureBits(uint8_t bits);
		uint32_t PeekBits(uint8_t bits) const;
		void RemoveBits(uint8_t bits);
		uint32_t ReadBits(uint8_t bits);
		uint16_t ReadUInt16();
		uint32_t ReadUInt32();

		uint32_t buffer;
		uint8_t bitsleft;
		uint_fast32_t inpos;

	private:
		template <class type> type ReadType();

		uint_fast32_t inlen;
		const uint8_t* inBuf;
};
