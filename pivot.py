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
import os

logDir = "details"

class Result:
  def __init__(self, line):
    self.algorithm    = line[00:10].strip()
    self.dataset      = line[10:30].strip()
    self.numLogs      = line[30:40].strip()
    self.inputBytes   = line[40:55].strip()
    self.outputBytes  = line[55:70].strip()
    self.ratio        = line[70:80].strip()
    self.computeTime  = line[80:95].strip()
    self.outputTime   = line[95:110].strip()
    self.maxTime      = line[110:125].strip()
    self.processingBW = line[125:145].strip()
    self.savedBW      = line[145:160].strip()
    self.logBW        = line[160:170].strip()
    self.avgMsgSize   = line[170:180].strip()
    self.line         = line.strip()

  @staticmethod
  def parseLine(line):
    if len(line) < 170:
      return None
    return Result(line)

  @staticmethod
  def validLine(line):
    if len(line) < 170:
      return False
    return True

def runBandwidthCalculations(dataset2results, listOfAlgorithms, filename):
  if len(dataset2results) == 0:
    print "Warning %s has no data\r\n" % filename
    return

  if not os.path.exists(logDir):
    os.makedirs(logDir)

  with open(logDir + "/" + filename + '.txt', 'w') as oFile, \
       open(logDir + "/" + filename + "-detailed.dat", 'w') as dFile:
    lastBestAlgo = "dummy"
    datsetsConsidered = set()

    print "Running Bandwith Calculations for %s" % filename

    oFile.write("%-10s" % "MB/s")
    for algorithm in listOfAlgorithms:
      oFile.write("%10s" % algorithm)
    oFile.write("\r\n");

    lastWins = {}
    for bandwidthMBs in range(1,500, 1) + range(500, 5000, 50)  + range(5000, 100000, 100):
      wins = {}
      for algorithm in listOfAlgorithms:
        wins[algorithm] = []

      for dataset in dataset2results:
        datsetsConsidered.add(dataset)
        bestAlgo = "dummy"
        bestAlgoTime = 100000000000000000000

        for result in dataset2results[dataset]:
          if result.algorithm not in listOfAllAlgorithms:
            continue

          computeTime = float(result.computeTime)
          outputTime = float(result.outputBytes)/(bandwidthMBs*1024*1024)
          maxTime = max(computeTime, outputTime)

          if bestAlgoTime > maxTime:
            bestAlgoTime = maxTime
            bestAlgo = result.algorithm

        wins[bestAlgo].append(dataset)
        # print "\tBest was %s at %f seconds at BW %d for %s" % (bestAlgo, bestAlgoTime, bandwidthMBs, dataset)

      bestAlgo = "dummy"
      bestAlgoCnt = -1

      oFile.write("%-10d" % bandwidthMBs)
      for algorithm in listOfAlgorithms:
        if len(wins[algorithm]) > bestAlgoCnt:
          bestAlgoCnt = len(wins[algorithm])
          bestAlgo = algorithm

        oFile.write("%10d" % len(wins[algorithm]))
      oFile.write("\r\n")

      # output detailed file
      if lastWins != wins:
        dFile.write("%-10d\r\n" % bandwidthMBs)
        winCounts = [len(v) for k, v in wins.iteritems()]
        winCounts = sorted(winCounts, reverse=True)

        for winCount in winCounts:
          if winCount == 0:
            continue

          for k,v in wins.iteritems():
            if len(v) == winCount:
              dFile.write("\t%s:%s\r\n\r\n" % (k, sorted(v)))
        dFile.write("\r\n")


        # # Debug Info
        # print "For Algo %s we got: " % algorithm,
        # print wins[algorithm]

      # Prints deltas
      if (lastBestAlgo != bestAlgo):
        print "Best Algorithm starting at %6d MB/s with %0.2lf%% win rate is %s" % (bandwidthMBs, len(wins[bestAlgo])*100.0/len(datsetsConsidered), bestAlgo)
        lastBestAlgo = bestAlgo

      lastWins = wins

    print ""
    oFile.write("# Data sets included in this calculation: ")
    oFile.write("%s\r\n" % sorted(list(datsetsConsidered)))

