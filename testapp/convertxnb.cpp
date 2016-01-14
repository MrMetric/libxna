#include <XNAconverter.hpp>
#include <BinaryReader.hpp>
#include <BinaryWriter.hpp>
#include <sstream>
#include <iostream>

enum Mode
{
	none,
	decompress,
	wav,
	png,
};

int main(int argc, char** argv)
{
	if(argc < 2)
	{
		argv[1] = (char*)"-h";
	}
	std::string firstArg(argv[1]);
	if(firstArg == "--help" || firstArg == "-h" || firstArg == "-?")
	{
		std::cout << "Arguments:\n";
		std::cout << " -i        Input file*\n";
		std::cout << " -o        Output file\n";
		std::cout << " -m        Conversion mode*\n";
		std::cout << "             decompress\n";
		std::cout << "             wav\n";
		std::cout << "             png\n";
		std::cout << "* = required\n";
		std::cout << "\n";
		std::cout << "If no output name is specified, the default for each mode is:\n";
		std::cout << "  decompress: file.xnb → file_dec.xnb\n";
		std::cout << "  wav:        file.xnb → file.wav\n";
		std::cout << "  png:        file.xnb → file.png\n";
		return 0;
	}

	Mode mode = none;
	std::string inputFile = "";
	std::string outputFile = "";

	for(int arg = 1; arg < argc; ++arg)
	{
		std::string argument(argv[arg]);
		if(argument == "-")
		{
			// put no-value arguments here
		}
		else
		{
			++arg;
			std::string value;
			try
			{
				value = std::string(argv[arg]);
			}
			catch(...)
			{
				std::cerr << "No value given for " << argument << "\n";
				return 1;
			}

			if(argument == "-i")
			{
				inputFile = value;
			}
			else if(argument == "-o")
			{
				outputFile = value;
			}
			else if(argument == "-m")
			{
				if(value.compare("decompress") == 0)
				{
					mode = decompress;
				}
				else if(value.compare("wav") == 0)
				{
					mode = wav;
				}
				else if(value.compare("png") == 0)
				{
					mode = png;
				}
				else
				{
					std::cerr << "Invalid mode: " << value << "\n";
					return 1;
				}
			}
		}
	}

	if(inputFile == "")
	{
		std::cerr << "No input file specified\n";
		return EXIT_FAILURE;
	}

	if(mode == none)
	{
		std::cerr << "No mode specified\n";
		return EXIT_FAILURE;
	}

	if(outputFile == "")
	{
		std::stringstream ss;
		ss << inputFile.substr(0, inputFile.length() - 4);
		if(mode == decompress)
		{
			ss << "_dec.xnb";
		}
		else if(mode == wav)
		{
			ss << ".wav";
		}
		else if(mode == png)
		{
			ss << ".png";
		}
		outputFile = ss.str();
	}

	try
	{
		if(mode == decompress)
		{
			BinaryReader br(inputFile);
			std::string type;
			XNAconverter::readXNB(br, type, outputFile);
			std::cout << "XNB type: " << XNAconverter::getTypeName(type) << "\n";
		}
		else if(mode == wav)
		{
			XNAconverter::XNB2WAV(inputFile, outputFile);
		}
		else if(mode == png)
		{
			XNAconverter::XNB2PNG(inputFile, outputFile);
		}
	}
	catch(const std::string& e)
	{
		std::cout << "Error converting \"" << inputFile << "\" (" << e << ")\n";
		return EXIT_FAILURE;
	}

	std::cout << "saved " << outputFile << "\n";

	return EXIT_SUCCESS;
}
