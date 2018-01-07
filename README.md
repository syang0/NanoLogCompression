# NanoLog Compression Comparison

This is repository implements a benchmark that compares [NanoLog's](https://github.com/PlatformLab/NanoLog) compression algorithm with the ones provided by [zlib](https://zlib.net) and [Google snappy](https://github.com/google/snappy).

## Requirements
* [zlib](https://zlib.net) must be installed on your benchmarking platform
* C++11 compatible compiler
* gnuplot 5.0+

## Usage
### Quick Start

```bash
./run.sh
```

After the application ```./run.sh``` completes, there will be a ```results.txt``` file that is a copy of the stdout printed by the application and should look like the sample below.
```
#Algorithm             Dataset   NumLogs    Input Bytes   Output Bytes     Ratio    Compute (s)     Output (s)        Max (s)     MB/s Processing     MB/s saved   Mlogs/s     B/msg
gzip,0        Rand Small 1 Int   3355443       67108860       67119106    1.0002       0.315189       0.256039       0.315189             203.053         -0.031    10.646     20.00
gzip,1        Rand Small 1 Int   3355443       67108860       20364631    0.3035       0.870902       0.077685       0.870902              73.487         51.187     3.853      6.07
gzip,6        Rand Small 1 Int   3355443       67108860       18299453    0.2727       2.695115       0.069807       2.695115              23.747         17.271     1.245      5.45
gzip,9        Rand Small 1 Int   3355443       67108860       18140019    0.2703      24.348063       0.069199      24.348063               2.629          1.918     0.138      5.41
memcpy        Rand Small 1 Int   3355443       67108860       67108860    1.0000       0.019985       0.256000       0.256000            3202.426          0.000   167.899     20.00
snappy        Rand Small 1 Int   3355443       67108860       28757320    0.4285       0.109718       0.109700       0.109718             583.313        333.353    30.582      8.57
NanoLog       Rand Small 1 Int   3355443       67108860       18840077    0.2807       0.046341       0.071869       0.071869            1381.073        993.352    72.408      5.61
NL+snappy     Rand Small 1 Int   3355443       67108860       13444000    0.2003       0.114653       0.051285       0.114653             558.204        446.378    29.266      4.01
```