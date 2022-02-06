#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iterator>

#include "kwz-verifier.hpp"

void readFile(std::string t_path) {
    std::ifstream file(t_path, std::ios::binary);

    if (file) {
        // Stop eating newlines and white space in binary mode
        file.unsetf(std::ios::skipws);

        file.seekg(0, std::ios::end);
        file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        file_buffer.reserve(file_size);
        file_buffer.insert(file_buffer.begin(),
                           std::istream_iterator<uint8_t>(file),
                           std::istream_iterator<uint8_t>());

        file.close();
    }
    else {
        std::cout << "Failed to read file. " << std::endl;
        exit(-1);
    }
}

uint16_t getUint16(int t_start) {
    return *reinterpret_cast<uint16_t*>(file_buffer.data() + t_start);
}

uint32_t getUint32(int t_start) {
    return *reinterpret_cast<uint32_t*>(file_buffer.data() + t_start);
}

uint32_t getCRC32(int t_start, int t_end) {
    // Gets the crc32 checksum of the data from the file buffer from t_start to t_end (exclusive of t_end alue)
    uint32_t crc32 = 0xFFFFFFFF;

    for (int i = t_start; i < t_end; i++) {
        crc32 = crc32_table[(crc32 ^ file_buffer[i]) & 0xff] ^ (crc32 >> 8);
    }

    return ~crc32;
}

bool verifySection(bool is_KSN) {
    // Get CRC32 checksum from the section size
    int section_size = getUint32(offset + 4);
    uint32_t section_crc32 = 0;
    uint32_t contents_crc32 = 0;

    if (is_KSN) {
        // KSN has its CRC32 after all track sizes
        // If there is no sound data (tracks are all 0), we don't need to verify the section.
        // We have to assume the section is valid if there's no sound data.
        if (getUint32(offset + 12) > 0 && getUint32(offset + 14) > 0 &&
            getUint32(offset + 16) > 0 && getUint32(offset + 20) > 0 &&
            getUint32(offset + 24) > 0 && getUint32(offset + 28) > 0) {
                section_crc32 = getUint32(offset + 32);
                contents_crc32 = getCRC32(offset + 36, offset + section_size + 8);
            }
        else {
            std::cout << "No sound data found, skipping verification." << std::endl;
        }
    }
    else {
        section_crc32 = getUint32(offset + 8);
        contents_crc32 = getCRC32(offset + 12, offset + section_size + 8);
    }

    return section_crc32 == contents_crc32;
}

void verifySections() {
    // Traverse sections using sizes at the end of each header
    int next_offset = 0;

    bool prev_section_failed = false;

    while (offset < (int)file_buffer.size()) {
        if (prev_section_failed) {
            // Find the next section to continue verification.
            // Ensure offset is still a multiple of 4
            offset = (offset + 3) & ~3;

            // Keep increasing
            while (!(file_buffer[offset] == (uint8_t)"K") || !(offset == file_size)) {
                offset += 4;
            }
        }

        if (file_buffer[offset + 1] == 'F') {
            // File header (KFH)
            std::cout << "Verifying KFH..." << std::endl;

            KFH_valid = verifySection(false);
            has_KFH = true;

            std::cout << "KFH Valid?: " << KFH_valid << std::endl << std::endl;
        }
        else if (file_buffer[offset + 1] == 'T') {
            // Thumbnail (KTN)
            std::cout << "Verifying KTN..." << std::endl;

            KTN_valid = verifySection(false);
            has_KTN = true;

            std::cout << "KTN Valid?: " << KTN_valid << std::endl << std::endl;
        }
        else if (file_buffer[offset + 1] == 'M') {
            // Frame data (KMC)
            if (file_buffer[offset + 2] == 'C') {
                std::cout << "Verifying KMC..." << std::endl;

                KMC_valid = verifySection(false);
                has_KMC = true;

                std::cout << "KMC Valid?: " << KMC_valid << std::endl << std::endl;
            }
            // Frame meta (KMI)
            else if (file_buffer[offset + 2] == 'I') {
                std::cout << "KMI is present. " << std::endl;

                has_KMI = true;

                std::cout << "KSN Valid? Unknown." << std::endl << std::endl;
            }
        }
        else if (file_buffer[offset + 1] == 'S') {
            // Sound header (KSN)
            std::cout << "Verifying KSN..." << std::endl;

            KSN_valid = verifySection(true);
            has_KSN = true;

            std::cout << "KSN Valid?: " << KSN_valid << std::endl << std::endl;
        }
        else {
            // Section is invalid
            std::cout << "Section has invalid magic at offset: " << offset << std::endl;
        }

        next_offset = getUint32(offset + 4);

        if ((offset + next_offset + 256 + 8) < file_size) {
            offset += next_offset + 8;
        }
        else if (file_size == (offset + next_offset + 256 + 8)) {
            std::cout << "Verification completed (signature exists too)" << std::endl;
            exit(0);
        }
        else if ((offset + next_offset + 256 + 8) < file_size && (offset + next_offset + 256 + 8) > file_size - 256) {
            std::cout << "Verification completed, but the signature doesn't appear to exist" << std::endl;
            exit(0);
        }
        else {
            std::cout << "This shouldn't be reached." << std::endl;
            exit(0);
        }
    }
}

int main(int argc, char** argv) {
    switch(argc) {
        case 1:
            std::cout << "You must pass an argument!" << std::endl;
            exit(-1);
            break;
        case 2:
            std::cout << "Input file path: " << argv[1] << std::endl;
            readFile(std::string(argv[1]));
            break;
        default:
            std::cout << "Invalid arguments passed!" << std::endl;
            exit(-1);
    }

    std::cout << "File size: " << file_size << std::endl << std::endl;

    verifySections();

    if (has_KFH && has_KMC && has_KMI) {
        std::cout << "File is a valid .KWC (FG:W comment) file!" << std::endl;
    }
    else if (has_KFH && has_KMC && has_KMI && has_KTN && has_KSN) {
        std::cout << "File is a valid .KWZ file!" << std::endl;
    }
    else {
        std::cout << "File is not valid." << std::endl;
        exit(1);
    }
}