with open("results.txt", 'r') as iFile:
  lines = iFile.readlines()

  # Parse the first line to make sure we have our headers right.
  headerLine = Result(lines[0])
  if headerLine.algorithm     != "#Algorithm": print "Header Error - Expected: Algorithm, Found: " + headerLine.algorithm
  if headerLine.dataset       != "Dataset"  : print "Header Error - Expected: Dataset, Found:" + headerLine.dataset
  if headerLine.numLogs       != "NumLogs"  : print "Header Error - Expected: NumLogs, Found:" + headerLine.numLogs
  if headerLine.inputBytes    != "Input Bytes"  : print "Header Error - Expected: Input Bytes, Found:" + headerLine.inputBytes
  if headerLine.outputBytes   != "Output Bytes"  : print "Header Error - Expected: Ouput Bytes, Found:" + headerLine.outputBytes
  if headerLine.ratio         != "Ratio"  : print "Header Error - Expected: Ratio, Found:" + headerLine.ratio
  if headerLine.computeTime   != "Compute (s)"  : print "Header Error - Expected: Compute (s), Found:" + headerLine.computeTime
  if headerLine.outputTime    != "Output (s)"  : print "Header Error - Expected: Output (s), Found:" + headerLine.outputTime
  if headerLine.maxTime       != "Max (s)"  : print "Header Error - Expected: Max (s), Found:" + headerLine.maxTime
  if headerLine.processingBW  != "MB/s Processing"  : print "Header Error - Expected: MB/s Processing, Found:" + headerLine.processingBW
  if headerLine.savedBW       != "MB/s saved"  : print "Header Error - Expected: MB/s saved, Found:" + headerLine.savedBW
  if headerLine.logBW         != "Mlogs/s"  : print "Header Error - Expected: Mlogs/s, Found:" + headerLine.logBW
  if headerLine.avgMsgSize    != "B/msg"  : print "Header Error - Expected: B/msg, Found:" + headerLine.avgMsgSize

  allResults = [Result.parseLine(line) for line in lines[1:] if Result.validLine(line)]

  print allResults[0].algorithm + allResults[0].dataset, allResults[0].numLogs, allResults[0].computeTime,

  # Output all the binary files
  #
  # logResults = {
  #   distribution: {
  #     frequency: {
  #       algorithm:data
  #     }
  #   }
  # }

  logResults = {}
  algorithms = set()
  frequencies = set()
  dataset2results = {}

  matcher = re.compile("([BA]\d+-\d+)-([^ ]+)")
  for res in allResults:
    m = matcher.match(res.dataset)
    if m:
      distribution = m.group(1)
      frequency = m.group(2)
      algorithm = res.algorithm
      time = res.totalTime

      frequencies.add(frequency)

      if not logResults.get(distribution):
        logResults[distribution] = {}

      if not logResults.get(distribution).get(frequency):
        logResults[distribution][frequency] = {}

      if not logResults.get(distribution).get(frequency).get(algorithm):
        logResults[distribution][frequency][algorithm] = res

    algorithms.add(res.algorithm)
    if not dataset2results.get(res.dataset):
      dataset2results[res.dataset] = [res]
    else:
      dataset2results[res.dataset].append(res)

  algorithms = sorted(list(algorithms))
  frequencies = sorted(list(frequencies))

  for dist in logResults:
    with open(logDir + "/" + dist + ".txt", 'w') as oFile:
      oFile.write("# " + dist + "\r\n")
      oFile.write("Frequency ")
      for algorithm in algorithms:
        oFile.write(algorithm + " ")

      oFile.write("\r\n")
      for frequency in frequencies:
        algo2data = logResults.get(dist).get(frequency)
        if algo2data:
          oFile.write(frequency + " ")
          for algorithm in algorithms:
            if algo2data.get(algorithm):
              res = algo2data.get(algorithm)

              maxTime = max(float(res.computeTime), float(res.outputTime))
              oFile.write("%10.5f " % maxTime)
            else:
              oFile.write("0 ")
        oFile.write("\r\n")
      oFile.write("\r\n")



# Analysis part
algorithms.remove("NL+snappy")
listOfAllAlgorithms = sorted(list(algorithms))
print listOfAllAlgorithms

runBandwidthCalculations(dataset2results, listOfAllAlgorithms, "all")

# Filter set by random arguments only
randArgsOnly = {k:v for k,v in dataset2results.iteritems() if k.startswith("Rand")}
runBandwidthCalculations(randArgsOnly, listOfAllAlgorithms, "e_rand")

# Filter set by incr arguments only
randArgsOnly = {k:v for k,v in dataset2results.iteritems() if k.startswith("Incr")}
runBandwidthCalculations(randArgsOnly, listOfAllAlgorithms, "e_incr")

# Filter set by small arguments only
randArgsOnly = {k:v for k,v in dataset2results.iteritems() if re.match(".*Small.*", k)}
runBandwidthCalculations(randArgsOnly, listOfAllAlgorithms, "s_small")

# Filter set by regular arguments only
randArgsOnly = {k:v for k,v in dataset2results.iteritems() if re.match(".*Reg.*", k)}
runBandwidthCalculations(randArgsOnly, listOfAllAlgorithms, "s_reg")

# Filter set by big arguments only
randArgsOnly = {k:v for k,v in dataset2results.iteritems() if re.match(".*Big.*", k)}
runBandwidthCalculations(randArgsOnly, listOfAllAlgorithms, "s_big")

# Filter set by double arguments only
randArgsOnly = {k:v for k,v in dataset2results.iteritems() if re.match(".*Double.*", k)}
runBandwidthCalculations(randArgsOnly, listOfAllAlgorithms, "t_double")

# Filter set by int arguments only
randArgsOnly = {k:v for k,v in dataset2results.iteritems() if re.match(".*Int.*", k)}
runBandwidthCalculations(randArgsOnly, listOfAllAlgorithms, "t_int")

# Filter set by long arguments only
randArgsOnly = {k:v for k,v in dataset2results.iteritems() if re.match(".*Long.*", k)}
runBandwidthCalculations(randArgsOnly, listOfAllAlgorithms, "t_long")

# Filter set by string arguments only
randArgsOnly = {k:v for k,v in dataset2results.iteritems() if re.match(".*Chars.*", k)}
runBandwidthCalculations(randArgsOnly, listOfAllAlgorithms, "t_string")





