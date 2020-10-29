#include <Windows.h>
#include "detours.h"
#include <d3d9.h>
#include <d3dx9.h>
#include <math.h>
#include <stdio.h>
#include <vector>
#include <conio.h>

typedef HRESULT(WINAPI* f_EndScene)(IDirect3DDevice9* pDevice);
typedef HRESULT(WINAPI* f_DrawIndexedPrimitive)(LPDIRECT3DDEVICE9 pDevice, D3DPRIMITIVETYPE PrimType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount);
typedef HRESULT(WINAPI* f_Reset)(LPDIRECT3DDEVICE9 pDevice, D3DPRESENT_PARAMETERS* d3dpp);

f_EndScene oEndScene = nullptr;
f_DrawIndexedPrimitive oDrawIndexedPrimitive = nullptr;
f_Reset oReset = nullptr;

D3DPRESENT_PARAMETERS d3dpp = { NULL };

LPDIRECT3D9 pD3D = nullptr;
LPDIRECT3DDEVICE9 pD3DDevice = nullptr;
HWND hWndD3D = NULL;

std::vector<DWORD> vecBaseTexture;
LPDIRECT3DBASETEXTURE9 pBaseTexture = nullptr;

LPDIRECT3DTEXTURE9 pGreen = NULL;
LPDIRECT3DTEXTURE9 pTexture = NULL;

LPD3DXFONT pFont = nullptr;
D3DVIEWPORT9 pViewPort = { NULL };
D3DLOCKED_RECT d3d_rect = { NULL };

char strBuf[260] = { NULL };

UINT iStride = 0;
UINT iBaseTex = 0;
bool Found = false;

DWORD getVF(DWORD classInst, DWORD funcIndex)
{
	DWORD VFTable = *((DWORD*)classInst);
	DWORD hookAddress = VFTable + funcIndex * sizeof(DWORD);
	return *((DWORD*)hookAddress);
}

BOOL getD3DDevice()
{
	WNDCLASSEXA wc = { sizeof(WNDCLASSEX), CS_CLASSDC, DefWindowProc, 0L, 0L, GetModuleHandleA(NULL), NULL, NULL, NULL, NULL, "DX", NULL };
	RegisterClassExA(&wc);

	hWndD3D = CreateWindowA("DX", NULL, WS_OVERLAPPEDWINDOW, 100, 100, 600, 600, GetDesktopWindow(), NULL, wc.hInstance, NULL);

	LPDIRECT3D9 pD3D = Direct3DCreate9(D3D_SDK_VERSION);
	if (!pD3D)
		return FALSE;

	D3DPRESENT_PARAMETERS d3dpp;
	ZeroMemory(&d3dpp, sizeof(d3dpp));
	d3dpp.Windowed = TRUE;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.hDeviceWindow = hWndD3D;

	HRESULT res = pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWndD3D, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pD3DDevice);
	if (FAILED(res))
		return FALSE;

	return TRUE;
}

