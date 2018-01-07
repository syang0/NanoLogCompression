#! /usr/bin/python

# Copyright (c) 2018 Stanford University
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

import re
import os.path
import urllib

# This file takes the 100k most common words from http://norvig.com/ngrams/
# and generates a .cc file where we can query for the top 1k for at random
# weighted by their actual likelihood of appearance.

# First download the dictionary, if we don't already have it
if not os.path.isfile("count_1w100k.txt"):
  urllib.urlretrieve("http://norvig.com/ngrams/count_1w100k.txt", "count_1w100k.txt")

with open("count_1w100k.txt", 'r') as iFile, open("CommonWords.cc", 'w') as oFile:

  oFile.write("""
/**
 *           ** WARNING ** This is a generated file!!!
 *
 * Do not directly edit this file. Instead, edit it via transform.py
 *
 */
 
#include <cstdint>

#include <random>

#include "CommonWords.h"

namespace WordData {

/**
 * This file creates a virtual word array whereby the number of slots a word takes up
 * is directly proportional to how likely it is to appear in the dataset provided by
 * Peter Norving (http://norvig.com/ngrams/). The goal here is to create a structure
 * where if we uniform randomly indexed into the array, the random words we'd get out
 * of it would reflect the actual distribution of words in dataset.
 *
 * What we end up with is an physical array of WordIndex objects where each WordIndex
 * stores a word from the dataset and a range of virtual indecies it occupies in the form
 * of an endIndex (the start index is implicitly starts from the endIndex of the
 * WordIndex that preceded it).
 */

struct WordIndex {
  const char *word;             // Word from the dataset
  unsigned long int endIndex;   // The end of the range of Indecies this word occupies
};

static struct WordIndex OccuranceMap[] =
{
""")

  matcher = re.compile("([^ ]+)\s+(\d+)")

  numWords = 0
  cumulativeCount = 0
  for line in iFile.readlines():
    m = matcher.match(line)
    if m:
      word = m.group(1)
      count = int(m.group(2))

      numWords = numWords + 1
      cumulativeCount = cumulativeCount + count
      oFile.write("\t{\"%s\", %d },\r\n" % (word, cumulativeCount))

  oFile.write("""\
};

// Maximum index in our virtual array
unsigned long int maxEndIndex = %d;

// Maximum index in our physical array
unsigned long int numUniqueWords = %d;

const char*
RandomWordGenerator::getRandomWord() {
    uint64_t indexLimit = OccuranceMap[wordLimit - 1].endIndex;
    std::uniform_int_distribution<uint64_t> indexDist(0, indexLimit);
    uint64_t randomIndex = indexDist(generator);

    // Linear search through our list to find the word at the appropriate occurrence index
    // Note: we could do binary search, but the indexes are exponentially distributed,
    // which means a large number of them will appear at the front of the list. So let's just
    // do the simple thing until we need something faster.

    for (int i = 0; i < numUniqueWords; ++i) {
      if (randomIndex < OccuranceMap[i].endIndex)
        return OccuranceMap[i].word;
    }

    return OccuranceMap[0].word;
  }

}; // namesace WordData
""" % (cumulativeCount, numWords))


