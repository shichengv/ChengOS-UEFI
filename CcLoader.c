/** @file
 * CcLoader
 **/

#include <Uefi.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/AcpiSystemDescriptionTable.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/AcpiTable.h>
#include <Guid/FileInfo.h>
#include <Guid/Acpi.h>

#include "MachineInfo.h"

EFI_GRAPHICS_OUTPUT_PROTOCOL *gGraphicsOutput;
EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *gSimpleFileSystem;
EFI_ACPI_SDT_PROTOCOL *gAcpiSdt;
EFI_SYSTEM_TABLE *gSystemTable;

// Function to convert memory type to string
CONST CHAR16 *
MemoryTypeToStr(
    IN EFI_MEMORY_TYPE MemoryType)
{
  switch (MemoryType)
  {
  case EfiReservedMemoryType:
    return L"Reserved";
  case EfiLoaderCode:
    return L"Loader Code";
  case EfiLoaderData:
    return L"Loader Data";
  case EfiBootServicesCode:
    return L"Boot Services Code";
  case EfiBootServicesData:
    return L"Boot Services Data";
  case EfiRuntimeServicesCode:
    return L"Runtime Services Code";
  case EfiRuntimeServicesData:
    return L"Runtime Services Data";
  case EfiConventionalMemory:
    return L"Conventional Memory";
  case EfiUnusableMemory:
    return L"Unusable Memory";
  case EfiACPIReclaimMemory:
    return L"ACPI Reclaim Memory";
  case EfiACPIMemoryNVS:
    return L"ACPI Memory NVS";
  case EfiMemoryMappedIO:
    return L"Memory Mapped IO";
  case EfiMemoryMappedIOPortSpace:
    return L"Memory Mapped IO Port Space";
  case EfiPalCode:
    return L"Pal Code";
  case EfiPersistentMemory:
    return L"Persistent Memory";
  default:
    return L"Unknown Type";
  }
}

STATIC UINT32 CompareGuid(CONST UINT64 *dst, CONST UINT64 *src)
{
  if (dst[0] == src[0] && dst[1] == src[1])
  {
    return 1;
  }
  return 0;
}

STATIC struct XSDP_t *ExamineEfiConfigurationTable()
{
  struct XSDP_t *XSDP = NULL;
  for (UINTN i = 0; i < gSystemTable->NumberOfTableEntries; i++)
  {
    EFI_CONFIGURATION_TABLE *ConfigurationTable = &gSystemTable->ConfigurationTable[i];
    // Check if the current entry is the extended system description pointer
    if (CompareGuid((UINT64 *)&ConfigurationTable->VendorGuid, (UINT64 *)&gEfiAcpiTableGuid))
    {
      // Found the extended system description pointer
      XSDP = ConfigurationTable->VendorTable;
      break;
    }
  }
  return XSDP;
}

EFI_STATUS
EFIAPI
GetEfiMemMap(
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable,
    IN MACHINE_INFORMATION *MachineInfo)
{
  EFI_STATUS Status;
  UINTN MemoryMapSize = 0;
  EFI_MEMORY_DESCRIPTOR *MemoryMap = NULL;
  UINTN MapKey;
  UINTN DescriptorSize;
  UINT32 DescriptorVersion;
  UINTN Index = 0;
  UINTN PageCount = 0;
  EFI_PHYSICAL_ADDRESS HighestAddress = 0;
  EFI_MEMORY_DESCRIPTOR *Descriptor;

  Status = gBS->GetMemoryMap(
      &MemoryMapSize,
      MemoryMap,
      &MapKey,
      &DescriptorSize,
      &DescriptorVersion);

  MemoryMap = MachineInfo->MemoryInformation.EfiMemoryDesc;

  Status = gBS->GetMemoryMap(
      &MemoryMapSize,
      MemoryMap,
      &MapKey,
      &DescriptorSize,
      &DescriptorVersion);
  if (EFI_ERROR(Status))
  {
    return Status;
  }
    // Now we can print the memory map
  Print(L"Type       Physical Start    Number of Pages    Attribute\n");
  for (Index = 0; Index < MemoryMapSize / DescriptorSize; Index++) {
    Descriptor = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)MemoryMap + (Index * DescriptorSize));
    Print(L"%-10s %-16lx %-16lx %-16lx\n",
      MemoryTypeToStr(Descriptor->Type),
      Descriptor->PhysicalStart,
      Descriptor->NumberOfPages,
      Descriptor->Attribute);
  }
  MachineInfo->MemoryInformation.EfiMemDescCount = Index;

  // Calculate total RAM size and find the highest physical address
  for(UINTN i = 0; i < MemoryMapSize / DescriptorSize; i++)
  {
    Descriptor = (EFI_MEMORY_DESCRIPTOR*)((UINT8 *)MemoryMap + i * DescriptorSize);
    switch (Descriptor->Type)
    {
    case EfiConventionalMemory:
      PageCount += Descriptor->NumberOfPages;
      break;
    case EfiLoaderCode:
    case EfiLoaderData:
    case EfiBootServicesCode:
    case EfiBootServicesData:
    case EfiRuntimeServicesCode:
    case EfiRuntimeServicesData:
    case EfiACPIReclaimMemory:
    case EfiACPIMemoryNVS:
    case EfiMemoryMappedIO:
    case EfiMemoryMappedIOPortSpace:
    case EfiPalCode:
    case EfiPersistentMemory:
      EFI_PHYSICAL_ADDRESS EndAddress = Descriptor->PhysicalStart + Descriptor->NumberOfPages * EFI_PAGE_SIZE;
      if (EndAddress > HighestAddress)
      {
        HighestAddress = EndAddress;
      }
      break;
    
    default:
      break;
    }

  }

  MachineInfo->MemoryInformation.RamSize = PageCount * EFI_PAGE_SIZE;
  // Subtract page size to get the last addressable byte
  MachineInfo->MemoryInformation.HighestPhysicalAddress = HighestAddress - EFI_PAGE_SIZE;
  Print(L"Total RAM: %ld Bytes\nHighest Physical Address: %-16lx\n", 
    MachineInfo->MemoryInformation.RamSize,
    MachineInfo->MemoryInformation.HighestPhysicalAddress);


  return EFI_SUCCESS;

}