HRESULT WINAPI Hooked_DrawIndexedPrimitive(LPDIRECT3DDEVICE9 pDevice, D3DPRIMITIVETYPE PrimType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount)
{
	LPDIRECT3DVERTEXBUFFER9 Stream_Data = nullptr;
	UINT Offset = 0;
	UINT Stride = 0;

	if (pDevice->GetStreamSource(0, &Stream_Data, &Offset, &Stride) == S_OK)
		Stream_Data->Release();

	if (Stride == iStride)
	{
		pDevice->GetTexture(0, &pBaseTexture);
		Found = false;

		for (UINT i = 0; i < vecBaseTexture.size(); i++)
			if (vecBaseTexture[i] == (DWORD)pBaseTexture)
				Found = true;

		if (Found == false)
			vecBaseTexture.push_back((DWORD)pBaseTexture);

		if (vecBaseTexture[iBaseTex] == (DWORD)pBaseTexture && pGreen)
		{
			pDevice->SetTexture(0, pGreen);
			pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
			oDrawIndexedPrimitive(pDevice, PrimType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
			pDevice->SetRenderState(D3DRS_ZENABLE, TRUE);

			printf("[%d / %d] NumVertex : %d, primCount : %d\n", iStride, iBaseTex, NumVertices, primCount);
		}
	}

	return oDrawIndexedPrimitive(pDevice, PrimType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
}

HRESULT WINAPI Hooked_EndScene(LPDIRECT3DDEVICE9 pDevice)
{
	pDevice->GetViewport(&pViewPort);

	RECT FRect = { pViewPort.Width - 250, pViewPort.Height - 300, pViewPort.Width, pViewPort.Height };

	if (pGreen == NULL)
	{
		if (pDevice->CreateTexture(8, 8, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &pGreen, NULL) == S_OK)
		{
			if (pDevice->CreateTexture(8, 8, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &pTexture, NULL) == S_OK)
			{
				if (pTexture->LockRect(0, &d3d_rect, 0, D3DLOCK_DONOTWAIT | D3DLOCK_NOSYSLOCK) == S_OK)
				{
					for (UINT xy = 0; xy < 8 * 8; xy++)
					{
						((PDWORD)d3d_rect.pBits)[xy] = 0xFF00FF00;
					}

					pTexture->UnlockRect(0);
					pDevice->UpdateTexture(pTexture, pGreen);
					pTexture->Release();
				}
			}
		}
	}

	if (pFont == NULL)
	{
		D3DXCreateFontA(pDevice, 16, 0, 700, 0, 0, 1, 0, 0, DEFAULT_PITCH | FF_DONTCARE, "Calibri", &pFont);
	}

	sprintf(strBuf, "STRIDE LOGGER V1\nNum of Textures: %d\niStride: %d\niBaseTex : %d", vecBaseTexture.size(), iStride, iBaseTex);

	if (pFont)
	{
		pFont->DrawTextA(0, strBuf, -1, &FRect, DT_CENTER | DT_NOCLIP, 0xFF00FF00);
	}

	if (GetAsyncKeyState(VK_NUMPAD1) & 1)
	{
		iStride++;
		vecBaseTexture.clear();
		iBaseTex = 0;
	}

	if (GetAsyncKeyState(VK_NUMPAD2) & 1)
	{
		if (iStride > 0)
		{
			iStride--;
			vecBaseTexture.clear();
			iBaseTex = 0;
		}
	}

	if (GetAsyncKeyState(VK_NUMPAD3) & 1)
	{
		if (iBaseTex < vecBaseTexture.size() - 1)
		{
			iBaseTex++;
		}
	}

	if (GetAsyncKeyState(VK_NUMPAD4) & 1)
	{
		if (iBaseTex > 0)
		{
			iBaseTex--;
		}
	}

	if (GetAsyncKeyState(VK_NUMPAD0) & 1)
	{
		iStride = 0;
		iBaseTex = 0;
	}

	return oEndScene(pDevice);
}

HRESULT WINAPI Hooked_Reset(LPDIRECT3DDEVICE9 pDevice, D3DPRESENT_PARAMETERS* d3dpp)
{
	if (pFont) { pFont->Release(); pFont = NULL; }
	if (pGreen) { pGreen->Release(); pGreen = NULL; }

	return oReset(pDevice, d3dpp);
}

DWORD WINAPI MainThread(LPVOID param)
{
	getD3DDevice();

	oEndScene = (f_EndScene)DetourFunction((PBYTE)getVF((DWORD)pD3DDevice, 42), (PBYTE)Hooked_EndScene);
	oReset = (f_Reset)DetourFunction((PBYTE)getVF((DWORD)pD3DDevice, 16), (PBYTE)Hooked_Reset);
	oDrawIndexedPrimitive = (f_DrawIndexedPrimitive)DetourFunction((PBYTE)getVF((DWORD)pD3DDevice, 82), (PBYTE)Hooked_DrawIndexedPrimitive);

	if (AllocConsole())
	{
		freopen("CONIN$", "rb", stdin);
		freopen("CONOUT$", "wb", stdout);
		freopen("CONOUT$", "wb", stderr);
	}

	return 1;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		CreateThread(0, 0, MainThread, hModule, 0, 0);
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}

	return TRUE;
}
