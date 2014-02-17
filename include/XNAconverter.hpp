#ifndef XNACONVERTER_H
#define XNACONVERTER_H

#include <BinaryReader.hpp>

#define SURFACEFORMAT_RGBA8888	0
#define SURFACEFORMAT_BGR565	1
#define SURFACEFORMAT_BGRA5551	2
#define SURFACEFORMAT_BGRA4444	3
#define SURFACEFORMAT_DXT1		4
#define SURFACEFORMAT_DXT3		5
#define SURFACEFORMAT_DXT5		6

class XNAconverter
{
	public:
		static void readHeader(BinaryReader *br, bool& compressed, uint32_t& fileLength);
		static void readXNB(BinaryReader* br, std::string& type, std::string outputDec = "");

		static void XNB2WAV(std::string xnb, std::string wav);
		static void XNB2PNG(std::string xnb, std::string png);
		static void XNB2PNG(std::string xnb, std::string png, uint32_t& width, uint32_t& height);

		static std::string getTypeName(std::string type);
};

#if (__cplusplus != 201103L) && !defined(NULLPTR_EMU)
#define NULLPTR_EMU
// From http://stackoverflow.com/a/2419885
const								// this is a const object...
class
{
	public:
		template<class T>			// convertible to any type
			operator T*() const		// of null non-member
			{ return 0; }			// pointer...
		template<class C, class T>	// or any type of null
			operator T C::*() const	// member pointer...
			{ return 0; }
	private:
		void operator&() const;		// whose address can't be taken
} nullptr = {};						// and whose name is nullptr
#endif

#endif // XNACONVERTER_H
