#include "physical.h"

PVOID phys::GetProcessPeb(DWORD processId) {
    PEPROCESS process;
    PVOID ProcessPEB;
    PsLookupProcessByProcessId((HANDLE)processId, &process); //Getting the Eprocess of the process
    memcpy(&ProcessPEB, (PVOID)((ULONG_PTR)process + 0x550), sizeof(ProcessPEB));

    return ProcessPEB; // Adding the offset that corresponds to the location of the PEB in Eprocess struct
}

u64 phys::GetDirectoryTable(DWORD processId)
{
    PEPROCESS process;
    PsLookupProcessByProcessId((HANDLE)processId, &process); //Getting the Eprocess of the process
    PVOID ProcessDirBase = NULL;
    PVOID UsermodeDirBase = NULL;
    //Getting the processDirbase 
    memcpy(&ProcessDirBase, (PVOID)((u64)process + 0x28), sizeof(ProcessDirBase));
    if (!ProcessDirBase)
    {
        memcpy(&ProcessDirBase, (PVOID)((u64)process + 0x388), sizeof(UsermodeDirBase));
        return (u64)UsermodeDirBase;
    }
    return (u64)ProcessDirBase;
}

u64 phys::GetPageTableBase(DWORD pid) //need to make this noattach
{
    KAPC_STATE apcState;
    PEPROCESS process;
    PsLookupProcessByProcessId((HANDLE)pid, &process);
    KeStackAttachProcess(process, &apcState);
    PHYSICAL_ADDRESS pageTableBase = {.QuadPart = static_cast<LONGLONG>((u64)process + 0x28)}; //0x28 is where the pagetable is located in eprocess
    KeUnstackDetachProcess(&apcState);
    return pageTableBase.QuadPart;
}

u64 phys::TranslateVirtual(DWORD pid, u64 virtualAddress)
{
    u64 pageDirectoryBase = phys::GetPageTableBase(pid);

    u64 virtualAddr = reinterpret_cast<u64>(virtualAddress);

    // Extract the index values from the virtual address
    u64 index1 = (virtualAddr >> 39) & 0x1FF;
    u64 index2 = (virtualAddr >> 30) & 0x1FF;
    u64 index3 = (virtualAddr >> 21) & 0x1FF;
    u64 index4 = (virtualAddr >> 12) & 0x1FF;

    // Calculate the offsets for the page table entries
    u64 pml4eOffset = index1 * sizeof(u64);
    u64 pdpteOffset = index2 * sizeof(u64);
    u64 pdeOffset = index3 * sizeof(u64);
    u64 pteOffset = index4 * sizeof(u64);

    // Read the Page Map Level 4 Entry (PML4E)
    u64 pml4e;
    if (!MmIsAddressValid(reinterpret_cast<PVOID>(pageDirectoryBase + pml4eOffset)))
        return NULL;

    memcpy(&pml4e, reinterpret_cast<PVOID>(pageDirectoryBase + pml4eOffset), sizeof(u64));

    // Check if the PML4E is present
    if (!(pml4e & 1))
        return NULL;

    // Read the Page Directory Pointer Table Entry (PDPTE)
    u64 pdpte;
    if (!MmIsAddressValid(reinterpret_cast<PVOID>((pml4e & 0xFFFFFFF0000ULL) + pdpteOffset)))
        return NULL;

    memcpy(&pdpte, reinterpret_cast<PVOID>((pml4e & 0xFFFFFFF0000ULL) + pdpteOffset), sizeof(u64));

    // Check if the PDPTE is present
    if (!(pdpte & 1))
        return NULL;

    // Read the Page Directory Entry (PDE)
    u64 pde;
    if (!MmIsAddressValid(reinterpret_cast<PVOID>((pdpte & 0xFFFFFFFFFF000ULL) + pdeOffset)))
        return NULL;

    memcpy(&pde, reinterpret_cast<PVOID>((pdpte & 0xFFFFFFFFFF000ULL) + pdeOffset), sizeof(u64));

    // Check if the PDE is present
    if (!(pde & 1))
        return NULL;

    // Read the Page Table Entry (PTE)
    u64 pte;
    if (!MmIsAddressValid(reinterpret_cast<PVOID>((pde & 0xFFFFFFFFFF000ULL) + pteOffset)))
        return NULL;

    memcpy(&pte, reinterpret_cast<PVOID>((pde & 0xFFFFFFFFFF000ULL) + pteOffset), sizeof(u64));

    // Check if the PTE is present
    if (!(pte & 1))
        return NULL;

    // Calculate the physical address using the PTE
    u64 physicalAddress = (pte & 0xFFFFFFFFFF000ULL) + (virtualAddr & 0xFFF);

    return physicalAddress;
}

