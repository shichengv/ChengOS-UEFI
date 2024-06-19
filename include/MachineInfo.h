#include <Uefi.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiLib.h>

#include "types.h"
#include "AcpiSDT.h"

#define MACHINE_INFO_STRUCT_ADDR                            0x1000
#define MACHINE_INFO_STRUCT_SIZE                            0xF000

#define CCLDR_BASE_ADDR                                     0x100000
#define CCLDR_SIZE                                          0x700000

#define KRNL_BASE_ADDR                                      0x8000000

#define DEFAULT_HORIZONTAL_RESOLUTION                       1920
#define DEFAULT_VERTICAL_RESOLUTION                         1080

#define MAX_FONT_NAME                                       0x18

// #define _DEBUG                                              0x10100101

typedef struct _FONT_DESCRIPTOR
{
    char FontName[MAX_FONT_NAME];
    uint64_t FntAddr;
    uint32_t FntSize;
    uint64_t PngAddr;
    uint32_t PngSize;
}FONT_DESCRIPTOR;


typedef struct _MACHINE_GRAPHICS_OUTPUT_INFORMATION
{
    UINT32 HorizontalResolution;
    UINT32 VerticalResolution;
    UINT32 PixelsPerScanLine;
    UINT32 Reserved; // for memory aligned 

    UINTN FrameBufferBase;
    UINTN FrameBufferSize;
}MACHINE_GRAPHICS_OUTPUT_INFORMATION;

typedef struct _ACPI_TABLE
{
    VOID* DescriptorTablePtr;

    UINT32 Version;
    UINT32 Reserved;   // for memory aligned

    UINT64 TableKey;

} ACPI_TABLE;

typedef struct _ACPI_INFORMATION
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
    struct _ACPI_TABLE APIC;

} ACPI_INFORMATION;


typedef struct _MEMORY_INFORMATION
{
    UINTN RamSize;
    UINTN HighestPhysicalAddress;
    UINTN EfiMemDescCount;
    EFI_MEMORY_DESCRIPTOR EfiMemoryDesc[];

} MEMORY_INFORMATION;

typedef struct _TSL_MEMORY_SPACE_INFORMATION
{
    UINTN BaseAddress;
    UINTN Size;

} TSL_MEMORY_SPACE_INFORMATION;

typedef struct _MACHINE_INFORMATION
{
    // Memory Informations
    // The first item recorded Kernel Space
    // second item: ccldr image space
    // third item: krnl image space
    TSL_MEMORY_SPACE_INFORMATION MemorySpaceInformation[3]; 
    UINTN SizeofSumofImageFiles;
    UINTN RandomNumber;
    FONT_DESCRIPTOR Font;
    // EFI_TIME CurrentTime;
    MACHINE_GRAPHICS_OUTPUT_INFORMATION GraphicsOutputInformation;
    ACPI_INFORMATION AcpiInformation;
    MEMORY_INFORMATION MemoryInformation;

} MACHINE_INFORMATION;
