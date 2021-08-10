#include "stdafx.h"
#include "LSD.h"

int main(int argc, char** argv) {
	if (argc != 2) { printf("Usage: program <number of threads>\n"); exit(-1); }

	int sz = 1 << 27;
	//const int N = atoi(argv[1]);

	LSD radix(sz);

	uint64_t memory = sz * sizeof(uint32_t);
	int blockSizePower = 20;
	StreamPool* sp = new StreamPool(blockSizePower);
	Stream* inputS = new VortexS(memory, memory, sp, 256);
	//list = (uint32_t*)inputS->GetReadBuf();

	for (int iter = 0; iter < 10; iter++)
	{
		radix.Create();
		auto start = high_resolution_clock::now();
		radix.Run();
		auto stop = high_resolution_clock::now();
		duration<double> duration = (stop - start);
		//radix.Print();
		//radix.Test();
		radix.Reset();
		//cout << "total items: " << sz << endl;
		cout << "seconds: " << duration.count() << " M/sec: " << (double)sz / duration.count() / 1e6 << endl;
		//cout << "first item: " << list[0] << endl;
		//cout << "last item: " << list[sz - 1] << endl;
		inputS->Reset();
	}
	return 0;
}