EFI_STATUS
EFIAPI
FindAcpiTable(
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable,
    IN MACHINE_INFORMATION *MachineInfo)
{
  UINTN MaskSet = NOTHING_WAS_FOUND;
  MachineInfo->AcpiInformation.XSDP = ExamineEfiConfigurationTable();
  VOID* AcpiSystemDescTablePtr = 0;
  UINT32 AcpiVersion;
  UINT64 AcpiTableKey;

  /*!!! Warning:  No Error Check  */
  gBS->LocateProtocol(&gEfiAcpiSdtProtocolGuid, NULL, (VOID**)&gAcpiSdt);

  /*  Acpi Configuration  */
  for (UINT32 i = 0; MaskSet != ALL_HAVE_BEEN_FOUND; i++)
  {
    gAcpiSdt->GetAcpiTable(i, (EFI_ACPI_SDT_HEADER**)&AcpiSystemDescTablePtr,
      &AcpiVersion, &AcpiTableKey);

    // Find APIC Table
    if (*(UINT32*)AcpiSystemDescTablePtr == APIC_SIGNATURE)
    {
      Print(L"APIC Table found!\n");
      MachineInfo->AcpiInformation.APIC.DescriptorTablePtr = AcpiSystemDescTablePtr;
      MachineInfo->AcpiInformation.APIC.Version = AcpiVersion;
      MachineInfo->AcpiInformation.APIC.TableKey = AcpiTableKey;
      MaskSet |= APIC_TABLE_MASK;
      continue;
    }
    
  }

  /*!!! Warning:  No Error Check  */
  gBS->CloseProtocol(gAcpiSdt, &gEfiAcpiSdtProtocolGuid, ImageHandle, NULL);

  return EFI_SUCCESS;
  
}



