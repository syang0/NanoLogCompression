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

#include <cstring>
#include <cstdint>
#include <cstdio>

#include <algorithm>
#include <random>

#include "./snappy/snappy.h"
#include "zlib.h"

#include "CommonWords.h"
#include "Logger.h"

using namespace PerfUtils;

/**
 * This class keeps the state required to generate a stream of
 * random/incremented integers/doubles for use as log arguments.
 */
class ArgumentGenerator {
    std::default_random_engine generator;
    uint64_t counter;

public:
    ArgumentGenerator()
        : generator(0)
        , counter(0)
    {}

    void reset(uint64_t seed=0) {
        generator.seed(seed);
        counter = seed;
    }

    template <typename T>
    static T
    randSmallInt(ArgumentGenerator& ag) {
        std::uniform_int_distribution<uint64_t> intValDist(0, uint64_t(-1));
        std::uniform_int_distribution<int> pow_dist(1, 16);

        long mask = (1UL << pow_dist(ag.generator)) - 1;
        return mask & intValDist(ag.generator);
    }

    template <typename T>
    static T
    randBigInt(ArgumentGenerator& ag) {
        std::uniform_int_distribution<uint64_t> intValDist(0, uint64_t(-1));
        return intValDist(ag.generator);
    }

    static double
    randDouble(ArgumentGenerator& ag) {
        std::uniform_real_distribution<double> doubleValDist(0, uint64_t(-1));
        return doubleValDist(ag.generator) + 1.0/doubleValDist(ag.generator);
    }

    template <typename T>
    static T
    incSmallInt(ArgumentGenerator& ag) {
        return ((1<<16) - 1) & ag.counter++;
    }

    template <typename T>
    static T
    incRegInt(ArgumentGenerator& ag) {
        return ag.counter++;
    }

    template <typename T>
    static T
    incBigInt(ArgumentGenerator& ag) {
        static constexpr T offset = ((T)1)<<(8*(sizeof(T)/2));
        return ag.counter++ + offset;
    }

    static double
    incDouble(ArgumentGenerator& ag) {
        return ag.counter++;
    }
};

/**
 * This class maintains all the data buffers, generates the uncompressed log
 * data given constraints, and benchmarks all the compression algorithms on
 * the uncompressed data.
 */
class BenchmarkRunner {
    // Stores uncompressed log data
    unsigned char *rawDataBuffer;

    // Points to the first invalid byte in the previous buffer
    unsigned char *endOfRawDataBuffer;

    // Stores the compressed log data
    unsigned char *compressedOutputBuffer;

    // Stores the sizes of the two buffers above
    unsigned long int rawBufferSize;
    unsigned long int compressedBufferSize;

    // Maintains the state for argument generation
    ArgumentGenerator argumentGenerator;

public:
    /**
     * Stores and formats to output the important metrics recorded for a
     * particular algorithm/dataset benchmark run.
     */
    struct Result {
        // Name of the compression algorithm used
        std::string algorithm;

        // Name of the uncompressed dataset
        std::string dataset;

        // Length of the uncompressed data
        uint64_t inputBytes;

        // Length of the compressed data
        uint64_t outputBytes;

        // The number of NanoLog log statements contained
        uint32_t numLogMsgs;

        // Number of Cycles::rdtsc() cycles required to perform the compression
        uint64_t compressionCycles;

        Result(const char *algorithm, const char *dataset,
                uint64_t inputBytes, uint64_t outputBytes,
                uint32_t numLogMsgs, uint64_t compressionCycles)
                    : algorithm(algorithm)
                    , dataset(dataset)
                    , inputBytes(inputBytes)
                    , outputBytes(outputBytes)
                    , numLogMsgs(numLogMsgs)
                    , compressionCycles(compressionCycles)
        {}


