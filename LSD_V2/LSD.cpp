#include "stdafx.h"
#include "LSD.h"

void LSD::Reset() {
	memset(this->cnt, 0, 256 * sizeof(int));

	// reset buckets to beginning so that another iteration can be started 
	memcpy(pNext0, buckets0, sizeof(uint32_t*) * 256);
	memcpy(pNext1, buckets1, sizeof(uint32_t*) * 256);
	memset(tmpBucketSize, 0, sizeof(tmpBucketSize[0]) * 256);

	// perform StreamPool/VortexS resets 
	for (uint64_t i = 0; i < 512; i++)
		streams[i]->Reset();
	sp->Reset();
}


LSD::LSD(uint64_t sz)
{
	this->sz = sz;
	this->list = new uint32_t[sz]; 

	memset(this->cnt, 0, 256 * sizeof(int));

	int chunkSize = 1 << 27;
	int blockSizePower = 20;
	sp = new StreamPool(blockSizePower);
	for (uint64_t i = 0; i < 256; i++) {
		VortexS* s1 = new VortexS(sz, chunkSize, sp, i);
		VortexS* s2 = new VortexS(sz, chunkSize, sp, i);
		buckets0[i] = (uint32_t*)s1->GetReadBuf();
		buckets1[i] = (uint32_t*)s2->GetReadBuf();
		streams.push_back(s1);
		streams.push_back(s2);
	}

	int pagesNeededNew = (sz * sizeof(uint32_t) + 256*sp->blockSize) / sp->pageSize;
	sp->AdjustPoolPhysicalMemory(pagesNeededNew);

	tmpBucketSize = (short*)_aligned_malloc(sizeof(short) * 256, 64);
	tmpBuckets = (uint32_t*)_aligned_malloc(sizeof(uint32_t) * 256 * CACHE_LINE, 64);
	memcpy(pNext0, buckets0, sizeof(uint32_t*) * 256);
	memcpy(pNext1, buckets1, sizeof(uint32_t*) * 256);
	memset(tmpBucketSize, 0, sizeof(tmpBucketSize[0]) * 256);
}

LSD::~LSD()
{
	for (int i = 0; i < 512; i++) delete streams[i];
	_aligned_free(tmpBucketSize);
	_aligned_free(tmpBuckets);
	delete sp;
}

void LSD::Create()
{
	std::mt19937 generator(1);
	std::uniform_int_distribution<int> dis(1, 2147483647);
	for (int i = 0; i < sz; i++)
	{
		this->list[i] = dis(generator);
	}
}

void LSD::WC(uint32_t* buf, uint64_t size, int digit, uint32_t** pNext) {
	short* localSize = tmpBucketSize;
	uint32_t* localBuckets = tmpBuckets;
	uint64_t   avx_div = sizeof(__m256i) / sizeof(uint32_t);

	// split each input item into the stream buffers
	for (uint64_t i = 0; i < size; i++) {
		// prefetch the input data
		_mm_prefetch((char*)(buf + i) + 2048, _MM_HINT_T2);

		// split the key into the write combine buffer
		UCHAR* ptr = (UCHAR*)(buf + i);
		uint32_t  buck = ptr[digit];
		short     off = localSize[buck];
		uint32_t* p     = localBuckets + (buck << CACHE_LINE_BITS);
		uint32_t* q     = p + off;
		localSize[buck] = short(off + 1);
		*q              = buf[i];
		// begin counting for last phase
		if (digit == 2) cnt[ptr[3]]++;
		
		// if this bucket's temporary buffer is full, dump the cache line
		if (off == CACHE_LINE - 1) {
			__m256i* src = (__m256i*)p, * end = src + CACHE_LINE / avx_div,
				* dest = (__m256i*) pNext[buck];
			while (src < end) {
				__m256i x = _mm256_loadu_si256(src++);
				_mm256_stream_si256(dest++, x);
				__m256i y = _mm256_loadu_si256(src++);
				_mm256_stream_si256(dest++, y);
			}
			pNext[buck] = (uint32_t*)dest;

			// bucket size reset
			localSize[buck] = 0;
		}
	}
}

void LSD::Scalar(uint32_t* buf, int size)
{
	// performs last pass of Radix Sort
	for (uint64_t i = 0; i < size; i++)
	{
		_mm_prefetch((char*)(buf + i + 128), _MM_HINT_T2);
		UCHAR* p = (UCHAR*)(buf + i);
		UCHAR byte = p[3];
		list[idx[byte]++] = buf[i];
	}
}

void LSD::Offload(uint32_t** pNext)
{
	// ensures left over keys are taken care of
	for (int j = 0; j < 256; j++)
	{
		int leftover = tmpBucketSize[j];
		uint32_t* src = tmpBuckets + (j << CACHE_LINE_BITS);
		uint32_t* tmp;
		tmp = pNext[j];
		Copy(tmp, src, leftover);
		pNext[j] += leftover;
	}
}

void LSD::MakeIdx()
{
	// last pass needs counting phase
	idx[0] = 0;
	for (int j = 1; j < 256; j++)
		idx[j] = idx[j - 1] + cnt[j - 1];
}

void LSD::Run()
{
	// digit 0
	WC(list, sz, 0, (uint32_t**)pNext0);
	Offload(pNext0);
	memset(tmpBucketSize, 0, sizeof(tmpBucketSize[0]) * 256);

	// digit 1
	for (int i = 0; i < 256; i++)
	{
		WC(buckets0[i], pNext0[i] - buckets0[i], 1, (uint32_t**)pNext1);
	}
	Offload(pNext1);
	memset(tmpBucketSize, 0, sizeof(tmpBucketSize[0]) * 256);
	memcpy(pNext0, buckets0, sizeof(uint32_t*) * 256);

	// digit 2
	for (int i = 0; i < 256; i++)
	{
		WC(buckets1[i], pNext1[i] - buckets1[i], 2, (uint32_t**)pNext0);
	}
	Offload(pNext0);
	
	// digit 3
	MakeIdx();
	for (int i = 0; i < 256; i++)
	{
		Scalar(buckets0[i], pNext0[i] - buckets0[i]);
	}
}

void LSD::Test()
{
	for (uint64_t i = 0; i < sz - 1; i++)
	{
		assert(this->list[i] <= this->list[i + 1]);
	}
}

void LSD::Print()
{
	for (uint64_t i = 0; i < sz; i++)
	{
		cout << list[i] << " ";
	}
}

void __forceinline LSD::Copy(uint32_t* dst, uint32_t* src, uint64_t size) {
	for (uint64_t i = 0; i < size; i++) dst[i] = src[i];
}
