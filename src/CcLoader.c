/** @file
 * CcLoader
 **/

#include "include/FrameWork.h"
#include "include/AcpiSDT.h"
#include "include/MachineInfo.h"
#include "include/Wrapper.h"
#include "include/ReadFile.h"

#define PAGE_ALIGNED(x)               (((UINTN)x + (EFI_PAGE_SIZE - 1)) & ~(EFI_PAGE_SIZE - 1))
#define KRNL_PATH                     L"krnl"
#define CCLDR_PATH                    L"ccldr"
#define BG_PATH                       L"bg\\background"
#define BG_PNG_PATH                   L"bg\\background.png"

#define PRIMARY_FONT_PATH             L"font\\Caerulaarbor\\Caerulaarbor.fnt"
#define PRIMARY_FONT_IMG_PATH         L"font\\Caerulaarbor\\Caerulaarbor_0.bgra"
#define PRIMARY_FONT_PNG_PATH         L"font\\Caerulaarbor\\Caerulaarbor_0.png"

#define SECONDARY_FONT_PATH           L"font\\CascadiaCode\\CascadiaCode.fnt"
#define SECONDARY_FONT_IMG_PATH       L"font\\CascadiaCode\\CascadiaCode_0.bgra"
#define SECONDARY_FONT_PNG_PATH       L"font\\CascadiaCode\\CascadiaCode_0.png"


// Protocols
EFI_SYSTEM_TABLE *gSystemTable;

#ifndef _DEBUG
extern VOID DisplayLoadingLogo();
#endif


extern VOID FindAcpiTable(IN LOADER_MACHINE_INFORMATION *MachineInfo);
extern VOID AdjustGraphicsMode(IN LOADER_MACHINE_INFORMATION *MachineInfo);
extern VOID GetPNGSize(
	IN CHAR16* PngPath,
	IN OUT UINT32* Width,
	IN OUT UINT32* Height
);


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




STATIC
VOID
GetEfiMemMap(
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable,
    IN LOADER_MACHINE_INFORMATION *MachineInfo)
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
    Print(L"GetMemoryMap failed with error code:%d. \n\r", Status);
    UEFI_PANIC;
  }

#ifdef _DEBUG
  // Now we can print the memory map
  Print(L"Type       Physical Start    Number of Pages    Attribute\n");
#endif
  for (Index = 0; Index < MemoryMapSize / DescriptorSize; Index++)
  {
    Descriptor = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)MemoryMap + (Index * DescriptorSize));
#ifdef _DEBUG
    Print(L"%-10s %-16lx %-16lx %-16lx\n",
          MemoryTypeToStr(Descriptor->Type),
          Descriptor->PhysicalStart,
          Descriptor->NumberOfPages,
          Descriptor->Attribute);
#endif
  }

  MachineInfo->MemoryInformation.EfiMemDescCount = Index;

  // Calculate total RAM size and find the highest physical address
  for (UINTN i = 0; i < MemoryMapSize / DescriptorSize; i++)
  {
    Descriptor = (EFI_MEMORY_DESCRIPTOR *)((UINT8 *)MemoryMap + i * DescriptorSize);
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

#ifdef _DEBUG
  Print(L"Total RAM: %ld Bytes\nHighest Physical Address: %-16lx\n",
        MachineInfo->MemoryInformation.RamSize,
        MachineInfo->MemoryInformation.HighestPhysicalAddress);

#endif
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

  UINT32 PNGWidth;
  UINT32 PNGHeight;

  // default random number equals to skadi(73 6B 61 64 69)
  UINTN RandomNumber = 0x6964616B73;
  EFI_RNG_PROTOCOL *gRngProtocol;
  UINTN SizeOfRandomNumber = sizeof(UINTN);


  // Current Machine Information
  LOADER_MACHINE_INFORMATION *MachineInfo;

  EFI_PHYSICAL_ADDRESS KrnlImageBase;
  UINTN KernelBufferSize = 0;
  UINTN KernelSpaceSize = 0;

  UINTN CcldrBufferSize = 0;
  EFI_PHYSICAL_ADDRESS CcldrBase;



  EFI_PHYSICAL_ADDRESS FileBuffer;
  UINTN FileSize;

  // ExitUefiService
  UINTN MemMapSize = 0;
  EFI_MEMORY_DESCRIPTOR *MemMap = 0;
  UINTN MapKey = 0;
  UINTN DescriptorSize = 0;
  UINT32 DesVersion = 0;


  // initialization
  gSystemTable = SystemTable;
  SystemTable->ConOut->ClearScreen(SystemTable->ConOut);

  Status = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, (CCLDR_SIZE + MACHINE_INFO_STRUCT_SIZE) / EFI_PAGE_SIZE, &CcldrBase);
  if (EFI_ERROR(Status))
  {
    Print(L"[ERROR]: Allocate memory for ccldr failed...\n\r");
    goto ExitUefi;
  }


  MachineInfo = (LOADER_MACHINE_INFORMATION *)(CcldrBase);
  gBS->SetMem((VOID*)MachineInfo, MACHINE_INFO_STRUCT_SIZE, 0);
  CcldrBase += MACHINE_INFO_STRUCT_SIZE;


  // Get Randomized Number
  Status = gBS->LocateProtocol(&gEfiRngProtocolGuid, NULL, (VOID**)&gRngProtocol);
  if (!EFI_ERROR(Status))
  {
    gRngProtocol->GetRNG(gRngProtocol, NULL, SizeOfRandomNumber, (UINT8 *)RandomNumber);
    MachineInfo->RandomNumber = RandomNumber;
  }
  else
    MachineInfo->RandomNumber = RandomNumber;

  ReadFileInit();

  /* ========================= Adjust the Graphics mode ====================== */
  AdjustGraphicsMode(MachineInfo);

