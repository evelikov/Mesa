#include "d3d9.h"
#include "d3dadapter/d3dadapter9.h"

typedef struct ID3DWineDriver ID3DWineDriver;

/* Class representing a Wine backend */
typedef struct ID3DWineDriverVtbl
{
    /* IUnknown */
    HRESULT (WINAPI *QueryInterface)(ID3DWineDriver *This, REFIID riid, void **ppvObject);
    ULONG (WINAPI *AddRef)(ID3DWineDriver *This);
    ULONG (WINAPI *Release)(ID3DWineDriver *This);

    /* ID3DWineDriver */
    HRESULT (WINAPI *CreatePresentGroup)(ID3DWineDriver *This, HWND hFocusWnd, D3DPRESENT_PARAMETERS *pParams, unsigned nParams, ID3DPresentGroup **ppPresentGroup);
    HRESULT (WINAPI *CreateAdapter9)(ID3DWineDriver *This, HDC hdc, ID3DAdapter9 **ppAdapter);
} ID3DWineDriverVtbl;

struct ID3DWineDriver
{
    ID3DWineDriverVtbl *lpVtbl;
};

/* IUnknown macros */
#define ID3DWineDirver_QueryInterface(p,a,b) (p)->lpVtbl->QueryInterface(p,a,b)
#define ID3DWineDirver_AddRef(p) (p)->lpVtbl->AddRef(p)
#define ID3DWineDirver_Release(p) (p)->lpVtbl->Release(p)
/* ID3DWineDriver macros */
#define ID3DWineDriver_CreatePresentGroup(p,a,b,c,d) (p)->lpVtbl->CreatePresentGroup(p,a,b,c,d)
#define ID3DWineDriver_CreateAdapter9(p,a,b) (p)->lpVtbl->CreateAdapter9(p,a,b)
