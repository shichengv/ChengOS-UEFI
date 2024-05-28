#include <Uefi.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiLib.h>

/*  ACPI Table  */
#define APIC_TABLE_MASK                 0x1
#define APIC_SIGNATURE                  0x43495041

#define NOTHING_WAS_FOUND               0x0
#define ALL_HAVE_BEEN_FOUND             ( NOTHING_WAS_FOUND | APIC_TABLE_MASK )

#define MACHINE_INFO_STRUCT_ADDR        0x1000
#define MACHINE_INFO_STRUCT_SIZE        0xE000

#define CCLDR_BASE_ADDR                 0x100000
#define CCLDR_SIZE                      0x700000

#define KRNL_BASE_ADDR                  0x8000000

#define DEFAULT_HORIZONTAL_RESOLUTION             1280
#define DEFAULT_VERTICAL_RESOLUTION               720


struct RSDP_t
{
    CHAR8 Signature[8];
    UINT8 Checksum;
    CHAR8 OEMID[6];
    UINT8 Revision;
    UINT32 RsdtAddress;
} __attribute__((packed));

struct XSDP_t {
    CHAR8 Signature[8];
    UINT8 Checksum;
    CHAR8 OEMID[6];
    UINT8 Revision;
    UINT32 RsdtAddress;      // deprecated since version 2.0

    UINT32 Length;
    UINTN XsdtAddress;
    UINT8 ExtendedChecksum;
    UINT8 reserved[3];
} __attribute__ ((packed));

typedef struct _MACHINE_GRAPHICS_OUTPUT_INFORMATION
{
    UINT32 HorizontalResolution;
    UINT32 VerticalResolution;
    UINT32 PixelsPerScanLine;

    UINTN FrameBufferBase;
    UINTN FrameBufferSize;
}MACHINE_GRAPHICS_OUTPUT_INFORMATION;

typedef struct _ACPI_TABLE
{
    VOID* DescriptorTablePtr;
    UINT32 Version;
    UINT64 TableKey;

} ACPI_TABLE;

typedef struct _ACPI_INFORMATION
{
    struct XSDP_t* XSDP;
    struct _ACPI_TABLE APIC;

} ACPI_INFORMATION;


typedef struct _MEMORY_INFORMATION
{
    UINTN RamSize;
    UINTN HighestPhysicalAddress;
    UINTN EfiMemDescCount;
    EFI_MEMORY_DESCRIPTOR EfiMemoryDesc[];

} MEMORY_INFORMATION;

typedef struct _TSL_IMAGE_INFORMATION
{
    UINTN BaseAddress;
    UINTN Size;

} TSL_IMAGE_INFORMATION;

typedef struct _MACHINE_INFORMATION
{
    /*  1st Kernel Space
        2st Ccldr Image
        3st Kernel Image    */
    TSL_IMAGE_INFORMATION ImageInformation[3]; 

    MACHINE_GRAPHICS_OUTPUT_INFORMATION GraphicsOutputInformation;
    ACPI_INFORMATION AcpiInformation;
    MEMORY_INFORMATION MemoryInformation;

} MACHINE_INFORMATION;
