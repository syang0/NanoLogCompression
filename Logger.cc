/* Copyright (c) 2018 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "Logger.h"

/**
 * This file implements some features in the Logger.h file
 */

// See Header
int NanoLogCompress2(unsigned char *outputBuffer,
                     long unsigned int *outputSize,
                     const unsigned char *inputBuffer,
                     long unsigned int inputSize,
                     int compressionLevel)
{
    using namespace NanoLogInternal;
    using namespace LoggerInternals;

    const unsigned char *readPos = inputBuffer;
    unsigned char *writePos = outputBuffer;

    uint64_t lastTime = 0;
    while (readPos < inputBuffer + inputSize) {
        auto metadata =reinterpret_cast<const Log::UncompressedEntry*>(readPos);
        readPos += sizeof(Log::UncompressedEntry);

        Log::compressLogHeader(metadata, (char**)&writePos, lastTime);
        lastTime = metadata->timestamp;

        int argSize = metadata->entrySize - sizeof(Log::UncompressedEntry);
        if (argSize > 0) {
            if (metadata->fmtId < LOG_ID_INT_ARGS_START) {
                // Strings are incompressible, so we just memcpy them
                memcpy(writePos, readPos, argSize);
                writePos += argSize;
            } else if (metadata->fmtId < LOG_ID_LONG_ARGS_START) {
                int numInts =  metadata->fmtId - LOG_ID_INT_ARGS_START;
                auto *args = reinterpret_cast<const int*>(metadata->argData);

                int i = 0;
                while (i < numInts) {
                    auto twoNibbles = reinterpret_cast<
                                            BufferUtils::TwoNibbles*>(writePos);
                    writePos += sizeof(BufferUtils::TwoNibbles);

                    twoNibbles->first =
                            BufferUtils::pack((char**) &writePos, args[i]);

                    if (++i >= numInts) break;

                    twoNibbles->second =
                            BufferUtils::pack((char**) &writePos, args[i]);
                    ++i;
                }
            } else if (metadata->fmtId < LOG_ID_DBL_ARGS_START) {
                long numLongs =  metadata->fmtId - LOG_ID_LONG_ARGS_START;
                auto *args = reinterpret_cast<const long*>(metadata->argData);

                int i = 0;
                while (i < numLongs) {
                    BufferUtils::TwoNibbles* twoNibbles = reinterpret_cast<
                                            BufferUtils::TwoNibbles*>(writePos);
                    writePos += sizeof(BufferUtils::TwoNibbles);

                    twoNibbles->first =
                            BufferUtils::pack((char**) &writePos, args[i]);

                    if (++i >= numLongs) break;

                    twoNibbles->second =
                            BufferUtils::pack((char**) &writePos, args[i]);
                    ++i;
                }
            } else {
                // Doubles are incompressible, so just copy it.
                memcpy(writePos, readPos, argSize);
                writePos += argSize;
            }

            readPos += argSize;
        }
    }

    if (outputBuffer + *outputSize < writePos) {
        fprintf(stderr, "Ran out of space in the output buffer\r\n");
        return Z_BUF_ERROR;
    }

    *outputSize = writePos - outputBuffer;
    return Z_OK;
}

