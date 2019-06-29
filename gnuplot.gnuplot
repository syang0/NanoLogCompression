# set xtics ("100%%" 0, "50%%" 1, "10%%" 2, "1%%" 3)
set terminal svg size 576, 432 fname 'Verdana,14'

set auto x
set ylabel "Number of Test Cases Better than the Rest" font 'Verdana,18'
set xlabel "Disk Bandwidth (MB/s)" font 'Verdana,16'
set yrange [0:]
set logscale x
set mxtics 11

set key autotitle columnhead

FILES = system("ls -1 details/*.txt")
do for [ file in FILES]{
    set output file.".svg"
    plot file u 1:2 w l lc 'red', \
           '' u 1:3 w l, \
           '' u 1:4 w l, \
           '' u 1:5 w l, \
           '' u 1:6 w l, \
           '' u 1:7 w l, \
           '' u 1:8 w l, \
           '' u 1:9 w l, \
           '' u 1:10 w l, \
}

set logscale y
FILES = system("ls -1 details/*-absolute.txt")
do for [ file in FILES]{
    set output file.".svg"
    plot file u 1:2 w l lc 'red', \
           '' u 1:3 w l, \
           '' u 1:4 w l, \
           '' u 1:5 w l, \
           '' u 1:6 w l, \
           '' u 1:7 w l, \
           '' u 1:8 w l, \
           '' u 1:9 w l, \
           '' u 1:10 w l, \
}