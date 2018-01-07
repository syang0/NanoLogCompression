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
#ifndef COMPRESSION_LOGGER_H
#define COMPRESSION_LOGGER_H

/**
 * This header manages the data layout for logging NanoLog binary data. It
 * is pretty close to the record() (i.e. NANO_LOG call) layout, but
 * approximates the compression algorithm. There are two differences between
 * the NanoLog compression algorithm and this (1) unique functions are not
 * generated for every unique log invocation and instead we group them into
 * argument types and (2) we use for-loops instead of inlining code if there
 * are multiple arguments to read in. This design decision was made so that
 * the resulting benchmark code can be more compact with for loops (the
 * alternative would require exhaustively including all possible combinations
 * in the source files).
 */

#include <cstdint>
#include <zlib.h>

#include "Cycles.h"
#include "Log.h"
#include "Packer.h"
#include "../../NanoLog/runtime/Log.h"


// This namespace is meant to be internally used by Logger; they are included
// here due to templating rules. Scroll down to the end of the namespace for
// the public API.
namespace LoggerInternals {

// Allocate a range of LogIds for integer and double arg log statements so we
// can use the lower bits inbetween to encode the number of arguments.
static const uint32_t LOG_ID_MAX_ARGS = 64;
static const uint32_t LOG_ID_STRING_START = 0;
static const uint32_t LOG_ID_INT_ARGS_START = 64;
static const uint32_t LOG_ID_LONG_ARGS_START = 128;
static const uint32_t LOG_ID_DBL_ARGS_START = 192;

// Returns the starting log id for a given type of argument.
static constexpr uint32_t getLogIdStart(const char *dummy) {
    return LOG_ID_STRING_START;
}

static constexpr uint32_t getLogIdStart(int dummy) {
    return LOG_ID_INT_ARGS_START;
}

static constexpr uint32_t getLogIdStart(long dummy) {
    return LOG_ID_LONG_ARGS_START;
}

static constexpr uint32_t getLogIdStart(double dummy) {
    return LOG_ID_DBL_ARGS_START;
}

/**
* Stores an array of arguments into a buffer.
*
* @tparam T
*      Inferred argument type
* @param buffer
*      Buffer to store arguments into
* @param numArgs
*      Number of arguments in the argument buffer
* @param args
*      Argument buffer
*/
template<typename T>
static inline typename std::enable_if<!std::is_pointer<T>::value, void>::type
pushArgs(unsigned char **buffer, int numArgs, T *args) {
    for (int i = 0; i < numArgs; ++i) {
        *((T *) *buffer) = args[i];
        (*buffer) += sizeof(T);
    }
}

static void
pushArgs(unsigned char **buffer, int numArgs, const char **args) {
    for (int i = 0; i < numArgs; ++i) {
        // +1 for null character
        *buffer =
                (unsigned char *) (stpcpy((char *) (*buffer), args[i])) + 1;
    }
}

/**
* Returns the byte size of all the elements in an array of elements.
*
* @tparam T
*      Type of the arguments stored on the array
* @param numArgs
*      The number of arguments stored on the array
* @param args
*      The argument array
* @return
*      The byte size of the arguments
*/
template<typename T>
static inline typename std::enable_if<!std::is_pointer<T>::value, uint32_t>::type
getArgSize(int numArgs, T *args) {
    return numArgs * sizeof(T);
}

static uint32_t
getArgSize(int numArgs, const char **args) {
    uint32_t size = 0;
    for (int i = 0; i < numArgs; ++i) {
        size += strlen(args[i]) + 1;
    }

    return size;
}
}; // namespace LoggerInternals

/**
 * Create a binary NanoLog log entry in BufferIn containing a variable number
 * of int/long/double arguments (up to 64).
 *
 * @param[in/out] bufferIn
 *      Pointer to a buffer to write the log entry into (pointer will be
 *      incremented after write)
 * @param endOfBuffer
 *      A pointer to the end of the bufferIn
 * @param numArgs
 *      Number arguments to place in the log entry
 * @param args
 *      An array of arguments (int/long/double) to place into the array
 * @return
 *      true if successful, false means disregard data.
 */
template <typename ArgumentType>
bool binaryLogWithArgs(unsigned char **bufferIn, unsigned char *endOfBuffer,
                       int numArgs, ArgumentType *args)
{
    using namespace NanoLogInternal::Log;
    using namespace LoggerInternals;

    // First make sure we have enough space in the buffer
    uint64_t remainingSpace = endOfBuffer - *bufferIn;
    uint32_t logIdStart = getLogIdStart(args[0]);
    uint32_t bytesRequired = sizeof(UncompressedEntry) +getArgSize(numArgs,
                                                                   args);

    if (remainingSpace < bytesRequired)
        return false;

    // If you hit this assertion, you used too many arguments.
    if (numArgs >= LOG_ID_MAX_ARGS) {
        fprintf(stderr, "Used too many arguments with binaryLogWithIntArgs %d",
                numArgs);
        exit(1);
    }

    auto meta = reinterpret_cast<UncompressedEntry*>(*bufferIn);
    *bufferIn += sizeof(UncompressedEntry);

    meta->timestamp = PerfUtils::Cycles::rdtsc();
    meta->fmtId = logIdStart + numArgs;
    meta->entrySize = bytesRequired;

    if (numArgs > 0)
        pushArgs(bufferIn, numArgs, args);

    return true;
}

/**
 * Applies the NanoLog compaction scheme to data produced by
 * binaryLogWithArgs() in inputBuffer and outputs it to outputBuffer.
 * It has the same API as zlib's compress function except the compressionLevel
 * parameter is unused.
 *
 * \param outputBuffer
 *      Ouptut buffer to store the NanoLog compacted output
 * \param *ouputSize
 *      Initially set by the caller to indicate the size of the input buffer to
 *      fill with messages. On return, it is set to the number of bytes actually
 *      used in the buffer.
 * \param inputBuffer
 *      Buffer that contains data generated by the generateRawBinaryData() call
 * \param inputSize
 *      Number of bytes to consume in the inputBuffer
 * \param compressionLevel
 *      Unused; only here to match zlib's compress() API
 *
 * \return
 *      Same as libz's return status's
 */
int NanoLogCompress2(unsigned char *outputBuffer, long unsigned int *outputSize,
                     const unsigned char *inputBuffer, long unsigned int inputSize,
                     int compressionLevel=0);

/**
 * Primarily used as a debug function, takes a buffer with NanoLog log entries
 * and outputs their content to stdout for human consumption.
 *
 * @param inputBuffer
 *      Buffer containing the NanoLog log entries
 * @param inputSize
 *      Number of valid bytes in the buffer
 */
void NanoLogDecompress(const char *inputBuffer,
                       long unsigned int inputSize);

/**
 * Simple test that exercises the NanoLog log, compression, and decompression
 * functions. The decompression function results are printed to stdout and it's
 * up to the caller to read the code below and figure out whether the output
 * is correct or not.
 */
void simpleTest();

#endif //COMPRESSION_LOGGER_H