#ifndef _DEBUG
  DisplayLoadingLogo();
#endif


  /* ========================= Print Memory Map ====================== */
  GetEfiMemMap(ImageHandle, SystemTable, MachineInfo);

  /* ========================= Find Acpi Configuration ====================== */
  FindAcpiTable(MachineInfo);



  /* ==================== Load krnl, ccldr and icons ==================== */

  /*  Allocate Kernel Space Address. */

#ifdef _DEBUG
  Print(L"Allocate Kernel Space Address.\n\r");
#endif

  gBS->SetMem((VOID*)CcldrBase, (CCLDR_SIZE - MACHINE_INFO_STRUCT_SIZE), 0);
  // Read krnl image at address 0x1000000
  // Calculate the memory size for Kernel Space Note that the RamSize is 512 MBytes aligned.
  // Kernel Space Size = RamSize was aligned / 4;
  KernelSpaceSize = (((MachineInfo->MemoryInformation.RamSize + 0x20000000 - 1) & (~0x1fffffff)) >> 2);

  Status = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, (KernelSpaceSize / EFI_PAGE_SIZE), &KrnlImageBase);
  if (EFI_ERROR(Status))
  {
    Print(L"[ERROR]: Allocate memory for krnl failed...\n\r");
    goto ExitUefi;
  }

  ReadFileToBufferAt(KRNL_PATH, KrnlImageBase, &KernelBufferSize);
  ReadFileToBufferAt(CCLDR_PATH, CcldrBase, &CcldrBufferSize);

  MachineInfo->MemorySpaceInformation[0].BaseAddress = KrnlImageBase;
  MachineInfo->MemorySpaceInformation[0].Size = KernelSpaceSize;
  MachineInfo->MemorySpaceInformation[1].BaseAddress = (UINTN)MachineInfo;
  MachineInfo->MemorySpaceInformation[1].Size = CCLDR_SIZE;
  MachineInfo->MemorySpaceInformation[2].BaseAddress = KrnlImageBase;
  MachineInfo->MemorySpaceInformation[2].Size = KernelBufferSize;

  // Read Background
  MachineInfo->SumOfSizeOfFilesInBytes = PAGE_ALIGNED(KernelBufferSize);
  FileBuffer = KrnlImageBase + MachineInfo->SumOfSizeOfFilesInBytes;

  ReadFileToBufferAt(BG_PATH, FileBuffer, &FileSize);
  GetPNGSize(BG_PNG_PATH, &PNGWidth, &PNGHeight);
  MachineInfo->Background.Width = (UINT16)PNGWidth;
  MachineInfo->Background.Height = (UINT16)PNGHeight;
  MachineInfo->Background.Addr = (UINTN)FileBuffer;
  MachineInfo->Background.Size = (UINT32)FileSize;

  // Read Font info
  MachineInfo->SumOfSizeOfFilesInBytes += PAGE_ALIGNED(FileSize);
  FileBuffer = KrnlImageBase + MachineInfo->SumOfSizeOfFilesInBytes;
  ReadFileToBufferAt(PRIMARY_FONT_PATH, FileBuffer, &FileSize);
  MachineInfo->Font[0].FntAddr = FileBuffer;
  MachineInfo->Font[0].FntSize = FileSize;

  MachineInfo->SumOfSizeOfFilesInBytes += PAGE_ALIGNED(FileSize);
  FileBuffer = KrnlImageBase + MachineInfo->SumOfSizeOfFilesInBytes;
  ReadFileToBufferAt(PRIMARY_FONT_IMG_PATH, FileBuffer, &FileSize);
  GetPNGSize(PRIMARY_FONT_PNG_PATH, &PNGWidth, &PNGHeight);
  MachineInfo->Font[0].Img.Addr = (UINTN)FileBuffer;
  MachineInfo->Font[0].Img.Width = (UINT16)PNGWidth;
  MachineInfo->Font[0].Img.Height = (UINT16)PNGHeight;
  MachineInfo->Font[0].Img.Size = (UINT32)FileSize;

  MachineInfo->SumOfSizeOfFilesInBytes += PAGE_ALIGNED(FileSize);
  FileBuffer = KrnlImageBase + MachineInfo->SumOfSizeOfFilesInBytes;
  ReadFileToBufferAt(SECONDARY_FONT_PATH, FileBuffer, &FileSize);
  MachineInfo->Font[1].FntAddr = FileBuffer;
  MachineInfo->Font[1].FntSize = FileSize;

  MachineInfo->SumOfSizeOfFilesInBytes += PAGE_ALIGNED(FileSize);
  FileBuffer = KrnlImageBase + MachineInfo->SumOfSizeOfFilesInBytes;
  ReadFileToBufferAt(SECONDARY_FONT_IMG_PATH, FileBuffer, &FileSize);
  GetPNGSize(SECONDARY_FONT_PNG_PATH, &PNGWidth, &PNGHeight);
  MachineInfo->Font[1].Img.Addr = (UINTN)FileBuffer;
  MachineInfo->Font[1].Img.Width = (UINT16)PNGWidth;
  MachineInfo->Font[1].Img.Height = (UINT16)PNGHeight;
  MachineInfo->Font[1].Img.Size = (UINT32)FileSize;


  ReadFileFnit();


#ifdef _DEBUG
  Print(L"Ccldr Starting...\n\r");
#endif

  /* ========================== Exit Services ================================ */
  gBS->GetMemoryMap(&MemMapSize, MemMap, &MapKey, &DescriptorSize, &DesVersion);
  gBS->ExitBootServices(ImageHandle, MapKey);

  void (*ccldr_start)(LOADER_MACHINE_INFORMATION *MachineInfo) = (void *)(CcldrBase);

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