        static constexpr const char *metricsOutputString =
            "%-10s%20s%10lu%15lu%15lu%10.4lf%15.6lf%15.6lf%15.6lf%20.3lf"
                    "%15.3lf%10.3lf%10.2lf\r\n";

        static void printHeader() {
            printf("#%-9s%20s%10s%15s%15s%10s%15s%15s%15s%20s%15s%10s%10s\r\n",
                "Algorithm",
                "Dataset",
                "NumLogs",
                "Input Bytes",
                "Output Bytes",
                "Ratio",
                "Compute (s)",
                "Output (s)",
                "Max (s)",
                "MB/s Processing",
                "MB/s saved",
                "Mlogs/s",
                "B/msg");
        }

        void print() {
            double computeTime = PerfUtils::Cycles::toSeconds(compressionCycles);
            double outputTime = outputBytes/(250.0*1024*1024);
            int64_t bytesSaved = inputBytes - outputBytes;

            printf(metricsOutputString,
                    algorithm.c_str(),
                    dataset.c_str(),
                    numLogMsgs,
                    inputBytes,
                    outputBytes,
                    (1.0*outputBytes)/inputBytes,
                    computeTime,
                    outputTime,
                    std::max(computeTime, outputTime),
                    inputBytes/(1024*1024*computeTime),
                    bytesSaved/(1024*1024*computeTime),
                    numLogMsgs/(1e6*computeTime),
                    outputBytes/(1.0*numLogMsgs)
                    );
        }
    };

    void printHeader() {
        Result::printHeader();
    }

    /**
     * Allocate a BenchmarkRunner with a specific uncompressed buffer size.
     * This value determines the size of the uncompressed log data, i.e. the
     * compression algorithm's input data size.
     *
     * @param bufferSize
     *      Size of the uncompresesd log data buffer
     */
    BenchmarkRunner(const unsigned long int bufferSize)
            : rawDataBuffer(nullptr)
            , endOfRawDataBuffer(nullptr)
            , compressedOutputBuffer(nullptr)
            , rawBufferSize(bufferSize)
            , compressedBufferSize(2*bufferSize)
            , argumentGenerator()
    {
        rawDataBuffer = static_cast<unsigned char*>(malloc(rawBufferSize));
        compressedOutputBuffer = static_cast<unsigned char*>(
                                                  malloc(compressedBufferSize));

        if (rawDataBuffer == nullptr || compressedOutputBuffer == nullptr) {
            fprintf(stderr, "Could not allocate input/output buffers of "
                    "size %lu and %lu bytes for compression\r\n",
                    rawBufferSize, compressedBufferSize);
            exit(-1);
        }

        bzero(rawDataBuffer, rawBufferSize);
        bzero(compressedOutputBuffer, compressedBufferSize);
        endOfRawDataBuffer = rawDataBuffer + bufferSize;
    }

    ~BenchmarkRunner() {
        if (rawDataBuffer != nullptr)
            free(rawDataBuffer);
        rawDataBuffer = nullptr;

        if (compressedOutputBuffer != nullptr)
            free(compressedOutputBuffer);
        compressedOutputBuffer = nullptr;

    }

