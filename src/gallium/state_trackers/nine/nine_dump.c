
#include "nine_dump.h"
#include "nine_debug.h"

const char *nine_D3DDEVTYPE_to_str(D3DDEVTYPE type)
{
    switch (type) {
    case D3DDEVTYPE_HAL: return "HAL";
    case D3DDEVTYPE_NULLREF: return "NULLREF";
    case D3DDEVTYPE_REF: return "REF";
    case D3DDEVTYPE_SW: return "SW";
    default:
       return "(D3DDEVTYPE:?)";
    }
}

void
nine_dump_D3DADAPTER_IDENTIFIER9(unsigned ch, const D3DADAPTER_IDENTIFIER9 *id)
{
    DBG_FLAG(ch, "D3DADAPTER_IDENTIFIER9(%p):\n"
             "Driver: %s\n"
             "Description: %s\n"
             "DeviceName: %s\n"
             "DriverVersion: %u.%u\n"
             "VendorId: %x\n"
             "DeviceId: %x\n"
             "SubSysId: %u\n"
             "Revision: %u\n"
             "GUID: %08x.%04x.%04x.%02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x\n"
             "WHQLLevel: %u\n", id, id->Driver, id->Description,
             id->DeviceName,
             id->DriverVersionLowPart, id->DriverVersionHighPart,
             id->VendorId, id->DeviceId, id->SubSysId,
             id->Revision,
             id->DeviceIdentifier.Data1,
             id->DeviceIdentifier.Data2,
             id->DeviceIdentifier.Data3,
             id->DeviceIdentifier.Data4[0],
             id->DeviceIdentifier.Data4[1],
             id->DeviceIdentifier.Data4[2],
             id->DeviceIdentifier.Data4[3],
             id->DeviceIdentifier.Data4[4],
             id->DeviceIdentifier.Data4[5],
             id->DeviceIdentifier.Data4[6],
             id->DeviceIdentifier.Data4[7],
             id->WHQLLevel);
}
