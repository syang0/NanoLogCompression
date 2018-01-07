CXXFLAGS= -std=c++11 -DNDEBUG -g -O3
CXX=g++

NANOLOG_DIR=./NanoLog

all: benchmark

%.o: %.cc %.h
	$(CXX) $(CXXFLAGS) -c -o $@ $< -I$(NANOLOG_DIR)/runtime/

main.o: main.cc libsnappy.a
	$(CXX) $(CXXFLAGS) -c -o $@ $< -I$(NANOLOG_DIR)/runtime/

Cycles.o: $(NANOLOG_DIR)/runtime/Cycles.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $< -I$(NANOLOG_DIR)/runtime/

benchmark: main.o Cycles.o Logger.o CommonWords.o RAMCloudLogs.o libsnappy.a
	$(CXX) $(CXXFLAGS) -o $@ $^ -lpthread -I$(NANOLOG_DIR)/runtime/ -L. -lz -lsnappy


SNAPPY_DIR=./snappy/

$(SNAPPY_DIR)/installDir/lib/libsnappy.a:
	cd $(SNAPPY_DIR) && \
	./autogen.sh && \
	CFLAGS="-O3 -DNDEBUG" CXXFLAGS="-O3 -DNDEBUG" ./configure && \
	make -j10 && \
	make install prefix=$(realpath $(SNAPPY_DIR))/installDir

libsnappy.a: $(SNAPPY_DIR)/installDir/lib/libsnappy.a
	cp $(SNAPPY_DIR)/installDir/lib/libsnappy.a .

CommonWords.cc: transform.py
	python transform.py

clean:
	rm -f *.o benchmark
