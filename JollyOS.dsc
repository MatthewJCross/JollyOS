[Defines]
PLATFORM_NAME                  = JollyOS
PLATFORM_GUID                  = 9B5A3C71-6D42-4F18-A8E3-2C7D1B904F65
PLATFORM_VERSION               = 1.0
DSC_SPECIFICATION              = 0x0001001B
OUTPUT_DIRECTORY               = Build/JollyOS
SUPPORTED_ARCHITECTURES        = X64
BUILD_TARGETS                  = DEBUG|RELEASE
SKUID_IDENTIFIER               = DEFAULT

[LibraryClasses]
UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf
UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
CpuLib|MdePkg/Library/BaseCpuLib/BaseCpuLib.inf
DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
RegisterFilterLib|MdePkg/Library/RegisterFilterLibNull/RegisterFilterLibNull.inf
PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
StackCheckLib|MdePkg/Library/StackCheckLibNull/StackCheckLibNull.inf

[Components]
Bootloader/Bootloader.inf

