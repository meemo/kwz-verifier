#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iterator>

#include "kwz-verifier.hpp"

void readFile(std::string path) {
    std::ifstream file(path, std::ios::binary);

    if (file) {
        // Stop eating newlines in binary mode
        file.unsetf(std::ios::skipws);

        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        file_buffer.reserve(file_size);
        file_buffer.insert(file_buffer.begin(),
                           std::istream_iterator<uint8_t>(file),
                           std::istream_iterator<uint8_t>());

        file.close();
    }
    else {
        std::cout << "Failed to read file." << std::endl;
        exit(-1);
    }
}

u32 getCRC32(int start, int end) {
    // Gets the crc32 checksum of the data from the file buffer from t_start to t_end (exclusive of t_end alue)
    u32 crc32 = 0xFFFFFFFF;

    for (auto i = start; i < end; i++) {
        crc32 = crc32_table[(crc32 ^ file_buffer[i]) & 0xff] ^ (crc32 >> 8);
    }

    return ~crc32;
}

void verifySections() {
    // Go through every section of the file and
    // - Verify that we got to the next section
    //     - Verify section length is correct
    // - Verify the section contents via crc32
    int offset = 0;
    int section_size = 0;
    std::string previous_section = "";




}

int main(int argc, char** argv) {
    std::cout << "Hi" << std::endl;


}