u64 phys::TranslatePhysical(DWORD pid, u64 physicalAddress)
{
    // Get the page table base for the specified process
    u64 pageTableBase = phys::GetPageTableBase(pid); 

    // Calculate the index values from the physical address
    ULONG_PTR pdIndex = (physicalAddress >> 22) & 0x3FF;
    ULONG_PTR ptIndex = (physicalAddress >> 12) & 0x3FF;

    // Read the Page Directory Entry (PDE)
    PULONG_PTR pdEntry = reinterpret_cast<PULONG_PTR>(
        reinterpret_cast<ULONG_PTR*>(pageTableBase)[pdIndex]);

    // Check if the PDE is present
    if (!(pdEntry && (*pdEntry & 1)))
        return NULL;

    // Read the Page Table Entry (PTE)
    PULONG_PTR ptEntry = reinterpret_cast<PULONG_PTR>(
        reinterpret_cast<ULONG_PTR>(*pdEntry & ~0xFFF) + ptIndex * sizeof(ULONG_PTR));

    // Check if the PTE is present
    if (!(ptEntry && (*ptEntry & 1)))
        return NULL;

    // Calculate the virtual address using the PTE
    u64 virtualAddress = (*ptEntry & ~0xFFF) + (physicalAddress & 0xFFF);

    return virtualAddress;
}


NTSTATUS phys::ReadMemory(u64 src, PVOID buffer, size_t size)
{
    // Check if the source address is null
    if (src == NULL)
        return STATUS_UNSUCCESSFUL;

    // Initialize physical addresses
    PHYSICAL_ADDRESS LowAddr = { .QuadPart = static_cast<LONGLONG>(src) };
    PHYSICAL_ADDRESS HighAddr = { .QuadPart = static_cast<LONGLONG>(src + size) };
    PHYSICAL_ADDRESS SkipBytes = {};
    // Allocate memory descriptor list (MDL)
    PMDL Info = MmAllocatePagesForMdlEx(LowAddr, HighAddr, SkipBytes, size, MmNonCached, MM_ALLOCATE_FULLY_REQUIRED);
    if (Info == NULL)
        return STATUS_UNSUCCESSFUL;

    // Attempt to map physical memory
    PVOID MappedMem = MmMapLockedPagesSpecifyCache( Info, KernelMode, MmNonCached, (PVOID)src, 0, 32);
    if (MappedMem == NULL)
    {
        // Cleanup: Release allocated MDL on failure
        MmFreePagesFromMdl( Info);
        return STATUS_UNSUCCESSFUL;
    }

    // Copy memory from mapped region to user-provided buffer
    memcpy((PVOID)buffer, MappedMem, size);

    // Unmap the memory and release the MDL
    MmUnmapLockedPages( MappedMem, Info);
    MmFreePagesFromMdl( Info);

    return STATUS_SUCCESS;
}

NTSTATUS phys::WriteMemory(u64 src, PVOID buffer, size_t size)
{
    // Check if the source address is null
    if (src == NULL)
        return STATUS_UNSUCCESSFUL;

    // Initialize physical addresses
    PHYSICAL_ADDRESS LowAddr = { .QuadPart = static_cast<LONGLONG>(src)};
    PHYSICAL_ADDRESS HighAddr = { .QuadPart = static_cast<LONGLONG>(src + size)};
    PHYSICAL_ADDRESS SkipBytes = {};

    // Allocate memory descriptor list (MDL)
    PMDL Info = MmAllocatePagesForMdlEx( LowAddr, HighAddr, SkipBytes, size, MmNonCached, MM_ALLOCATE_FULLY_REQUIRED);
    if (Info == NULL)
        return STATUS_UNSUCCESSFUL;

    // Attempt to map physical memory
    PVOID MappedMem = MmMapLockedPagesSpecifyCache( Info, KernelMode, MmNonCached, (PVOID)src, 0, 32);
    if (MappedMem == NULL)
    {
        // Cleanup: Release allocated MDL on failure
        MmFreePagesFromMdl(Info);
        return STATUS_UNSUCCESSFUL;
    }

    // Copy memory from buffer to Mapped 
    memcpy( MappedMem, (PVOID)buffer, size);

    // Unmap the memory and release the MDL
    MmUnmapLockedPages( MappedMem, Info);
    MmFreePagesFromMdl( Info);

    return STATUS_SUCCESS;
}



