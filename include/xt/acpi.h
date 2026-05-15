#ifndef __XT_ACPI_H__
#define __XT_ACPI_H__

#include <xt/result.h>

#include <stdint.h>

typedef struct DescriptionHeader {
    char Signature[4];
    uint32_t Length;
    uint8_t Revision;
    uint8_t Checksum;
    char OEMID[6];
    uint64_t OEMTID;
    uint32_t OEMRevision;
    uint32_t CreatorID;
    uint32_t CreatorRevision;
} __attribute__((packed)) DescriptionHeader;

typedef struct XSDT {
    DescriptionHeader Header;
    uint64_t          Tables[1];
} __attribute__((packed)) XSDT;

typedef struct RSDP {
    char Signature[8];
    uint8_t Checksum;
    char OEMID[6];
    uint8_t Revision;
    uint32_t RsdtAddress;
    uint32_t Length;
    XSDT* XsdtAddress;
    uint8_t ExtendedCheckSum;
    uint8_t Reserved[3];
} __attribute__((packed)) RSDP;

typedef struct MMECSPAAS {
    uint64_t BaseAddress;
    uint16_t PCISegmentGroupNumber;
    uint8_t StartBusNumber;
    uint8_t EndBusNumber;
    uint32_t Reserved;
} __attribute__((packed)) MMECSPAAS;

typedef struct MCFG {
    DescriptionHeader Header;
    uint64_t Reserved;
    MMECSPAAS ConfigurationSpacesFirst;
} __attribute__((packed)) MCFG;

#define GAS_SPACE_SYSTEM_MEMORY 0
#define GAS_SPACE_SYSTEM_IO     1

typedef struct GenericAddressStructure
{
  uint8_t AddressSpace;
  uint8_t BitWidth;
  uint8_t BitOffset;
  uint8_t AccessSize;
  uint64_t Address;
} __attribute__((packed)) GenericAddressStructure;

typedef struct FADT
{
    DescriptionHeader Header;
    uint32_t FirmwareCtrl;
    uint32_t Dsdt;

    uint8_t  Reserved;
    uint8_t  PreferredPowerManagementProfile;
    uint16_t SCI_Interrupt;
    uint32_t SMI_CommandPort;
    uint8_t  AcpiEnable;
    uint8_t  AcpiDisable;
    uint8_t  S4BIOS_REQ;
    uint8_t  PSTATE_Control;
    uint32_t PM1aEventBlock;
    uint32_t PM1bEventBlock;
    uint32_t PM1aControlBlock;
    uint32_t PM1bControlBlock;
    uint32_t PM2ControlBlock;
    uint32_t PMTimerBlock;
    uint32_t GPE0Block;
    uint32_t GPE1Block;
    uint8_t  PM1EventLength;
    uint8_t  PM1ControlLength;
    uint8_t  PM2ControlLength;
    uint8_t  PMTimerLength;
    uint8_t  GPE0Length;
    uint8_t  GPE1Length;
    uint8_t  GPE1Base;
    uint8_t  CStateControl;
    uint16_t WorstC2Latency;
    uint16_t WorstC3Latency;
    uint16_t FlushSize;
    uint16_t FlushStride;
    uint8_t  DutyOffset;
    uint8_t  DutyWidth;
    uint8_t  DayAlarm;
    uint8_t  MonthAlarm;
    uint8_t  Century;

    uint16_t BootArchitectureFlags;
    uint8_t  Reserved2;
    uint32_t Flags;

    GenericAddressStructure ResetReg;
    uint8_t  ResetValue;
    uint8_t  Reserved3[3];

    uint64_t X_FirmwareControl;
    uint64_t X_Dsdt;

    GenericAddressStructure X_PM1aEventBlock;
    GenericAddressStructure X_PM1bEventBlock;
    GenericAddressStructure X_PM1aControlBlock;
    GenericAddressStructure X_PM1bControlBlock;
    GenericAddressStructure X_PM2ControlBlock;
    GenericAddressStructure X_PMTimerBlock;
    GenericAddressStructure X_GPE0Block;
    GenericAddressStructure X_GPE1Block;
} FADT;

typedef struct HPET {
    DescriptionHeader header;
    uint8_t hardware_rev_id;
    uint8_t comparator_count:5;
    uint8_t counter_size:1;
    uint8_t reserved:1;
    uint8_t legacy_replacement:1;
    uint16_t pci_vendor_id;
    GenericAddressStructure address;
    uint8_t hpet_number;
    uint16_t minimum_tick;
    uint8_t page_protection;
} __attribute__((packed)) HPET;

XTResult xtACPIInit();

XTResult xtFindACPITable(const char* signature, void** out);

#endif