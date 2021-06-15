#include <iostream>
#include <thread>
#include <Windows.h>
#include <chrono>
#include <stdint.h>
#include <cassert>

using namespace std;
using namespace std::chrono;

void create_arr(uint32_t* list, unsigned size)
{
	for (int i = 0; i < size; i++)
	{
		list[i] = rand() | (rand() << 16);
	}
}

void lsb_sort(uint32_t* list, unsigned size) {
	uint32_t* list2 = new uint32_t[size];
	uint32_t* arr[2] = { list, list2 };

	unsigned cnt[4][256];
	unsigned idx[4][256];

	memset(cnt[0], 0, sizeof(cnt[0]));
	memset(cnt[1], 0, sizeof(cnt[1]));
	memset(cnt[2], 0, sizeof(cnt[2]));
	memset(cnt[3], 0, sizeof(cnt[3]));

	for (int digit = 0; digit < 4; digit++)
	{
		uint32_t* input = arr[0];

		for (int i = 0; i < size; i++)
		{
			UCHAR* p = (UCHAR*)(input + i);
			cnt[digit][p[digit]]++;
		}
	}

	idx[0][0] = 0;
	idx[1][0] = 0;
	idx[2][0] = 0;
	idx[3][0] = 0;

	for (int digit = 0; digit < 4; digit++)
	{
		for (int k = 1; k < 256; k++)
		{
			idx[digit][k] = idx[digit][k - 1] + cnt[digit][k - 1];
		}
	}

	for (int digit = 0; digit < 4; digit++)
	{
		int swap = digit & 1;
		uint32_t* input = arr[swap];
		uint32_t* output = arr[swap ^ 1];

		for (int i = 0; i < size; i++)
		{
			UCHAR* p = (UCHAR*)(input + i);
			output[idx[digit][p[digit]]++] = input[i];
		}
	}
	delete[] list2;
}

void lsb_print(uint32_t* list, unsigned size)
{
	for (int i = 0; i < size - 1; i++)
	{
		assert(list[i] <= list[i + 1]);
	}
}

int main() {
	int n = 1 << 27;
	uint32_t* data = new uint32_t[n];
	create_arr(data, n);

	auto start = high_resolution_clock::now();
	lsb_sort(data, n);
	auto stop = high_resolution_clock::now();
	duration<double> duration = (stop - start);

	cout << "total items: " << n << endl;
	cout << "seconds: " << duration.count() << endl;
	cout << "M/s: " << (double)n / duration.count() / 1e6 << endl;
	cout << "first item: " << data[0] << endl;
	cout << "last item: " << data[n - 1] << endl;

	lsb_print(data, n);
	delete[] data;
	return 0;
}