EFI_STATUS
EFIAPI
AdjustGraphicsMode(
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable,
    IN MACHINE_INFORMATION *MachineInfo)
{

  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *GrahpicsOutputModeInformation;
  UINTN InfoSize = 0;
  UINT32 i = 0;
  // Default Screen size  1600x900
  UINT32 DefaultMode = 0;


  /* ========================= Adjust the Graphics mode ====================== */
  /*!!! Warning:  No Error Check  */
  gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&gGraphicsOutput);
  // Max Screen
  DefaultMode = gGraphicsOutput->Mode->MaxMode - 1;

  /*  List Graphics Modes*/
  for (i = 0; i < gGraphicsOutput->Mode->MaxMode; i++)
  {
    gGraphicsOutput->QueryMode(gGraphicsOutput, i, &InfoSize, &GrahpicsOutputModeInformation);
    Print(L"Mode:%02d, Version:%x, Format:%d, Horizontal:%d, Vertical:%d, ScanLine:%d\n", i,
          GrahpicsOutputModeInformation->Version, GrahpicsOutputModeInformation->PixelFormat,
          GrahpicsOutputModeInformation->HorizontalResolution, GrahpicsOutputModeInformation->VerticalResolution,
          GrahpicsOutputModeInformation->PixelsPerScanLine);
    if (GrahpicsOutputModeInformation->HorizontalResolution == DEFAULT_HORIZONTAL_RESOLUTION
        && GrahpicsOutputModeInformation->VerticalResolution == DEFAULT_VERTICAL_RESOLUTION)
    {
      DefaultMode = i;
    }
    
  }
  


  gGraphicsOutput->SetMode(gGraphicsOutput, DefaultMode);
  gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&gGraphicsOutput);
  Print(L"Default Mode:%02d, Version:%x, Format:%d, Horizontal:%d, Vertical:%d, ScanLine:%d, FrameBufferBase:%018lx, FrameBufferSize:%018lx\n",
        gGraphicsOutput->Mode->Mode, gGraphicsOutput->Mode->Info->Version, gGraphicsOutput->Mode->Info->PixelFormat,
        gGraphicsOutput->Mode->Info->HorizontalResolution, gGraphicsOutput->Mode->Info->VerticalResolution,
        gGraphicsOutput->Mode->Info->PixelsPerScanLine,
        gGraphicsOutput->Mode->FrameBufferBase, gGraphicsOutput->Mode->FrameBufferSize);

  MachineInfo->GraphicsOutputInformation.HorizontalResolution = gGraphicsOutput->Mode->Info->HorizontalResolution;
  MachineInfo->GraphicsOutputInformation.VerticalResolution = gGraphicsOutput->Mode->Info->VerticalResolution;
  MachineInfo->GraphicsOutputInformation.PixelsPerScanLine = gGraphicsOutput->Mode->Info->PixelsPerScanLine;
  MachineInfo->GraphicsOutputInformation.FrameBufferBase = gGraphicsOutput->Mode->FrameBufferBase;
  MachineInfo->GraphicsOutputInformation.FrameBufferSize = gGraphicsOutput->Mode->FrameBufferSize;

  /*!!! Warning:  No Error Check  */
  gBS->CloseProtocol(gGraphicsOutput, &gEfiGraphicsOutputProtocolGuid, ImageHandle, NULL);

  return EFI_SUCCESS;

}


