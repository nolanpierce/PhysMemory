#ifndef PHYSICALMEMORY
#define PHYSICALMEMORY
#include <ntifs.h>
#include <intrin.h>
#include "DataTypes/types.h"
#include "XOR/sky_crypt.h"
#include "Logger/log.h"

namespace phys {

	inline PVOID GetProcessPeb(DWORD processId);
	inline u64 GetDirectoryTable(DWORD processId);
	
	u64 GetPageTableBase(DWORD pid);

	inline u64 TranslateVirtual(DWORD pid, PVOID virtualAddress);

	inline NTSTATUS ReadMemory(u64 src, PVOID buffer, size_t size);
	inline NTSTATUS WriteMemory(u64 src, PVOID buffer, size_t size);

	//Going to add this evenutally
	inline NTSTATUS ReadMemoryNoAttatch(u64 src, PVOID buffer, size_t size);
	inline NTSTATUS WriteMemoryNoAttatch(u64 src, PVOID buffer, size_t size);
}



#endif // !PHYSICALMEMORY
