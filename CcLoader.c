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

  MemoryMap = MachineInfo->HardwareInformation.EfiMemoryDesc;

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
  MachineInfo->HardwareInformation.EfiMemDescCount = Index;

  // Calculate total RAM size and find the highest physical address
  for(UINTN i = 0; i < MemoryMapSize / DescriptorSize; i++)
  {
    Descriptor = (EFI_MEMORY_DESCRIPTOR*)((UINT8 *)MemoryMap + i * DescriptorSize);
    if (Descriptor->Type == EfiConventionalMemory)
    {
      PageCount += Descriptor->NumberOfPages;
      EFI_PHYSICAL_ADDRESS EndAddress = Descriptor->PhysicalStart + Descriptor->NumberOfPages * EFI_PAGE_SIZE;
      if (EndAddress > HighestAddress)
      {
        HighestAddress = EndAddress;
      }
      
    }
  }

  MachineInfo->HardwareInformation.RamSize = PageCount * EFI_PAGE_SIZE;
  // Subtract page size to get the last addressable byte
  MachineInfo->HardwareInformation.HighestPhysicalAddress = HighestAddress - EFI_PAGE_SIZE;
  Print(L"Total RAM: %ld Bytes\nHighest Physical Address: %-16lx\n", 
    MachineInfo->HardwareInformation.RamSize,
    MachineInfo->HardwareInformation.HighestPhysicalAddress);


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
  EFI_PHYSICAL_ADDRESS MachineInformationAddress = 0x1000;
  MACHINE_INFORMATION *MachineInfo = (MACHINE_INFORMATION *)MachineInformationAddress;

  EFI_FILE_PROTOCOL *RootProtocol;

  EFI_FILE_PROTOCOL *CcldrProtocol;
  EFI_FILE_INFO *fiCcldr;
  UINTN CcldrBufferSize = 0;
  EFI_PHYSICAL_ADDRESS CcldrImageBase = 0x8000;

  EFI_FILE_PROTOCOL *KrnlProtocol;
  EFI_FILE_INFO *fiKrnl;
  UINTN KrnlBufferSize = 0;
  EFI_PHYSICAL_ADDRESS KrnlImageBase = 0x1000000;
  UINTN SumOfKrnlImageSizeAndPfnDatabaseSize = 0;

  UINTN MemMapSize = 0;
  EFI_MEMORY_DESCRIPTOR *MemMap = 0;
  UINTN MapKey = 0;
  UINTN DescriptorSize = 0;
  UINT32 DesVersion = 0;

  gBS->AllocatePages(AllocateAddress, EfiConventionalMemory, 7, &MachineInformationAddress);
  gBS->SetMem((VOID *)MachineInfo, (0x7 << 12), 0);

  gSystemTable = SystemTable;
  SystemTable->ConOut->ClearScreen(SystemTable->ConOut);


  /* ========================= Adjust the Graphics mode ====================== */
  /*!!! Warning:  No Error Check  */
  AdjustGraphicsMode(ImageHandle, SystemTable, MachineInfo);

  /* ========================= Print Memory Map ====================== */
  Status = GetEfiMemMap(ImageHandle, SystemTable, MachineInfo);
  if (EFI_ERROR(Status))
  {
    goto ExitUefi;
  }
  
  SumOfKrnlImageSizeAndPfnDatabaseSize = ((fiKrnl->FileSize + 0x1000 - 1) & (~0xfff)) + 
    ((MachineInfo->HardwareInformation.RamSize + 0x1000 - 1) & (~0xfff)) / EFI_PAGE_SIZE * PFN_ITEM_SIZE;

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
  gBS->AllocatePool(EfiRuntimeServicesData, KrnlBufferSize, (VOID **)&fiKrnl);
  gBS->AllocatePool(EfiRuntimeServicesData, CcldrBufferSize, (VOID **)&fiCcldr);

  // Read krnl image at address 0x1000000
  KrnlProtocol->GetInfo(KrnlProtocol, &gEfiFileInfoGuid, &KrnlBufferSize, fiKrnl);
  gBS->AllocatePages(AllocateAddress, EfiConventionalMemory, SumOfKrnlImageSizeAndPfnDatabaseSize, &KrnlImageBase);
  KrnlBufferSize = fiKrnl->FileSize;
  KrnlProtocol->Read(KrnlProtocol, &KrnlBufferSize, (VOID *)KrnlImageBase);

  // Read ccldr image at address 0x8000
  CcldrProtocol->GetInfo(CcldrProtocol, &gEfiFileInfoGuid, &CcldrBufferSize, fiCcldr);
  gBS->AllocatePages(AllocateAddress, EfiConventionalMemory, (fiCcldr->FileSize + 0x1000 - 1) & (~0xfff), &CcldrImageBase);
  CcldrBufferSize = fiCcldr->FileSize;
  CcldrProtocol->Read(CcldrProtocol, &CcldrBufferSize, (VOID *)CcldrImageBase);

  MachineInfo->ImageInformation.KernelImageStartAddress = KrnlImageBase;
  MachineInfo->ImageInformation.AllocatedMemory = SumOfKrnlImageSizeAndPfnDatabaseSize;
  MachineInfo->ImageInformation.KernelImageSize = KrnlBufferSize;

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

  void (*ccldr_start)(MACHINE_INFORMATION *MachineInfo) = (void *)(0x300000);
  ccldr_start(MachineInfo);

  return EFI_SUCCESS;

  // Exit with error
ExitUefi:
  return Status;

}