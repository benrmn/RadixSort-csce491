#include <iostream>
#include <Windows.h>
#include <chrono>
#include <stdint.h>
#include <cassert>
#include <vector>
#include <thread>
#include <random>
#include <winnt.h>

#include <mutex>
std::mutex m;

using namespace std;
using namespace std::chrono;

class LSD {
public:
	HANDLE doneCounting[4], doneDistributing[4];
	LONG nFinishedCounting[4], nFinishedDistributing[4];
	int** cnt;
	int N;
	thread* threads;
	uint32_t* list;
	uint32_t* list2;
	uint32_t* lists[2];

	LSD(int sz, int N)
	{
		cnt = new int* [N];

		for (int j = 0; j < N; j++)
			cnt[j] = new int[2560];		// pad with extra bytes to avoid false sharing

		for (int i = 0; i < 4; i++)
		{
			doneCounting[i] = CreateEvent(NULL, TRUE, FALSE, NULL);
			doneDistributing[i] = CreateEvent(NULL, TRUE, FALSE, NULL);
			nFinishedCounting[i] = 0;
			nFinishedDistributing[i] = 0;
		}

		this->N = N;
		this->threads = new thread[N];
		this->list = new uint32_t[sz];
		this->list2 = new uint32_t[sz];
		memset(this->list2, 0, sizeof(DWORD) * sz);
		lists[0] = this->list;
		lists[1] = this->list2;
		
	}

	~LSD()
	{
		for (int i = 0; i < N; i++)
		{
			delete[] cnt[i];
		}
		delete[] cnt;
		delete[] list;
		delete[] list2;
	}

	void Create(int size)
	{
		for (int i = 0; i < 4; i++)
		{
			ResetEvent(doneCounting[i]);
			ResetEvent(doneDistributing[i]);
			nFinishedCounting[i] = 0;
			nFinishedDistributing[i] = 0;
		}

		std::mt19937 generator(1);
		std::uniform_int_distribution<int> dis(1, 2147483647);
		for (int i = 0; i < size; i++)
		{
			this->list[i] = dis(generator);
		}
	}

	void LSDsort(int size, int threadID, int N)
	{
		SetThreadAffinityMask(GetCurrentThread(), 1 << (threadID * 2));

		int start = floor((double)threadID / N * size);
		int end = floor((double)(threadID + 1) / N * size) - 1;

		unsigned idx[256];
		int* mycnt = cnt[threadID];

		for (int digit = 0; digit < 4; digit++)
		{
			memset(mycnt, 0, 256 * sizeof(int));

			int swap = digit & 1;
			uint32_t* input = lists[swap];
			uint32_t* output = lists[swap ^ 1];

			UCHAR* p = (UCHAR*)(input + start);
			UCHAR* endP = (UCHAR*)(input + end);

			for (; p <= endP; p += sizeof(int))
			{
				_mm_prefetch((char*)(p + 512), _MM_HINT_T2);
				UCHAR byte = p[digit];
				mycnt[byte]++;
			}

			if (InterlockedIncrement(&nFinishedCounting[digit]) == (LONG)N)
			{
				SetEvent(doneCounting[digit]);
			}

			WaitForSingleObject(doneCounting[digit], INFINITE);

			int sum = 0;
			for (int i = 0; i < 256; i++)
			{
				if (i == 0)
				{
					sum = 0;
				}
				else
				{
					sum = idx[i - 1];
					for (int thrd = threadID; thrd < N; thrd++)
						sum += cnt[thrd][i - 1];
				}

				for (int thrd = 0; thrd < threadID; thrd++)
					sum += cnt[thrd][i];

				idx[i] = sum;
			}

			for (int i = start; i <= end; i++)
			{
				_mm_prefetch((char*)(input + i + 128), _MM_HINT_T2);
				UCHAR* p = (UCHAR*)(input + i);
				UCHAR byte = p[digit];
				output[idx[byte]++] = input[i];
			}

			if (InterlockedIncrement(&nFinishedDistributing[digit]) == (ULONG)N)
			{
				SetEvent(doneDistributing[digit]);
			}

			WaitForSingleObject(doneDistributing[digit], INFINITE);

		}
	}

	void Run(int sz)
	{
		for (int i = 0; i < N; i++)
			threads[i] = thread(&LSD::LSDsort, this, sz, i, N);

		for (int i = 0; i < N; i++)
			threads[i].join();
	}

	void Test(int size)
	{
		for (int i = 0; i < size - 1; i++)
		{
			assert(this->list[i] <= this->list[i + 1]);
		}
	}
};

int main(int argc, char** argv) {
	if (argc != 2) { printf("Usage: program <number of threads>\n"); exit(-1); }

	int sz = 1 << 29;
	const int N = atoi(argv[1]);

	LSD radix(sz, N);

	for (int iter = 0; iter < 10; iter++)
	{
		radix.Create(sz);
		auto start = high_resolution_clock::now();
		radix.Run(sz);
		auto stop = high_resolution_clock::now();
		duration<double> duration = (stop - start);

		//cout << "total items: " << sz << endl;
		cout << "seconds: " << duration.count() << " M/sec: " << (double)sz / duration.count() / 1e6 << endl;
		//cout << "first item: " << list[0] << endl;
		//cout << "last item: " << list[sz - 1] << endl;
	}
	return 0;
}
