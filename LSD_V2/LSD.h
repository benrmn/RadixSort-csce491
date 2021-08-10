#pragma once

#define CACHE_LINE_BITS 6
#define CACHE_LINE (1 << CACHE_LINE_BITS)

class LSD {
public:
	uint32_t* list;
	uint32_t* buckets0[256];
	uint32_t* pNext0[256];
	uint32_t* buckets1[256];
	uint32_t* pNext1[256];
	vector<VortexS*> streams;
	StreamPool* sp;
	short* tmpBucketSize;
	uint32_t* tmpBuckets;
	uint64_t sz;
	int cnt[256];
	int idx[256];

	void Reset();

	LSD(uint64_t sz);

	~LSD();

	void Create();

	void WC(uint32_t* buf, uint64_t size, int digit, uint32_t** pNext);

	void Run();

	void Test();

	void Print();

	void Scalar(uint32_t* buf, int size);

	void Offload(uint32_t** pNext);

	void MakeIdx();

	void __forceinline Copy(uint32_t* dst, uint32_t* src, uint64_t size);
};
