#ifndef __INCLUDE_MACHINE_INFO_H__
#define __INCLUDE_MACHINE_INFO_H__

#include <Uefi.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiLib.h>

#include "types.h"
#include "AcpiSDT.h"

#define MACHINE_INFO_STRUCT_SIZE                            0x10000

// Byte-Alignment
#define MM1MB                                               0x100000
#define MM32MB                                              0x2000000
#define MM64MB                                              0x4000000
#define MM128MB                                             0x8000000
#define MM256MB                                             0x10000000
#define MM512MB                                             0x20000000
#define MM1GB                                               0x40000000
#define MM4GB                                               0x100000000
#define MM8GB                                               0x200000000
#define MM16GB                                              0x400000000


#define DEFAULT_HORIZONTAL_RESOLUTION                       1920
#define DEFAULT_VERTICAL_RESOLUTION                         1080

typedef struct _LOADER_IMAGE_DESCRIPTOR
{
    UINT16 Width;
    UINT16 Height;
    UINT64 Addr;
    UINT32 Size;

}LOADER_IMAGE_DESCRIPTOR;

typedef struct _LOADER_FONT_DESCRIPTOR
{
    UINTN FntAddr;
    UINTN FntSize;
    LOADER_IMAGE_DESCRIPTOR Img;

}LOADER_FONT_DESCRIPTOR;


typedef struct _LOADER_GRAPHICS_OUTPUT_INFORMATION
{
    UINT32 HorizontalResolution;
    UINT32 VerticalResolution;
    UINT32 PixelsPerScanLine;
    UINT32 Reserved; // for memory aligned 

    UINTN FrameBufferBase;
    UINTN FrameBufferSize;

}LOADER_GRAPHICS_OUTPUT_INFORMATION;

typedef struct _LOADER_ACPI_TABLE
{
    VOID* DescriptorTablePtr;

    UINT32 Version;
    UINT32 Reserved;   // for memory aligned

    UINT64 TableKey;

} LOADER_ACPI_TABLE;

typedef struct _LOADER_ACPI_INFORMATION
{
    struct _RSDP *RSDP;

    // I don't understand why the size of ACPISDTHeader was designed as 36 bytes. Due to 
    // "Compiler firewall" existence(the problem is that the size of ACPISDTHeader is not
    // equals to 36 bytes.), so that the C sources was compiled can't get correct address of 
    // Entry field. Actually, Header filed occupys 40 bytes in my compiler. 
    // For reason of the problem, we need change the type of Entry field to type uint64_t *, 
    // and we also need to set manually the value of Entry field when founding address of
    // ACPI table.
    struct {
        struct _ACPISDTHeader* Header;
        UINTN* Entry;
    } XSDT;
    struct _FADT *FADT;
    struct _DSDT *DSDT;
    struct _LOADER_ACPI_TABLE APIC;

} LOADER_ACPI_INFORMATION;


typedef struct _LOADER_MEMORY_INFORMATION
{
    UINTN RamSize;
    UINTN HighestPhysicalAddress;
    UINTN EfiMemDescCount;
    EFI_MEMORY_DESCRIPTOR EfiMemoryDesc[];

} LOADER_MEMORY_INFORMATION;

typedef struct _LOADER_MEMORY_SPACE_DESCRIPTOR
{
    UINTN BaseAddress;
    UINTN Size;

} LOADER_MEMORY_SPACE_DESCRIPTOR;

typedef struct _LOADER_MACHINE_INFORMATION
{
    // Memory Informations
    // The first item recorded Kernel Space
    // second item: ccldr image space
    // third item: krnl image space
    LOADER_MEMORY_SPACE_DESCRIPTOR MemorySpaceInformation[3]; 

    // Background Info
    LOADER_IMAGE_DESCRIPTOR Background; 

    // Font[0]: primary font
    // Font[1]: secondary font
    LOADER_FONT_DESCRIPTOR Font[2];

    // The size of the sum of all files in bytes
    UINTN SumOfSizeOfFilesInBytes;

    UINTN RandomNumber;

    LOADER_GRAPHICS_OUTPUT_INFORMATION GraphicsOutputInformation;
    LOADER_ACPI_INFORMATION AcpiInformation;
    LOADER_MEMORY_INFORMATION MemoryInformation;

} LOADER_MACHINE_INFORMATION;

// extern LOADER_MACHINE_INFORMATION* MachineInfo;

#endif