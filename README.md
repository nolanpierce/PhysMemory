# Reading-Phys-Memory
This is a small header library I made for reading x64 kernel memory!

# Info
It is still attatching to process so you can be interupted but for anticheats unlike eac you should be fine!

# Code Example
Here is an example of using the functions you can use it for really any usecase that requires reading and writing physcial memory.

``cpp

      // Translate PEB address from virtual to physical
    u64 pebVirtual = reinterpret_cast<u64>(phys::GetProcessPeb(targetPid)); //u would need to get the pid from usermode and pass it through if you are doinbg usermode instead of kernel only
    u64 pebPhysical = phys::TranslateVirtual(targetPid, pebVirtual);
      //check if we have the physicall address
    if (pebPhysical)
    {
        // Read some data from the translated physical address
        UCHAR buffer[16];
        NTSTATUS readStatus = phys::ReadMemory(pebPhysical, buffer, sizeof(buffer));

        if (NT_SUCCESS(readStatus))
        {
            return STATUS_UNSUCCESSFUL
        }
        else{
            return STATUS_SUCCESSFUL
        }
        
    else {
      return STATUS_UNSUCCESSFUL
    }
  
```
