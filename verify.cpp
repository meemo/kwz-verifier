#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <iterator>

#include "verify.hpp"

std::vector<u8> readFile(std::string path) {
    std::ifstream file(path, std::ios::binary);
    std::vector<u8> output;

    if (file) {
        // Stop eating newlines in binary mode
        file.unsetf(std::ios::skipws);

        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        output.reserve(file_size);
        output.insert(output.begin(),
                      std::istream_iterator<u8>(file),
                      std::istream_iterator<u8>());

        file.close();

        return output;
    }
    else {
        std::cout << "Failed to read file." << std::endl;
        exit(-1);
    }
}

bool verifyCRC32(std::vector<u8> buffer, int pos, u32 length, u32 file_crc32) {
    u32 crc32 = 0xFFFFFFFF;

    for (int i = pos; i < pos + (int)length; i++) {
        crc32 = crc32_table[(crc32 ^ buffer[i]) & 0xff] ^ (crc32 >> 8);
    }

    return (~crc32) == file_crc32;
}

// Verifies the KFH header
// Since KFH is a fixed size and fixed position, we only need to have the buffer passed
bool verifyKFH(std::vector<u8> buffer) {
    bool result = true;

    // Verify KFH header magic
    if (!(buffer[0] == 0x4B &&
          buffer[1] == 0x46 &&
          buffer[2] == 0x48 &&
          buffer[3] == 0x14)) {
        std::cout << "[KFH] Magic invalid." << std::endl;
        result = false;
    }

    // Verify KFH header crc32
    if (!verifyCRC32(buffer,
                     8 + 4,
                     getInt<u32>(buffer, 4) - 4,
                     getInt<u32>(buffer, 8))) {
        std::cout << "[KFH] CRC32 invalid." << std::endl;
        result = false;
    }

    return result;
}

// Verifies the KTN section
// Assumes `offset` points to the magic of the section
// KTN sections unfortunately only have a CRC32 for verification (not even consistent JPG headers)
// This is all we can do sadly
bool verifyKTN(std::vector<u8> buffer, int offset) {
    bool result = true;

    // Verify the KTN section's CRC32
    if (!verifyCRC32(buffer,
                     offset + 0xC,
                     getInt<u32>(buffer, offset + 4) - 4,
                     getInt<u32>(buffer, offset + 8))) {
        std::cout << "[KTN] CRC32 is invalid." << std::endl;
        result = false;
    }

    return result;
}

// Verifies the KSN section
// Assumes `offset` points to the magic of the section
// Unfortunately the KSN section CRC32 only covers the audio data
bool verifyKSN(std::vector<u8> buffer, int offset) {
    bool result = true;

    // We don't need to extract the track sizes because we're only looking at the audio data as a whole,
    // which the size of is stored just after the KSN magic

    // Verify the KSN section's CRC32
    if (!verifyCRC32(buffer,
                     offset + 0x24,
                     getInt<u32>(buffer, offset + 4) - 0x1C,
                     getInt<u32>(buffer, offset + 0x20))) {
        std::cout << "[KSN] CRC32 is invalid." << std::endl;
        result = false;
    }

    return result;
}

// Verifies the KMI section
// Assumes `offset` points to the magic of the section
// KMI sections unfortunately have literally nothing to verify the contents.
// In the future maybe some regex-esque checks could be implemented for each entry, but that's a lot of work
bool verifyKMI(std::vector<u8> buffer, int offset) {
    return true;
}

// Verifies the KMC section
// Assumes `offset` points to the magic of the section
// KMC has a CRC32 over all the frame data (which is the entire section minus the header)
bool verifyKMC(std::vector<u8> buffer, int offset) {
    bool result = true;

    // Verify the KMC section's CRC32
    if (!verifyCRC32(buffer,
                     offset + 0xC,
                     getInt<u32>(buffer, offset + 4) - 4,
                     getInt<u32>(buffer, offset + 8))) {
        std::cout << "[KMC] CRC32 is invalid." << std::endl;
        result = false;
    }

    return result;
}

int main(int argc, char **argv) {
    if (argc == 2) {
        file_buffer = readFile(std::string(argv[1]));
    }
    else {
        return 1;
    }

    int offset;

    // KFH
    if (file_buffer[0] == 0x4B &&
        file_buffer[1] == 0x46 &&
        file_buffer[2] == 0x48 &&
        file_buffer[3] == 0x14) {
        offset = (int)getInt<u32>(file_buffer, 4) + 8;
    }
    else {
        std::cout << "[KFH] Magic invalid, exiting." << std::endl;
        return 1;
    }

    verifyKFH(file_buffer);

    // Find if all sections exist, and if so grab the offset and lengths.
    // Every section starts at the beginning of a 4 byte block, so we can jump around to each 4 byte chunk
    // of the file buffer and look for a section magic.
    while (offset < (int)file_buffer.size()) {
        // Check for `K` first, since all magics start with it.
        // This will greatly speed up checking by being able to immediately jump to a new block after checking 1 byte.
        if (file_buffer[offset] == 0x4B) {
            // We can now check for the ENTIRE magic of each section, including the static flag
            // Collisions have happened before by just checking 1 or 2 bytes, so it's necessary to check it all.
            u8 magic_1 = file_buffer[offset + 1];
            u8 magic_2 = file_buffer[offset + 2];
            u8 magic_3 = file_buffer[offset + 3];

            // KSN
            if (magic_1 == 0x53 &&
                magic_2 == 0x4E &&
                magic_3 == 0x01) {
                ksn_offset = offset;
                offset += getInt<u32>(file_buffer, offset + 4) + 8;
            }
            // KMC
            else if (magic_1 == 0x4D &&
                     magic_2 == 0x43 &&
                     magic_3 == 0x02) {
                kmc_offset = offset;
                offset += getInt<u32>(file_buffer, offset + 4) + 8;
            }
            // KMI
            else if (magic_1 == 0x4D &&
                     magic_2 == 0x49 &&
                     magic_3 == 0x05) {
                kmi_offset = offset;
                offset += getInt<u32>(file_buffer, offset + 4) + 8;
            }
            // KTN
            else if (magic_1 == 0x54 &&
                     magic_2 == 0x4E &&
                     magic_3 == 0x02) {
                ktn_offset = offset;
                offset += getInt<u32>(file_buffer, offset + 4) + 8;
            }
            else {
                printf("[%07X] [%02X][%02X][%02X][%02X] huh?\n", offset,
                                                                        file_buffer[offset],
                                                                        file_buffer[offset + 1],
                                                                        file_buffer[offset + 2],
                                                                        file_buffer[offset + 3]);
                offset += 4;
            }
        }
        else {
            printf("[%07X] [%02X][%02X][%02X][%02X] huh?\n", offset,
                                                                    file_buffer[offset],
                                                                    file_buffer[offset + 1],
                                                                    file_buffer[offset + 2],
                                                                    file_buffer[offset + 3]);
            offset += 4;
        }
    }

    verifyKTN(file_buffer, ktn_offset);

    verifyKMC(file_buffer, kmc_offset);

    verifyKMI(file_buffer, kmi_offset);

    verifyKSN(file_buffer, ksn_offset);
}