    /**
     * Generates a NanoLog dataset with varying number of int/long/double
     * arguments, runs the various compression algorithms, and outputs the
     * result. This function is configurable via the numArgs and randFn
     * parameters which controls how many arguments and how they are generated
     * for each NanoLog log statement.
     *
     * The size of the uncompressed NanoLog log data is determined by the
     * constructor.
     *
     * @tparam T
     *      Type of arguments to generate (automatically inferred via randFn)
     * @param datasetName
     *      Name of the dataset to generate (used for printing)
     * @param numArgs
     *      Number of arguments to use per NanoLog log entry
     * @param randFn
     *      Function that generates the log arguments
     * @param runNanoLog
     *      Runs the NanoLog compression if true
     * @param runGzip
     *      Runs the gzip compression if true
     * @param runMemcpy
     *      Runs the memcpy compression if true
     * @param runSnappy
     *      Runs the snappy compression if true
     * @return
     *      Retruns a vector of Result (s), one for each of the tests run.
     */
    template <typename T>
    std::vector<Result>
    runBinaryTest(const char *datasetName, int numArgs,
                  T (*randFn)(ArgumentGenerator &),
                  bool runNanoLog = true, bool runGzip = true,
                  bool runMemcpy = true, bool runSnappy = true)
    {
        T args[MAX_ARGS];
        uint32_t numLogStatements = 0;
        unsigned char *writePtr = rawDataBuffer;
        unsigned char *endOfRawBuffer = rawDataBuffer + rawBufferSize;

        if (numArgs > MAX_ARGS) {
            fprintf(stderr, "You can only run tests with a maximum of "
                    "%d args (%d specified)\r\n", MAX_ARGS, numArgs);
            exit(-1);
        }

        // Generate the logs required
        argumentGenerator.reset();
        while (true) {
            for (int i = 0; i < numArgs; ++i) {
                args[i] = randFn(argumentGenerator);
            }

            if (!binaryLogWithArgs(&writePtr, endOfRawBuffer, numArgs, args))
                break;

            ++numLogStatements;
        }
        unsigned long int rawDataLength = writePtr - rawDataBuffer;

        return runCompressionAlgos(datasetName, rawDataLength, numLogStatements,
                                   runMemcpy, runSnappy, runGzip, runNanoLog);
    }

