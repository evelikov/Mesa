
#include "nine_dump.h"
#include "nine_debug.h"

#include <stdio.h>
#include "util/u_memory.h"

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
             "DriverVersion: %08x.%08x\n"
             "VendorId: %x\n"
             "DeviceId: %x\n"
             "SubSysId: %x\n"
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

#define C2S(args...) p += snprintf(&s[p],c-p,args)
void
nine_dump_D3DCAPS9(unsigned ch, const D3DCAPS9 *caps)
{
    const int c = 1 << 16;
    int p = 0;
    char *s = (char *)MALLOC(c);

    if (!s) {
        DBG_FLAG(ch, "D3DCAPS9(%p): (out of memory)\n", caps);
        return;
    }

    C2S("DeviceType: %s\n", nine_D3DDEVTYPE_to_str(caps->DeviceType));
    C2S("AdapterOrdinal: %u\nCaps:", caps->AdapterOrdinal);
    if (caps->Caps & 0x20000)
        C2S(" READ_SCANLINE");
    if (caps->Caps & 0)
        C2S(" OVERLAY");
    C2S("\nCaps2:");
    if (caps->Caps2 & D3DCAPS2_CANAUTOGENMIPMAP)
        C2S(" AUTOGENMIPMAP");
    if (caps->Caps2 & D3DCAPS2_CANCALIBRATEGAMMA)
        C2S(" CALIBRATEGAMMA");
    C2S("\nCursorCaps:");
    if (caps->CursorCaps & D3DCURSORCAPS_COLOR)
        C2S(" COLOR");
    if (caps->CursorCaps & D3DCURSORCAPS_LOWRES)
        C2S(" LOWRES");

    FREE(s);
}
