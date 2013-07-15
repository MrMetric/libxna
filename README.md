libxna
======

A library for decompressing XNB files and converting the data to a standard format such as WAV or PNG.

Example usage
======

    XNAconverter::XNB2PNG("Logo2.xnb", "Logo2.png");
Assuming that Logo2.xnb is the same as the one in Terraria, this will do the following:
* read Logo2.xnb
* determine that the file is compressed
* decompress the data and verify that it is an image
* save the data to a PNG image file (Logo2.png)

The XNB2WAV function takes the same arguments (XNB name and output name).

If you want to get the dimensions of the image:

    uint_fast32_t width, height;
    XNAconverter::XNB2PNG("Logo2.xnb", "Logo2.png", width, height);
    std::cout << width << "x" << height << "\n";
This outputs the following:

    486x142
Note that there is currently no function to get the image dimensions without converting the data to PNG format.

TODO
======

* Get XNB format information without having to convert the data (see example above)
* Make a general conversion function that detects the XNB type
* Convert standard formats to XNB
* Figure out how to create compressed XNB data by reversing the code in LzxDecoder
