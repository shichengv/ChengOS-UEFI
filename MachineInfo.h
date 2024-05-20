#include <Uefi.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiLib.h>

/*  ACPI Table  */
#define APIC_TABLE_MASK                 0x1
#define APIC_SIGNATURE                  0x43495041

#define NOTHING_WAS_FOUND               0x0
#define ALL_HAVE_BEEN_FOUND             ( NOTHING_WAS_FOUND | APIC_TABLE_MASK )


/*  PFNDatabaseSize = PFN_ITEM_SIZE * NumberOfPages   */
#define PFN_ITEM_SIZE       0x20


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


typedef struct _HARDWARE_INFORMATION 
{
    UINTN RamSize;
    UINTN HighestPhysicalAddress;
    UINTN EfiMemDescCount;
    EFI_MEMORY_DESCRIPTOR EfiMemoryDesc[];

} HARDWARE_INFORMATION;

typedef struct _IMAGE_INFORMATION
{
    UINTN KernelImageStartAddress;
    UINTN AllocatedMemory;  // KernelImageSize + PFN database size;
    UINTN KernelImageSize;

} IMAGE_INFORMATION;

typedef struct _MACHINE_INFORMATION
{
    IMAGE_INFORMATION ImageInformation;
    MACHINE_GRAPHICS_OUTPUT_INFORMATION GraphicsOutputInformation;
    ACPI_INFORMATION AcpiInformation;
    HARDWARE_INFORMATION HardwareInformation;

} MACHINE_INFORMATION;