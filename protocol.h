#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint> // For uint32_t

// The fixed-size header for all Tracker-Client communication.
// Using #pragma pack to ensure the struct is laid out the same way
// across different compilers and architectures, which is crucial for network programming.
#pragma pack(push, 1)
struct MessageHeader {
    char command[32];      // Null-padded command, e.g., "login", "upload_file"
    char status[32];       // For responses, e.g., "200_OK", "404_NOT_FOUND"
    uint32_t payload_size; // Size of the data that follows this header, in bytes
};
#pragma pack(pop)

#pragma pack(push, 1)
struct PieceRequest {
    char file_hash[41]; // The SHA1 hash of the ENTIRE file being requested
    uint32_t piece_index;
};
#pragma pack(pop)

// Sent by seeder to downloader in response
#pragma pack(push, 1)
struct PieceResponse {
    char file_hash[41];
    uint32_t piece_index;
    // The actual piece data will be sent immediately after this header
};
#pragma pack(pop)


// Define the standard piece size for file transfers
const int PIECE_SIZE = 512 * 1024; // 512 KB

#endif // PROTOCOL_H

