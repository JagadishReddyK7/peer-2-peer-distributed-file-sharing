#ifndef HASHING_H
#define HASHING_H

#include <string>
#include <vector>
#include <openssl/sha.h>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <fstream>

// Required for open, read, close, lseek
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

// Calculates SHA1 hash of a data buffer
std::string calculate_sha1(const char* data, size_t len) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(data), len, hash);

    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        ss << std::setw(2) << static_cast<unsigned int>(hash[i]);
    }
    return ss.str();
}

// Processes a file and returns a single concatenated string of all its piece hashes
std::string get_piece_hashes(const std::string& file_path, long long piece_size = 512 * 1024) {
    int fd = open(file_path.c_str(), O_RDONLY);
    if (fd < 0) {
        perror("open failed");
        return "";
    }

    std::string all_hashes;
    char* buffer = new char[piece_size];
    ssize_t bytes_read;

    while ((bytes_read = read(fd, buffer, piece_size)) > 0) {
        all_hashes += calculate_sha1(buffer, bytes_read);
    }

    delete[] buffer;
    close(fd);

    if (bytes_read < 0) {
        perror("read failed");
        return "";
    }
    return all_hashes;
}

std::string calculate_sha1_of_file(const std::string& file_path) {
    std::ifstream file(file_path, std::ifstream::binary);
    if (!file) {
        std::cerr << "Error opening file for full hash: " << file_path << std::endl;
        return "";
    }

    SHA_CTX sha_context;
    SHA1_Init(&sha_context);

    char buffer[8192];
    while (file.read(buffer, sizeof(buffer))) {
        SHA1_Update(&sha_context, buffer, sizeof(buffer));
    }
    // Handle the last chunk which might be smaller than the buffer size
    SHA1_Update(&sha_context, buffer, file.gcount());

    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1_Final(hash, &sha_context);

    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        ss << std::setw(2) << static_cast<unsigned int>(hash[i]);
    }
    
    file.close();
    return ss.str();
}

#endif // HASHING_H