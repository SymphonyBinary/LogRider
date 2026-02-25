#pragma once

#if defined LINUX || defined(__linux__) || defined ANDROID || defined __ANDROID__ || \
        defined APPLE || defined __APPLE__
#define CAPLOG_SOCKET_ENABLED
#else
#undef CAPLOG_SOCKET_ENABLED
#endif

#ifdef CAPLOG_SOCKET_ENABLED
#include <string>

#include <arpa/inet.h>
// #include <netinet/in.h> // for internet sockets... can't get it to work
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sstream>

#include <signal.h>
#include <cstring>
#include <iostream>
#include <mutex>
#include <vector>

#include "outputstdout.hpp"

namespace CAP {

constexpr const size_t defaultPort = 8427;
constexpr const char* defaultHost = "127.0.0.1";

#ifndef CAPLOG_SOCKET_PORT
#define CAPLOG_SOCKET_PORT defaultPort
#endif

#ifndef CAPLOG_SOCKET_HOST
#define CAPLOG_SOCKET_HOST defaultHost
#endif

class SocketLogger {
  public:
    struct Header {
        // [0] and [1] hold an 8 byte wide delimiter
        uint32_t payload[4] = {0x12345678, 0x87654321, 0, 0};
    };
    static_assert(sizeof(Header) == 16, "Header must be 16 bytes");

    static SocketLogger& getSocketLogger() {
        static SocketLogger logger;
        return logger;
    }

    static bool sendBufferOverSocket(int socketFD, const void* buffer, size_t numBytes) {
        const char* bufferPtr = static_cast<const char*>(buffer);
        size_t bytesSent = 0;

        // writeToPlatformOut("SOCKET OUT: " + std::string((char*)buffer));

        while (bytesSent < numBytes) {
            ssize_t retVal = send(socketFD, bufferPtr + bytesSent, numBytes - bytesSent, 0);
            if (retVal == -1) {
                static bool once = true;
                if (once) {
                    writeToPlatformOut("CAPLOG: Couldn't write to socket. | Errno: [" +
                                       std::to_string(errno) + "] | Error String: [" +
                                       strerror(errno) + "] \n");
                    once = false;
                }
                return false;
            }

            bytesSent += (size_t)retVal;
        }
        return true;
    }

    static void writeToSocket(const std::string& output) {
        SocketLogger& logger = getSocketLogger();
        if (logger.mSocketFD != 0) {
            // TODO formalize this
            // header[0] == type, header[1] == length in bytes.
            // type = 0 == text, type = 1 == binary stream.
            Header header{};
            header.payload[2] = 0;
            header.payload[3] = static_cast<uint32_t>(output.size());

            bool success = false;
            {
                const std::lock_guard<std::mutex> guard(logger.mMut);
                if (sendBufferOverSocket(logger.mSocketFD, header.payload, sizeof(Header))) {
                    if (sendBufferOverSocket(logger.mSocketFD, output.data(), output.size())) {
                        success = true;
                    }
                }
            }

            if (!success) {
                // failed to send, likely due to a disconnection.  Close the connection to stop
                // further sends.
                logger.closeSocket();
            }
        }
    }

    // the payload of the binary stream looks like this:
    // filename||binary data
    static void writeBinaryStreamToSocket(std::string_view filename, const void* data,
                                          size_t numberOfBytes) {
        SocketLogger& logger = getSocketLogger();
        if (logger.mSocketFD != 0) {
            // TODO formalize this
            // header[0] == type, header[1] == length in bytes.
            // type = 0 == text, type = 1 == binary stream.
            Header header{};
            header.payload[2] = 1;
            std::string bodyFilenamePart = std::string(filename) + std::string("||");
            header.payload[3] = (uint32_t)(bodyFilenamePart.size() + numberOfBytes);

            bool success = false;
            {
                const std::lock_guard<std::mutex> guard(logger.mMut);
                if (sendBufferOverSocket(logger.mSocketFD, header.payload, sizeof(Header))) {
                    if (sendBufferOverSocket(logger.mSocketFD, bodyFilenamePart.data(),
                                             (uint32_t)bodyFilenamePart.size())) {
                        if (sendBufferOverSocket(logger.mSocketFD, data, numberOfBytes)) {
                            success = true;
                        }
                    }
                }
            }

            if (!success) {
                // failed to send, likely due to a disconnection.  Close the connection to stop
                // further sends.
                logger.closeSocket();
            }
        }
    }