/**
  as the real entry point for the application.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
UefiMain(
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable)
{

  // Status
  EFI_STATUS Status;

  // Allocate Memory for Machine Information Structure
  EFI_PHYSICAL_ADDRESS MachineInformationAddress = MACHINE_INFO_STRUCT_ADDR;
  MACHINE_INFORMATION *MachineInfo = (MACHINE_INFORMATION *)MachineInformationAddress;

  EFI_FILE_PROTOCOL *RootProtocol;

  EFI_FILE_PROTOCOL *KrnlProtocol;
  EFI_FILE_INFO *fiKrnl;
  UINTN KrnlBufferSize = 0;
  EFI_PHYSICAL_ADDRESS KrnlImageBase = KRNL_BASE_ADDR;
  UINTN KernelSpaceSize = 0;

  EFI_FILE_PROTOCOL *CcldrProtocol;
  EFI_FILE_INFO *fiCcldr;
  UINTN CcldrBufferSize = 0;  // The size of ccldr must not exceed 1GByte.
  EFI_PHYSICAL_ADDRESS CcldrBase = CCLDR_BASE_ADDR;

  UINTN MemMapSize = 0;
  EFI_MEMORY_DESCRIPTOR *MemMap = 0;
  UINTN MapKey = 0;
  UINTN DescriptorSize = 0;
  UINT32 DesVersion = 0;

  gSystemTable = SystemTable;
  SystemTable->ConOut->ClearScreen(SystemTable->ConOut);

  Status = gBS->AllocatePages(AllocateAddress, EfiLoaderData, (MACHINE_INFO_STRUCT_SIZE / EFI_PAGE_SIZE), &MachineInformationAddress);
  if(EFI_ERROR(Status))
  {
    Print(L"Allocate 7 pages memory for MachineInfo Structure at 0x1000\n\r");
    Print(L"UefiMain() -> AllocatePages() failed with error code: %d \n\r", Status);
    goto ExitUefi;
  }
  Status = gBS->AllocatePages(AllocateAddress, EfiLoaderData, (CCLDR_SIZE / EFI_PAGE_SIZE), &CcldrBase);
  if(EFI_ERROR(Status))
  {
    Print(L"Allocate 7 MBytes memory for Ccldr at 0x100000\n\r");
    Print(L"UefiMain() -> AllocatePages() failed with error code: %d \n\r", Status);
    goto ExitUefi;
  }
  gBS->SetMem((VOID *)MachineInfo, MACHINE_INFO_STRUCT_SIZE, 0);

  /* ========================= Adjust the Graphics mode ====================== */
  /*!!! Warning:  No Error Check  */
  AdjustGraphicsMode(ImageHandle, SystemTable, MachineInfo);

  /* ========================= Print Memory Map ====================== */
  Status = GetEfiMemMap(ImageHandle, SystemTable, MachineInfo);
  if (EFI_ERROR(Status))
  {
    goto ExitUefi;
  }

  /* ========================= Find Acpi Configuration ====================== */
  /*!!! Warning:  No Error Check  */
  FindAcpiTable(ImageHandle, SystemTable, MachineInfo);

  /* ==================== Load krnl and ccldr ==================== */
  gBS->LocateProtocol(&gEfiSimpleFileSystemProtocolGuid, NULL, (VOID **)&gSimpleFileSystem);

  gSimpleFileSystem->OpenVolume(gSimpleFileSystem, &RootProtocol);
  RootProtocol->Open(RootProtocol, &KrnlProtocol, L"krnl", EFI_FILE_MODE_READ, 0);
  RootProtocol->Open(RootProtocol, &CcldrProtocol, L"ccldr", EFI_FILE_MODE_READ, 0);

  // restricts the filename must be less than 255 characters;
  KrnlBufferSize = sizeof(EFI_FILE_INFO) + sizeof(UINT16) * 0x100;
  CcldrBufferSize = sizeof(EFI_FILE_INFO) + sizeof(UINT16) * 0x100;

  // Allocate Memory for File Info
  Status = gBS->AllocatePool(EfiRuntimeServicesData, KrnlBufferSize, (VOID **)&fiKrnl);
  if (EFI_ERROR(Status))
  {
    Print(L"Allocate memory for Kernel File Info.\n\r");
    Print(L"UefiMain() -> AllocatePool() failed with error code: %d\n\r", Status);
    goto ExitUefi;
  }
  Status = gBS->AllocatePool(EfiRuntimeServicesData, CcldrBufferSize, (VOID **)&fiCcldr);
  if (EFI_ERROR(Status))
  {
    Print(L"Allocate memory for Ccldr File Info.\n\r");
    Print(L"UefiMain() -> AllocatePool() failed with error code: %d\n\r", Status);
    goto ExitUefi;
  }
  

  /*  Allocate Kernel Space Address. */

  // Read krnl image at address 0x1000000
  KrnlProtocol->GetInfo(KrnlProtocol, &gEfiFileInfoGuid, &KrnlBufferSize, fiKrnl);
  // Calculate the memory size for Kernel Space Note that the RamSize is 512 MBytes aligned.
  // Kernel Space Size = RamSize was aligned / 4;
  KernelSpaceSize = (((MachineInfo->MemoryInformation.RamSize + 0x20000000 -1) & (~0x1fffffff)) >> 2);

  Status = gBS->AllocatePages(AllocateAddress, EfiLoaderData, (KernelSpaceSize / EFI_PAGE_SIZE), &KrnlImageBase);
  if(EFI_ERROR(Status))
  {
    Print(L"Allocate Kernel Space Address at 0x8000000.\n\r");
    Print(L"UefiMain() -> AllocatePages() failed with error code: %d \n\r", Status);
    goto ExitUefi;
  }
  KrnlBufferSize = fiKrnl->FileSize;
  KrnlProtocol->Read(KrnlProtocol, &KrnlBufferSize, (VOID *)KrnlImageBase);

  // Read ccldr image at address 0x8000
  CcldrProtocol->GetInfo(CcldrProtocol, &gEfiFileInfoGuid, &CcldrBufferSize, fiCcldr);
  CcldrBufferSize = fiCcldr->FileSize;  
  CcldrProtocol->Read(CcldrProtocol, &CcldrBufferSize, (VOID *)CcldrBase);

  // fill MachineInfo
  MachineInfo->ImageInformation[0].BaseAddress = KrnlImageBase;
  MachineInfo->ImageInformation[0].Size = KernelSpaceSize;

  MachineInfo->ImageInformation[1].BaseAddress = CcldrBase;
  MachineInfo->ImageInformation[1].Size = CCLDR_SIZE;

  MachineInfo->ImageInformation[2].BaseAddress = KrnlImageBase;
  MachineInfo->ImageInformation[2].Size = KrnlBufferSize;

  // Free Memory and Close Protocol
  gBS->FreePool(fiKrnl);
  gBS->FreePool(fiCcldr);
  KrnlProtocol->Close(KrnlProtocol);
  CcldrProtocol->Close(CcldrProtocol);
  RootProtocol->Close(RootProtocol);

  gBS->CloseProtocol(gSimpleFileSystem, &gEfiSimpleFileSystemProtocolGuid, ImageHandle, NULL);


  /* ========================== Exit Services ================================ */
  gBS->GetMemoryMap(&MemMapSize, MemMap, &MapKey, &DescriptorSize, &DesVersion);
  gBS->ExitBootServices(ImageHandle, MapKey);

  void (*ccldr_start)(MACHINE_INFORMATION *MachineInfo) = (void *)(CcldrBase);
  ccldr_start(MachineInfo);

  return EFI_SUCCESS;

  // Exit with error
ExitUefi:
  while (1)
  {
    /* code */
  }
  return Status;

}