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

#include <cstdint>

#include <random>

/**
 * This class generates random words at the frequency in which they appear on
 * the internet according to a NLP dataset curated by Peter Norving
 * (http://norvig.com/ngrams/).
 *
 * Invoke RandomWordGenerator::getMaxWordLimit() to get how many unique words
 * this class contains.
 */

namespace WordData {
extern unsigned long int numUniqueWords;
class RandomWordGenerator {
    // Keeps the state of the PRNG
    std::default_random_engine generator;

    // Limits the words the class generates to the top N most common words.
    unsigned long int wordLimit;

public:
    /**
     * Create a RandomWordGenerator with a seed
     */
    explicit RandomWordGenerator(unsigned int seed=0)
            : generator(seed)
            , wordLimit(numUniqueWords) {}

    /**
     * Resets the state of the word generator
     *
     * \param seed
     *      optional seed parameter
     */
    void reset(unsigned int seed) {
        generator.seed(seed);
    }

    /**
     * Returns how many unique words the class can generate
     */
    static long int getMaxWordLimit() {
        return numUniqueWords;
    }

    /**
     * Limits the RandomWordGenerate to the top /limit/ words. The
     * value should be between [1, getMaxWordLimit] inclusive.
     *
     * \param limit
     *      Limit to set
     * \return
     *      Actual limit set
     */
    long int setWordLimit(long int limit) {
        wordLimit = limit;

        if (wordLimit <= 0 || wordLimit > numUniqueWords)
            wordLimit = numUniqueWords;

        return wordLimit;
    }

    /**
     * Returns a random word. The frequency at which words appear is directly
     * proportional their occurence on the Internet.
     *
     * \return
     *      A pointer to a statically allocated word (valid for the lifetime of
     *      the program)
     */
    const char* getRandomWord();
};
}; // namesace WordData
