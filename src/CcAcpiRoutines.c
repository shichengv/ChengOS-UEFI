#include "include/FrameWork.h"
#include "include/Wrapper.h"
#include "include/AcpiSDT.h"
#include "include/MachineInfo.h"

STATIC EFI_ACPI_SDT_PROTOCOL *gAcpiSdt;
extern EFI_SYSTEM_TABLE *gSystemTable;


STATIC UINT32 CompareGuid(CONST UINT64 *dst, CONST UINT64 *src)
{
  if (dst[0] == src[0] && dst[1] == src[1])
  {
    return 1;
  }
  return 0;
}

STATIC struct _RSDP *ExamineEfiConfigurationTable()
{

  struct _RSDP *RSDP = NULL;
  for (UINTN i = 0; i < gSystemTable->NumberOfTableEntries; i++)
  {
    EFI_CONFIGURATION_TABLE *ConfigurationTable = &gSystemTable->ConfigurationTable[i];
    // Check if the current entry is the extended system description pointer
    if (CompareGuid((UINT64 *)ConfigurationTable, (UINT64 *)&gEfiAcpiTableGuid))
    {
      // Found the extended system description pointer
      RSDP = ConfigurationTable->VendorTable;
      break;
    }
  }
  return RSDP;
}

VOID FindAcpiTable(IN LOADER_MACHINE_INFORMATION *MachineInfo)
{
  UINTN MaskSet = NOTHING_WAS_FOUND;
  VOID *AcpiSystemDescTablePtr = 0;
  UINT32 AcpiVersion;
  UINT64 AcpiTableKey;

  MachineInfo->AcpiInformation.RSDP = ExamineEfiConfigurationTable();
  MachineInfo->AcpiInformation.XSDT.Header = (struct _ACPISDTHeader *)MachineInfo->AcpiInformation.RSDP->XsdtAddress;
  MachineInfo->AcpiInformation.XSDT.Entry = (UINTN *)((UINTN)MachineInfo->AcpiInformation.XSDT.Header + 36);
  MachineInfo->AcpiInformation.FADT = (struct _FADT *)(MachineInfo->AcpiInformation.XSDT.Entry[0]);

  if (MachineInfo->AcpiInformation.FADT->h.Revision == 1)
  {
    MachineInfo->AcpiInformation.DSDT = (struct _DSDT *)((UINTN)MachineInfo->AcpiInformation.FADT->Dsdt);
  }
  // for higher acpi version than acpi version 1.0
  else
  {
    MachineInfo->AcpiInformation.DSDT = (struct _DSDT *)MachineInfo->AcpiInformation.FADT->X_Dsdt;
  }

  WpLocateProtocol(&gEfiAcpiSdtProtocolGuid, NULL, (VOID**)&gAcpiSdt, L"AcpiStdProtocol");
  
  /*  Acpi Configuration  */
  for (UINT32 i = 0; MaskSet != ALL_HAVE_BEEN_FOUND; i++)
  {
    gAcpiSdt->GetAcpiTable(i, (EFI_ACPI_SDT_HEADER **)&AcpiSystemDescTablePtr,
                           &AcpiVersion, &AcpiTableKey);

    // Find APIC Table
    if (*(UINT32 *)AcpiSystemDescTablePtr == APIC_SIGNATURE)
    {
#ifdef _DEBUG
      Print(L"APIC Table found!\n\r");
#endif
      MachineInfo->AcpiInformation.APIC.DescriptorTablePtr = AcpiSystemDescTablePtr;
      MachineInfo->AcpiInformation.APIC.Version = AcpiVersion;
      MachineInfo->AcpiInformation.APIC.TableKey = AcpiTableKey;
      MaskSet |= APIC_TABLE_MASK;
      continue;
    }
  }
}