    void reset() {
        signal(SIGPIPE, SIG_IGN);

        closeSocket();

        writeToPlatformOut("CAPLOG: Trying to connect to socket listener \n");
        mSocketFD = socket(AF_INET, SOCK_STREAM, 0);

        if (mSocketFD == -1) {
            writeToPlatformOut("CAPLOG: Failed to create CAPLOG socket.  Errno = [" +
                               std::to_string(errno) + "] | Errno Message = [" + strerror(errno) +
                               "] \n");
            return;
        } else {
            writeToPlatformOut("CAPLOG: Created CAPLOG socket.  FD is [" +
                               std::to_string(mSocketFD) + "] \n");
        }

        sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(CAPLOG_SOCKET_PORT);

        int inetRet = inet_pton(AF_INET, CAPLOG_SOCKET_HOST, &serv_addr.sin_addr);

        if (inetRet != 1) {
            writeToPlatformOut("CAPLOG: error converting network address \n");
            return;
        } else {
            writeToPlatformOut("CAPLOG: network address was successfully converted \n");
        }

        // Set socket to non-blocking mode to avoid hanging on iOS
        int flags = fcntl(mSocketFD, F_GETFL, 0);
        if (flags == -1) {
            writeToPlatformOut("CAPLOG: Failed to get socket flags. Errno: [" + 
                std::to_string(errno) + "] | Errno Message: [" + 
                strerror(errno) + "]\n");
            closeSocket();
            return;
        } 
        if (fcntl(mSocketFD, F_SETFL, flags | O_NONBLOCK) == -1) {
            writeToPlatformOut("CAPLOG: Failed to set socket flags. Errno: [" + 
                std::to_string(errno) + "] | Errno Message: [" + 
                strerror(errno) + "]\n");
            closeSocket();
            return;
        }

        int connectStatus = connect(mSocketFD, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
        writeToPlatformOut("CAPLOG: Attempted to connect to socket listener. Connect returned: [" +
                           std::to_string(connectStatus) + "] \n");

        if (connectStatus == -1) {
            if (errno == EINPROGRESS) {
                fd_set writefds;
                FD_ZERO(&writefds);
                FD_SET(mSocketFD, &writefds);

                struct timeval timeout;
                timeout.tv_sec = 0;
                timeout.tv_usec = 1000; // 1 millisecond timeout

                writeToPlatformOut("CAPLOG: Connection in progress, waiting 100 ms timeout...\n");

                // select will return > 0 if the socket is writable (connected) before the timeout, 
                // 0 if it times out, and -1 if there's an error.
                // mSocketFD + 1 is the nfds paramete which specifies the range of file descriptors
                // to be tested.  The select() function tests file descriptors in the range of 0 to nfds-1
                int selectResult = select(mSocketFD + 1, NULL, &writefds, NULL, &timeout);

                if (selectResult == 0) {
                    writeToPlatformOut("CAPLOG: Connection timed out.\n");
                    closeSocket();
                    return;
                } else if (selectResult == -1) {
                    writeToPlatformOut("CAPLOG: Select() failed. Errno: [" +
                        std::to_string(errno) + "] | Error String: [" + strerror(errno) + "]\n");
                    closeSocket();
                    return;
                } else if (!FD_ISSET(mSocketFD, &writefds)) {
                    writeToPlatformOut("CAPLOG: select() failed.  Socket not writeable \n");
                    closeSocket();
                    return;
                }
                 
                writeToPlatformOut("CAPLOG: Socket is writable, connection should be established.\n");
                    
                // Check if the connection succeeded by checking the socket error.
                int socket_error = 0;
                socklen_t len = sizeof(socket_error);
                if (getsockopt(mSocketFD, SOL_SOCKET, SO_ERROR, &socket_error, &len) == -1) {
                    writeToPlatformOut("CAPLOG: getsockopt() failed. Errno: [" +
                        std::to_string(errno) + "] | Error String: [" + strerror(errno) + "]\n");
                    closeSocket();
                    return;
                }
                if (socket_error != 0) {
                    writeToPlatformOut("CAPLOG: Socket connection failed after select. error: [" +
                        std::to_string(socket_error) + "] | Error String: [" + strerror(socket_error) + "]\n");
                    closeSocket();
                    return;
                }
                writeToPlatformOut("CAPLOG: Socket connection established after select.  | Socket: [" +
                    std::to_string(mSocketFD) + "] \n");
            } else {
                writeToPlatformOut("CAPLOG: Failed to connect to socket listener | Errno: [" +
                                std::to_string(errno) + "] | Errno Message: [" + strerror(errno) +
                                "] \n");
                // Close the socket on connect failure to prevent SIGPIPE.  Without this,
                // the socket FD remains open but unconnected.  Subsequent send() calls
                // would trigger SIGPIPE.
                closeSocket();
                return;
            }
        } else {
            writeToPlatformOut("CAPLOG: Connected to socket listener.  | Socket: [" + 
                std::to_string(mSocketFD) + "] \n");
        }

        // Restore blocking mode for subsequent operations
        if (fcntl(mSocketFD, F_SETFL, flags) == -1) {
            writeToPlatformOut("CAPLOG: Failed to restore socket flags. Errno: [" + 
                std::to_string(errno) + "] | Errno Message: [" + strerror(errno) + "]\n");
        }

        {
            union {
                uint32_t i;
                char c[4];
            } testBytes = {0x12345678};

            bool isLittleEndian = (testBytes.c[0] != 0x12);

            std::stringstream ss;
            ss << "CAPLOG: Endianness: [" << (isLittleEndian ? "little" : "big")
               << "] | TestBytes: [" << std::hex << (uint32_t)testBytes.c[0]
               << (uint32_t)testBytes.c[1] << (uint32_t)testBytes.c[2] << (uint32_t)testBytes.c[3]
               << "]" << std::endl;

            writeToPlatformOut(ss.str());

            if (!isLittleEndian) {
                writeToPlatformOut(
                        "CAPLOG: socket validation is only implemented for little endian order \n");
            }
        }
    }

  private:
    SocketLogger() { reset(); }

    void closeSocket() {
        if (mSocketFD != -1) {
            writeToPlatformOut("CAPLOG: closing socket.  Current FD value: [" +
                               std::to_string(mSocketFD) + "]\n");
            close(mSocketFD);
            mSocketFD = -1;
        }
    }

    ~SocketLogger() { closeSocket(); }

  private:
    // Streams can end up partially written, but each send needs to be "atomic".  That is,
    // header+payload needs to be kept together.
    std::mutex mMut;
    int mSocketFD = -1;
};

}  // namespace CAP

#else

namespace CAP {

class SocketLogger {
  public:
    static void writeToSocket(const std::string&) {}
    static void writeBinaryStreamToSocket(std::string_view, const void*, size_t) {}
};

}  // namespace CAP

#endif  // CAPLOG_SOCKET_ENABLED