    /**
     * Generates NanoLog log entries using random/top1000words strings and runs
     * the various compression algorithms on them.
     *
     * @param stringLength
     *      Length of the string to generate for the log entries
     * @param runTopNWords
     *      Generate strings using combinations of the top N words on
     *      the Internet
     * @param topNWordsLimit
     *      What the value of N is for the previous argument
     * @param runRandomStrings
     *      Generate strings using random strings
     */
    void stringTest(int stringLength,
                    bool runTopNWords = true,
                    long int topNWordsLimit=-1,
                    bool runRandomStrings = false) {
        char testName[100];
        uint32_t numLogStatements;
        uint64_t rawDataLength;
        unsigned char *writePtr;

        if (runRandomStrings) {
            numLogStatements = 0;
            writePtr = rawDataBuffer;

            std::default_random_engine generator;
            std::uniform_int_distribution<char> charDist(' ', '~');
            std::string myString(stringLength + 1, '\0');
            while (true) {
                const char *args[1];
                for (int i = 0; i < stringLength; ++i) {
                    myString[i] = charDist(generator);
                }

                args[0] = myString.c_str();
                if (!binaryLogWithArgs(&writePtr, endOfRawDataBuffer, 1, args))
                    break;

                ++numLogStatements;
            }

            rawDataLength = writePtr - rawDataBuffer;
            snprintf(testName, sizeof(testName), "Rand %d Chars", stringLength);
            runCompressionAlgos(testName, rawDataLength, numLogStatements);
        }

        if (runTopNWords) {
            numLogStatements = 0;
            writePtr = rawDataBuffer;

            WordData::RandomWordGenerator rwg;
            rwg.setWordLimit(topNWordsLimit);
            while (true) {
                std::string str;

                while (str.size() < stringLength) {
                    str += rwg.getRandomWord();
                    str += ' ';
                }

                str[stringLength - 1] = '\0';
                const char *args[1] = {str.c_str()};

                if (!binaryLogWithArgs(&writePtr, endOfRawDataBuffer, 1, args))
                    break;

                ++numLogStatements;
            }

            rawDataLength = writePtr - rawDataBuffer;
            snprintf(testName, sizeof(testName), "Top1000 %d Chars",
                     stringLength);
            runCompressionAlgos(testName, rawDataLength, numLogStatements);
        }
    }
private:
    /**
 * Runs the compression algorithms, prints out and returns the Result.
 *
 * @param datasetName
 *      Name of the uncompressed dataset
 *
 * @param rawDataLength
 *      Length of the data contained within the internal rawDataBuffer
 *
 * @param numLogStatements
 *      Number of log statements contained within the rawDataBuffer
 *
 * @param runMemcpy
 *      True runs the memcpy algorithm
 * @param runSnappy
 *      True runs the snappy algorithm
 * @param runGzip
 *      True runs the gzip0,1,6,9 algorithms
 * @param runNanoLog
 *      True runs the NanoLog algorithm
 *
 * \return
 *      Result(s) for the various compression algorithms
 */
    std::vector<Result>
    runCompressionAlgos(const char *datasetName,
                        unsigned long rawDataLength,
                        uint32_t numLogStatements,
                        bool runMemcpy = true,
                        bool runSnappy = true,
                        bool runGzip = true,
                        bool runNanoLog = true)
    {
        std::vector<Result> results;
        uint64_t start, stop, compressionCycles, compressedLength;

        if (runGzip) {
            char testName[100];
            int compressionLevels[] = {0, 1, 6, 9};

            for (int level : compressionLevels) {
                start = Cycles::rdtsc();
                compressedLength = compressedBufferSize;

                int retVal = compress2(compressedOutputBuffer, &compressedLength,
                                       rawDataBuffer, rawDataLength,
                                       level);
                stop = Cycles::rdtsc();
                compressionCycles = stop - start;

                snprintf(testName, sizeof(testName), "gzip,%d", level);

                if (retVal != Z_OK) {
                    fprintf(stderr, "Compression scheme %s with input \"%s\" "
                                    "failed with error code %d\r\n",
                            testName, datasetName, retVal);
                }

                Result r(testName, datasetName, rawDataLength, compressedLength,
                         numLogStatements, compressionCycles);
                r.print();
                results.push_back(r);
            }
        }

        // Memcpy
        if (runMemcpy) {
            start = Cycles::rdtsc();
            memcpy(compressedOutputBuffer, rawDataBuffer, rawDataLength);
            stop = Cycles::rdtsc();
            compressionCycles = stop - start;

            Result r("memcpy", datasetName, rawDataLength, rawDataLength,
                     numLogStatements, compressionCycles);
            r.print();
            results.push_back(r);
        }

        // Snappy
        if (runSnappy) {
            start = Cycles::rdtsc();
            compressedLength = compressedBufferSize;
            snappy::RawCompress((char *) rawDataBuffer,
                                rawDataLength,
                                (char *) compressedOutputBuffer,
                                &compressedLength);
            stop = Cycles::rdtsc();
            compressionCycles = stop - start;

            Result r("snappy", datasetName, rawDataLength, compressedLength,
                     numLogStatements, compressionCycles);
            r.print();
            results.push_back(r);
        }


        if (runNanoLog) {
            start = Cycles::rdtsc();
            compressedLength = compressedBufferSize;
            NanoLogCompress2(compressedOutputBuffer, &compressedLength,
                             rawDataBuffer, rawDataLength);
            stop = Cycles::rdtsc();
            compressionCycles = stop - start;

            Result r("NanoLog", datasetName, rawDataLength, compressedLength,
                     numLogStatements, compressionCycles);
            r.print();
            results.push_back(r);

            if (runSnappy) {
                unsigned long int snappyOutputBytes = compressedBufferSize;
                unsigned char *snappyOutputBuffer = static_cast<unsigned char*>(
                        malloc(compressedBufferSize));
                bzero(snappyOutputBuffer, compressedBufferSize);

                start = Cycles::rdtsc();
                snappy::RawCompress((char *) compressedOutputBuffer,
                                    compressedLength,
                                    (char *) snappyOutputBuffer,
                                    &snappyOutputBytes);
                stop = Cycles::rdtsc();
                compressionCycles += stop - start;

                Result r("NL+snappy", datasetName, rawDataLength,
                         snappyOutputBytes, numLogStatements, compressionCycles);
                r.print();
                results.push_back(r);

                free(snappyOutputBuffer);
            }
        }

        printf("\r\n");
        return results;
    }

public:

    // Maximum number of int/long/double arguments allowed in the log statements
    static const unsigned int MAX_ARGS = 50;
};

int main(int argc, char **argv) {
    if (argc > 1) {
        printf("This application measures the performance of different "
               "compression algorithms on NanoLog log data.\r\n"
               "Usage:\r\n"
               "\t%s\r\n\r\n", argv[0]);
        return 1;
    }

    /**
     * In this benchmark we need to vary the following variables:
     *
     * 1) Number of arguments/string length
     * 2) Type: small/big int/longs, doubles, strings
     * 3) Entropy of data (random, increment, hot)
     */
    const int rawInputDataSize = 1024*1024*64; // 64MB
    BenchmarkRunner runner(rawInputDataSize);
    runner.printHeader();

    // First, run all the binary data types (int/long/doubles)
    char datasetName[100];
    int numberOfArguments[] = {1, 2, 3, 4, 6, 10};
    for (int numArgs : numberOfArguments) {
        // Random Arguments
        snprintf(datasetName, 100, "Rand Small %d Int", numArgs);
        runner.runBinaryTest(datasetName, numArgs,
                             &ArgumentGenerator::randSmallInt<int>);

        snprintf(datasetName, 100, "Rand Big %d Int", numArgs);
        runner.runBinaryTest(datasetName, numArgs,
                             &ArgumentGenerator::randBigInt<int>);

        snprintf(datasetName, 100, "Rand Small %d Long", numArgs);
        runner.runBinaryTest(datasetName, numArgs,
                             &ArgumentGenerator::randSmallInt<long>);

        snprintf(datasetName, 100, "Rand Big %d Long", numArgs);
        runner.runBinaryTest(datasetName, numArgs,
                             &ArgumentGenerator::randBigInt<long>);

        snprintf(datasetName, 100, "Rand %d Double", numArgs);
        runner.runBinaryTest(datasetName, numArgs,
                             &ArgumentGenerator::randDouble);

        // Incremented Arguments
        snprintf(datasetName, 100, "Incr Small %d Int", numArgs);
        runner.runBinaryTest(datasetName, numArgs,
                             &ArgumentGenerator::incSmallInt<int>);

        snprintf(datasetName, 100, "Incr Reg %d Int", numArgs);
        runner.runBinaryTest(datasetName, numArgs,
                             &ArgumentGenerator::incRegInt<int>);

        snprintf(datasetName, 100, "Incr Big %d Int", numArgs);
        runner.runBinaryTest(datasetName, numArgs,
                             &ArgumentGenerator::incBigInt<int>);

        snprintf(datasetName, 100, "Incr Small %d Long", numArgs);
        runner.runBinaryTest(datasetName, numArgs,
                             &ArgumentGenerator::incSmallInt<long>);

        snprintf(datasetName, 100, "Incr Reg %d Long", numArgs);
        runner.runBinaryTest(datasetName, numArgs,
                             &ArgumentGenerator::incRegInt<long>);

        snprintf(datasetName, 100, "Incr Big %d Long", numArgs);
        runner.runBinaryTest(datasetName, numArgs,
                             &ArgumentGenerator::incBigInt<long>);

        snprintf(datasetName, 100, "Incr %d Double", numArgs);
        runner.runBinaryTest(datasetName, numArgs,
                             &ArgumentGenerator::incDouble);
    }

    // Run the ASCII tests, varying...
    // 1) string length (say 10, 20, 40)
    // 2) entropy (psuedo-random words by top 1000)

    int stringLengths[] = {10, 20, 40};
    for (int length : stringLengths) {
        runner.stringTest(length, 1000);
    }

    fflush(stdout);

    return 0;
}