// See Header
void NanoLogDecompress(const char *inputBuffer, long unsigned int inputSize)
{
    using namespace LoggerInternals;

    const char *endOfBuffer = inputBuffer + inputSize;
    uint64_t lastTimestamp = 0;

    while (endOfBuffer > inputBuffer) {
        uint32_t logId;
        uint64_t timestamp;
        NanoLogInternal::Log::decompressLogHeader(&inputBuffer,
                                                  lastTimestamp,
                                                  logId,
                                                  timestamp);
        uint64_t timeDelta = timestamp - lastTimestamp;
        lastTimestamp = timestamp;

        if (logId < LOG_ID_INT_ARGS_START) {
            uint32_t numStrings = logId - LOG_ID_STRING_START;
            printf("Found at %llu (+%llu) timestamp %u strings:\r\n",
                       timestamp, timeDelta, numStrings);
            for (int i = 0; i < numStrings; ++i) {
                printf("\t%d: %s\r\n", i, inputBuffer);
                inputBuffer += strlen(inputBuffer) + 1;
            }
        } else if (logId < LOG_ID_LONG_ARGS_START) {
            int numArgs = logId - LOG_ID_INT_ARGS_START;
            printf("Found at %llu (+%llu) timestamp %u ints:\r\n",
                       timestamp, timeDelta, numArgs);

            int i = 0;
            while (i < numArgs) {
                int result;
                auto twoNibbles = reinterpret_cast<
                                   const BufferUtils::TwoNibbles*>(inputBuffer);
                inputBuffer += sizeof(BufferUtils::TwoNibbles);

                result = BufferUtils::unpack<int>(&inputBuffer,
                                                  twoNibbles->first);
                printf("\t%d: %d\r\n", i, result);

                if (++i >= numArgs) break;

                result = BufferUtils::unpack<int>(&inputBuffer,
                                                  twoNibbles->second);
                printf("\t%d: %d\r\n", i, result);
                ++i;
            }
        } else if (logId < LOG_ID_DBL_ARGS_START) {
            int numArgs = logId - LOG_ID_LONG_ARGS_START;
            printf("Found at %llu (+%llu) timestamp %lu longs:\r\n",
                       timestamp, timeDelta, numArgs);

            int i = 0;
            while (i < numArgs) {
                long result;
                auto* twoNibbles = reinterpret_cast<
                                   const BufferUtils::TwoNibbles*>(inputBuffer);
                inputBuffer += sizeof(BufferUtils::TwoNibbles);

                result = BufferUtils::unpack<long>(&inputBuffer,
                                                   twoNibbles->first);
                printf("\t%d: %ld\r\n", i, result);

                if (++i >= numArgs) break;

                result = BufferUtils::unpack<long>(&inputBuffer,
                                                   twoNibbles->second);
                printf("\t%d: %ld\r\n", i, result);
                ++i;
            }
        } else if (logId < LOG_ID_MAX_ARGS) {
            int numArgs = logId - LOG_ID_DBL_ARGS_START;
            printf("Found at %llu (+%llu) timestamp %lu doubles:\r\n",
                       timestamp, timeDelta, numArgs);

            auto args = reinterpret_cast<const double*>(inputBuffer);
            for (int i = 0; i < numArgs; ++i)
                printf("\t%d: %lf\r\n", i, args[i]);

            inputBuffer += numArgs*sizeof(double);
        } else {
            printf("Malformed data!\r\n");
        }
    }
}

// See Header
void simpleTest() {
    unsigned int bufferSize = 1024*1024*1;

    auto *origStartingBuffer = static_cast<unsigned char*>(malloc(bufferSize));
    auto *origCompressedBuffer = static_cast<unsigned char*>(malloc(bufferSize));
    unsigned char *startingBuffer = origStartingBuffer;
    unsigned char *compressedBuffer = origCompressedBuffer;
    unsigned char *endOfStartingBuffer = startingBuffer + bufferSize;

    int counter = 0;
    // Log some ints
    for (int i = 0; i < 10; ++i) {
        int nums[10];
        for (int j = 0; j < i; ++j)
            nums[j] = ++counter;

        binaryLogWithArgs(&startingBuffer, endOfStartingBuffer, i, nums);
    }

    // log some longs
    for (int i = 0; i < 10; ++i) {
        long nums[10];
        for (int j = 0; j < i; ++j)
            nums[j] = ++counter + 1000;

        binaryLogWithArgs(&startingBuffer, endOfStartingBuffer, i, nums);
    }

    // log some strings
    const char* strings[] = {
            "First string",
            "Second string",
            "Third one",
            "Fourth",
            "And so on"
    };

    binaryLogWithArgs(&startingBuffer, endOfStartingBuffer, 0, strings);
    binaryLogWithArgs(&startingBuffer, endOfStartingBuffer, 4, strings);
    binaryLogWithArgs(&startingBuffer, endOfStartingBuffer, 1, &(strings[4]));

    long unsigned int compressedBufferSize = bufferSize;
    long unsigned int uncompressedBufferDatalen = startingBuffer - origStartingBuffer;
    NanoLogCompress2(compressedBuffer, &compressedBufferSize,
                     origStartingBuffer, uncompressedBufferDatalen);

    NanoLogDecompress((const char*)compressedBuffer, compressedBufferSize);

    printf("\r\n\r\nUncompressed size was %ld\r\n", uncompressedBufferDatalen);
    printf("Compressed size was %ld\r\n", compressedBufferSize);
}