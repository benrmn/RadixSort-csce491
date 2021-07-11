#include <iostream>
#include <Windows.h>
#include <chrono>
#include <stdint.h>
#include <cassert>
#include <vector>
#include <thread>
#include <winnt.h>

#include <mutex>
std::mutex m;

using namespace std;
using namespace std::chrono;

void create_arr(uint32_t* list, unsigned size)
{
	for (int i = 0; i < size; i++)
	{
		list[i] = rand() | (rand() << 16);
	}
}

void lsb_print(uint32_t* list, unsigned size)
{
	for (int i = 0; i < size - 1; i++)
	{
		assert(list[i] <= list[i + 1]);
	}
}

void lsb_sort(unsigned size, int id, int N, unsigned** cnt, uint32_t** lists, HANDLE done, ULONG& counter) {
	SetThreadAffinityMask(GetCurrentThread(), 1 << (id * 2));

	int start = floor((double)id / N * size);
	int end = floor((double)(id + 1) / N * size) - 1;

	/*unsigned long long int sum = 0;
	for (int i = start; i < end; i++)
		sum += list[i];
	cout << "sum for thread " << id << ": " << sum << endl;*/
	/*
	m.lock();
	cout << "thread: " << id << " start idx: " << start << endl;
	cout << "thread: " << id << " end idx: " << end << endl;
	m.unlock();
	*/

	cnt[id] = new unsigned[256 * 4];

	unsigned idx[4][256];

	memset(cnt[id], 0, 256 * 4 * sizeof(int));

	unsigned int* a = cnt[id];
	unsigned int* b = a + 256;
	unsigned int* c = b + 256;
	unsigned int* d = c + 256;

	UCHAR* p = (UCHAR*)(lists[0] + start);
	UCHAR* endP = (UCHAR*)(lists[0] + end);

	//counting loop
	for (; p <= endP; p += sizeof(int))
	{
		a[p[0]]++; b[p[1]]++; c[p[2]]++; d[p[3]]++;
	}

	if (InterlockedIncrement(&counter) == (ULONG)N)
	{
		SetEvent(done);
	}

	WaitForSingleObject(done, INFINITE);

	// v1 
	/*idx[0][0] = 0;
	idx[1][0] = 0;
	idx[2][0] = 0;
	idx[3][0] = 0;

	for (int digit = 0; digit < 4; digit++)
	{
		for (int k = 1; k < 256; k++)
		{
			idx[digit][k] = idx[digit][k - 1] + cnt[id][digit*256 + k - 1];
		}
	}*/

	// v2
	for (int digit = 0; digit < 4; digit++)
	{
		int sum = 0;
		for (int i = 0; i < 256; i++)
		{
			if (i == 0)
			{
				sum = 0;
			}
			else
			{
				sum = idx[digit][i - 1];
				for (int thrd = id; thrd < N; thrd++)
					sum += cnt[thrd][digit * 256 + i - 1];
			}
			for (int thrd = 0; thrd < id; thrd++)
				sum += cnt[thrd][digit * 256 + i];

			idx[digit][i] = sum;
		}
	}
	
	//distribution loop
	for (int digit = 0; digit < 4; digit++)
	{
		int swap = digit & 1;
		uint32_t* input = lists[swap];
		uint32_t* output = lists[swap ^ 1];

		for (int i = start; i <= end; i++)
		{
			UCHAR* p = (UCHAR*)(input + i);
			output[idx[digit][*(p + digit)]++] = input[i];
		}
	}
}

int main() {
	int sz = 1 << 27;
	const int N = 3;

	uint32_t* data = new uint32_t[sz];
	create_arr(data, sz);

	thread threads[N];
	unsigned** cnt = new unsigned* [N];
	uint32_t* list2 = new uint32_t[sz];
	uint32_t* lists[2] = { data, list2 };
	HANDLE done = CreateEvent(NULL, TRUE, FALSE, (LPTSTR)("eventName"));
	ULONG counter = 0;

	auto start = high_resolution_clock::now();

	for (int i = 0; i < N; i++)
	{
		threads[i] = thread(lsb_sort, sz, i, N, cnt, lists, done, ref(counter));
	}

	for (int i = 0; i < N; i++)
	{
		threads[i].join();
	}

	auto stop = high_resolution_clock::now();
	duration<double> duration = (stop - start);

	// testing counting loop
	/*int sum = 0;
	unsigned total[256] = { 0 };
	for (int i = 0; i < N; i++)
	{
		for (int j = 0; j < 256; j++)
		{
			cout << j << endl;
			total[j] += cnt[i][j];
		}
		for (int k = 256; k < 512; k++)
		{
			cout << k - 256 << endl;
			total[k - 256] += cnt[i][k];
		}
		for (int x = 512; x < 768; x++)
		{
			cout << x - 512 << endl;
			total[x - 512] += cnt[i][x];
		}
		for (int z = 768; z < 1024; z++)
		{
			cout << z - 768 << endl;
			total[z - 768] += cnt[i][z];
		}
	}
	for (int i = 0; i < 256; i++)
	{
		sum += total[i];
		cout << "bucket: " << i << " count: " << total[i] << endl;
	}
	cout << "total count: " << sum << endl;*/

	for (int i = 0; i < N; i++) 
	{
		delete[] cnt[i];
	}
	delete[] cnt;
	delete[] list2;

	cout << "total items: " << sz << endl;
	cout << "seconds: " << duration.count() << endl;
	cout << "M/s: " << (double)sz / duration.count() / 1e6 << endl;
	cout << "first item: " << data[0] << endl;
	cout << "last item: " << data[sz - 1] << endl;

	// average
	/*double sum = 0.0;
	create_arr(data, sz);
	for (int i = 0; i < 20; i++)
	{
		auto start = high_resolution_clock::now();
		thread threads[N];
		unsigned** cnt = new unsigned* [N];
		HANDLE process = GetCurrentProcess();
	
		for (int i = 0; i < N; i++)
		{
			DWORD_PTR p_affinity_mask = 1 << (i * 2);
			SetThreadAffinityMask(process, p_affinity_mask);
			threads[i] = thread(lsb_sort, data, sz, i, N, cnt);
		}

		for (int i = 0; i < N; i++)
		{
			threads[i].join();
		}

		for (int i = 0; i < N; i++)
		{
			delete[] cnt[i];
		}
		delete[] cnt;
		auto stop = high_resolution_clock::now();
		duration<double> duration = (stop - start);
		sum += (double)sz / duration.count() / 1e6;
	}
	cout << "Average M/s over " << 10 << ": " << sum / 20 << endl;*/

	//lsb_print(data, sz);
	delete[] data;
	return 0;
}
