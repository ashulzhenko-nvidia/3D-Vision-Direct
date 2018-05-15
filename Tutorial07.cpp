//--------------------------------------------------------------------------------------
// File: Tutorial07.cpp
//
// Originally the Tutorial07, now heavily modified to simply demonstrate
// the use of 3D Vision Direct Mode.
//
// http://msdn.microsoft.com/en-us/library/windows/apps/ff729724.aspx
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
//
// Bo3b: 5-8-17
//	This sample is derived from the Microsoft Tutorial07 sample in the 
//	DirectX SDK.  The goal was to use as simple an example as possible
//	but still demonstrate using 3D Vision Direct Mode, using DX11.
//	The code was modified as little as possible, so the pieces demonstrated
//	by the Tutorial07 are still valid.
//
//	Some documention used for this include this whitepaper from NVidia.
//	http://www.nvidia.com/docs/io/40505/wp-05482-001_v01-final.pdf
//	Be wary of that document, the code is completely broken, and misleading
//	in a lot of aspects.  The broad brush strokes are correct.
//
//	The Stereoscopy pdf/presentation gives some good details on how it
//	all works, and a better structure for Direct Mode.
//	http://www.nvidia.com/content/PDF/GDC2011/Stereoscopy.pdf
//
//	This sample is old, but it includes some details on modifying the
//	projection matrix directly that were very helpful. 
//	http://developer.download.nvidia.com/whitepapers/2011/StereoUnproject.zip
// 
// Bo3b: 5-14-17
//	Updated to simplify the code for this code branch.
//	In this branch, the barest minimum of DX11 is used, to make the use of
//	3D Vision Direct Mode more clear.
//--------------------------------------------------------------------------------------

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <directxcolors.h>
#include "resource.h"

#include "nvapi.h"
#include "nvapi_lite_stereo.h"


using namespace DirectX;

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct SimpleVertex
{
	XMFLOAT3 Pos;
	XMFLOAT2 Tex;
};

struct SharedCB
{
	XMMATRIX mWorld;
	XMMATRIX mView;
	XMMATRIX mProjection;

	XMVECTOR mStereoParamsArray[3];
};


//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HINSTANCE                           g_hInst = nullptr;
HWND                                g_hWnd = nullptr;

ID3D11Device*                       g_pd3dDevice = nullptr;
ID3D11DeviceContext*                g_pImmediateContext = nullptr;
IDXGISwapChain*                     g_pSwapChain = nullptr;

ID3D11Texture2D*					g_pBackBuffer = nullptr;
ID3D11RenderTargetView*             g_pRenderTargetView = nullptr;

ID3D11Texture2D*                    g_pOffscreenTexture = nullptr;
ID3D11RenderTargetView*             g_pOffscreenTextureView = nullptr;
ID3D11Texture2D*                    g_pDepthStencil = nullptr;
ID3D11DepthStencilView*             g_pDepthStencilView = nullptr;
ID3D11ShaderResourceView*           g_pPackedDepthTextureSRV = nullptr;

ID3D11RenderTargetView*             g_pOffscreenRTV_Color = nullptr;
ID3D11RenderTargetView*             g_pOffscreenRTV_Depth = nullptr;

ID3D11VertexShader*                 g_pVertexShader = nullptr;
ID3D11GeometryShader*               g_pGeometryShader = nullptr;
ID3D11PixelShader*                  g_pPixelShader = nullptr;
ID3D11InputLayout*                  g_pVertexLayout = nullptr;
ID3D11Buffer*                       g_pVertexBuffer = nullptr;
ID3D11Buffer*                       g_pIndexBuffer = nullptr;

ID3D11VertexShader*                 g_pQuadVertexShader = nullptr;
ID3D11PixelShader*                  g_pQuadPixelShader = nullptr;
ID3D11PixelShader*                  g_pMSQuadPixelShader = nullptr;

ID3D11Buffer*                       g_pSharedCB = nullptr;

XMMATRIX                            g_World;
XMMATRIX                            g_View;
XMMATRIX                            g_Projection;

StereoHandle						g_StereoHandle;
UINT								g_ScreenWidth = 1920;
UINT								g_ScreenHeight = 1080;

D3D11_VIEWPORT						g_Viewport;

bool								g_isMSAA = false;

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow);
HRESULT InitStereo();
HRESULT InitDevice();
HRESULT ActivateStereo();
void CleanupDevice();
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
void RenderFrame();


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	if (FAILED(InitWindow(hInstance, nCmdShow)))
		return 0;

	if (FAILED(InitStereo()))
		return 0;

	if (FAILED(InitDevice()))
	{
		CleanupDevice();
		return 0;
	}

	if (FAILED(ActivateStereo()))
	{
		CleanupDevice();
		return 0;
	}

	// Main message loop
	MSG msg = { 0 };
	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			RenderFrame();
		}
	}

	CleanupDevice();

	return (int)msg.wParam;
}


//--------------------------------------------------------------------------------------
// Register class and create window
//--------------------------------------------------------------------------------------
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow)
{
	// Register class
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, (LPCTSTR)IDI_TUTORIAL1);
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = L"TutorialWindowClass";
	wcex.hIconSm = LoadIcon(wcex.hInstance, (LPCTSTR)IDI_TUTORIAL1);
	if (!RegisterClassEx(&wcex))
		return E_FAIL;

	// Create window
	g_hInst = hInstance;
	RECT rc = { 0, 0, g_ScreenWidth, g_ScreenHeight };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	g_hWnd = CreateWindow(L"TutorialWindowClass", L"Direct3D 11 Tutorial 7",
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance,
		nullptr);
	if (!g_hWnd)
		return E_FAIL;

	ShowWindow(g_hWnd, nCmdShow);

	return S_OK;
}


//--------------------------------------------------------------------------------------
// Setup nvapi, and enable stereo by direct mode for the app.
// This must be called before the Device is created for Direct Mode to work.
//--------------------------------------------------------------------------------------
HRESULT InitStereo()
{
	NvAPI_Status status;

	status = NvAPI_Initialize();
	if (FAILED(status))
		return status;

	// The entire point is to show stereo.  
	// If it's not enabled in the control panel, let the user know.
	NvU8 stereoEnabled;
	status = NvAPI_Stereo_IsEnabled(&stereoEnabled);
	if (FAILED(status) || !stereoEnabled)
	{
		MessageBox(g_hWnd, L"3D Vision is not enabled. Enable it in the NVidia Control Panel.", L"Error", MB_OK);
		return status;
	}

	status = NvAPI_Stereo_SetDriverMode(NVAPI_STEREO_DRIVER_MODE_DIRECT);
	if (FAILED(status))
		return status;

	return status;
}


//--------------------------------------------------------------------------------------
// Activate stereo for the given device.
// This must be called after the device is created.
//--------------------------------------------------------------------------------------
HRESULT ActivateStereo()
{
	NvAPI_Status status;

	status = NvAPI_Stereo_CreateHandleFromIUnknown(g_pd3dDevice, &g_StereoHandle);
	if (FAILED(status))
		return status;

	return status;
}


//--------------------------------------------------------------------------------------
// Helper for compiling shaders with D3DCompile
//
// With VS 11, we could load up prebuilt .cso files instead...
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile(WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
	HRESULT hr = S_OK;

	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	dwShaderFlags |= D3DCOMPILE_DEBUG;

	// Disable optimizations to further improve shader debugging
	dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	ID3DBlob* pErrorBlob = nullptr;
	hr = D3DCompileFromFile(szFileName, nullptr, nullptr, szEntryPoint, szShaderModel,
		dwShaderFlags, 0, ppBlobOut, &pErrorBlob);
	if (FAILED(hr))
	{
		if (pErrorBlob)
		{
			OutputDebugStringA(reinterpret_cast<const char*>(pErrorBlob->GetBufferPointer()));
			pErrorBlob->Release();
		}
		return hr;
	}
	if (pErrorBlob) pErrorBlob->Release();

	return S_OK;
}


//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
HRESULT InitDevice()
{
	HRESULT hr = S_OK;

	UINT createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1;
	sd.BufferDesc.Width = g_ScreenWidth;
	sd.BufferDesc.Height = g_ScreenHeight;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 120;	// Needs to be 120Hz for 3D Vision 
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = g_hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	// Create the simple DX11, Device, SwapChain, and Context.
	hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, nullptr, 0,
		D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pImmediateContext);
	if (FAILED(hr))
		return hr;

	// For DX11 3D, it's required that we run in exclusive full-screen mode, otherwise 3D
	// Vision will not activate.
	hr = g_pSwapChain->SetFullscreenState(TRUE, nullptr);
	if (FAILED(hr))
		return hr;

	// Create a render target view from the backbuffer
	//
	// Since this is derived from the backbuffer, it will also be 2x in width.
	hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&g_pBackBuffer));
	if (FAILED(hr))
		return hr;

	hr = g_pd3dDevice->CreateRenderTargetView(g_pBackBuffer, nullptr, &g_pRenderTargetView);

	DXGI_SAMPLE_DESC sampleDesc = { 1, 0 };
	UINT numQualityLevels;
	//*
	if (SUCCEEDED(g_pd3dDevice->CheckMultisampleQualityLevels(DXGI_FORMAT_B8G8R8A8_UNORM, 4, &numQualityLevels)))
	{
		sampleDesc.Count = 4;
		sampleDesc.Quality = numQualityLevels - 1;
		g_isMSAA = true;
	}//*/

	// Create Offscreen texture
	D3D11_TEXTURE2D_DESC descOffscreen;
	ZeroMemory(&descOffscreen, sizeof(descOffscreen));
	descOffscreen.Width = g_ScreenWidth;
	descOffscreen.Height = g_ScreenHeight;
	descOffscreen.MipLevels = 1;
	descOffscreen.ArraySize = 3; // 2 slices for stereo + 1 for mono
	descOffscreen.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
	descOffscreen.SampleDesc = sampleDesc;
	descOffscreen.Usage = D3D11_USAGE_DEFAULT;
	descOffscreen.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	descOffscreen.CPUAccessFlags = 0;
	descOffscreen.MiscFlags = 0;
	hr = g_pd3dDevice->CreateTexture2D(&descOffscreen, nullptr, &g_pOffscreenTexture);
	if (FAILED(hr))
		return hr;

	D3D11_RENDER_TARGET_VIEW_DESC descOffscreenRTV;
	ZeroMemory(&descOffscreenRTV, sizeof(descOffscreenRTV));
	descOffscreenRTV.Format = DXGI_FORMAT_R8G8B8A8_UINT;
	if (sampleDesc.Count > 1)
	{
		descOffscreenRTV.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
		descOffscreenRTV.Texture2DMSArray.FirstArraySlice = 0;
		descOffscreenRTV.Texture2DMSArray.ArraySize = descOffscreen.ArraySize;
	}
	else
	{
		descOffscreenRTV.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		descOffscreenRTV.Texture2DArray.MipSlice = 0;
		descOffscreenRTV.Texture2DArray.FirstArraySlice = 0;
		descOffscreenRTV.Texture2DArray.ArraySize = descOffscreen.ArraySize;
	}

	hr = g_pd3dDevice->CreateRenderTargetView(g_pOffscreenTexture, &descOffscreenRTV, &g_pOffscreenTextureView);
	if (FAILED(hr))
		return hr;

	D3D11_RENDER_TARGET_VIEW_DESC descOffscreenRTV_Color;
	ZeroMemory(&descOffscreenRTV_Color, sizeof(descOffscreenRTV_Color));
	descOffscreenRTV_Color.Format = DXGI_FORMAT_R8G8B8A8_UINT;
	if (sampleDesc.Count > 1)
	{
		descOffscreenRTV_Color.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
		descOffscreenRTV_Color.Texture2DMSArray.FirstArraySlice = 0;
		descOffscreenRTV_Color.Texture2DMSArray.ArraySize = 2;
	}
	else
	{
		descOffscreenRTV_Color.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		descOffscreenRTV_Color.Texture2DArray.FirstArraySlice = 0;
		descOffscreenRTV_Color.Texture2DArray.ArraySize = 2;
	}

	hr = g_pd3dDevice->CreateRenderTargetView(g_pOffscreenTexture, &descOffscreenRTV_Color, &g_pOffscreenRTV_Color);
	if (FAILED(hr))
		return hr;

	D3D11_RENDER_TARGET_VIEW_DESC descOffscreenRTV_Depth;
	ZeroMemory(&descOffscreenRTV_Depth, sizeof(descOffscreenRTV_Depth));
	descOffscreenRTV_Depth.Format = DXGI_FORMAT_R8G8B8A8_UINT;
	if (sampleDesc.Count > 1)
	{
		descOffscreenRTV_Depth.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
		descOffscreenRTV_Depth.Texture2DMSArray.FirstArraySlice = 2;
		descOffscreenRTV_Depth.Texture2DMSArray.ArraySize = 1;
	}
	else
	{
		descOffscreenRTV_Depth.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		descOffscreenRTV_Depth.Texture2DArray.FirstArraySlice = 2;
		descOffscreenRTV_Depth.Texture2DArray.ArraySize = 1;
	}

	hr = g_pd3dDevice->CreateRenderTargetView(g_pOffscreenTexture, &descOffscreenRTV_Depth, &g_pOffscreenRTV_Depth);
	if (FAILED(hr))
		return hr;

	D3D11_SHADER_RESOURCE_VIEW_DESC descOffscreenSRV;
	descOffscreenSRV.Format = DXGI_FORMAT_R8G8B8A8_UINT;
	if (sampleDesc.Count > 1)
	{
		descOffscreenSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
		descOffscreenSRV.Texture2DMSArray.FirstArraySlice = 2;
		descOffscreenSRV.Texture2DMSArray.ArraySize = 1;
	}
	else
	{
		descOffscreenSRV.Texture2DArray.MostDetailedMip = 0;
		descOffscreenSRV.Texture2DArray.MipLevels = 1;
		descOffscreenSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		descOffscreenSRV.Texture2DArray.FirstArraySlice = 2;
		descOffscreenSRV.Texture2DArray.ArraySize = 1;
	}

	hr = g_pd3dDevice->CreateShaderResourceView(g_pOffscreenTexture, &descOffscreenSRV, &g_pPackedDepthTextureSRV);
	if (FAILED(hr))
		return hr;

	

	// Create depth stencil texture
	D3D11_TEXTURE2D_DESC descDepth;
	ZeroMemory(&descDepth, sizeof(descDepth));
	descDepth.Width = g_ScreenWidth;
	descDepth.Height = g_ScreenHeight;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 3; // 2 slices for stereo + 1 for mono
	descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDepth.SampleDesc = sampleDesc;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;
	hr = g_pd3dDevice->CreateTexture2D(&descDepth, nullptr, &g_pDepthStencil);
	if (FAILED(hr))
		return hr;

	// Create the depth stencil view
	//
	// This is not strictly necessary for our 3D, but is almost always used.
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory(&descDSV, sizeof(descDSV));
	descDSV.Format = descDepth.Format;
	if (sampleDesc.Count > 1)
	{
		descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
		descDSV.Texture2DMSArray.ArraySize = 2;
		descDSV.Texture2DMSArray.FirstArraySlice = 0;
	}
	else
	{
		descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		descDSV.Texture2DArray.ArraySize = 2;
		descDSV.Texture2DArray.FirstArraySlice = 0;
		descDSV.Texture2DArray.MipSlice = 0;
	}
	hr = g_pd3dDevice->CreateDepthStencilView(g_pDepthStencil, nullptr/*&descDSV*/, &g_pDepthStencilView);
	if (FAILED(hr))
		return hr;

	g_Viewport.Width = (FLOAT)g_ScreenWidth;
	g_Viewport.Height = (FLOAT)g_ScreenHeight;
	g_Viewport.MinDepth = 0.0f;
	g_Viewport.MaxDepth = 1.0f;
	g_Viewport.TopLeftX = 0;
	g_Viewport.TopLeftY = 0;

	// Compile the vertex shader
	ID3DBlob* pVSBlob = nullptr;
	hr = CompileShaderFromFile(L"Tutorial07.fx", "VS", "vs_5_0", &pVSBlob);
	if (FAILED(hr))
	{
		MessageBox(nullptr,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	// Create the vertex shader
	hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pVertexShader);
	if (FAILED(hr))
	{
		pVSBlob->Release();
		return hr;
	}

	// Define the input layout
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	UINT numElements = ARRAYSIZE(layout);

	// Create the input layout
	hr = g_pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(),
		pVSBlob->GetBufferSize(), &g_pVertexLayout);
	pVSBlob->Release();
	if (FAILED(hr))
		return hr;

	// Set the input layout
	g_pImmediateContext->IASetInputLayout(g_pVertexLayout);


	// Compile the geometry shader
	ID3DBlob* pGSBlob = nullptr;
	hr = CompileShaderFromFile(L"Tutorial07.fx", "GS", "gs_5_0", &pGSBlob);
	if (FAILED(hr))
	{
		MessageBox(nullptr,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	// Create the geometry shader
	hr = g_pd3dDevice->CreateGeometryShader(pGSBlob->GetBufferPointer(), pGSBlob->GetBufferSize(), nullptr, &g_pGeometryShader);
	pGSBlob->Release();
	if (FAILED(hr))
	{
		return hr;
	}

	// Compile the pixel shader
	ID3DBlob* pPSBlob = nullptr;
	hr = CompileShaderFromFile(L"Tutorial07.fx", "PS", "ps_5_0", &pPSBlob);
	if (FAILED(hr))
	{
		MessageBox(nullptr,
			L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	// Create the pixel shader
	hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pPixelShader);
	pPSBlob->Release();
	if (FAILED(hr))
		return hr;

	{
		// Compile the vertex shader
		ID3DBlob* pVSBlob = nullptr;
		hr = CompileShaderFromFile(L"Tutorial07.fx", "QuadVS", "vs_5_0", &pVSBlob);
		if (FAILED(hr))
		{
			MessageBox(nullptr,
				L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
			return hr;
		}
		hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pQuadVertexShader);
		pVSBlob->Release();
		if (FAILED(hr))
			return hr;

		// Compile the pixel shader
		ID3DBlob* pPSBlob = nullptr;
		hr = CompileShaderFromFile(L"Tutorial07.fx", "QuadPS", "ps_5_0", &pPSBlob);
		if (FAILED(hr))
		{
			MessageBox(nullptr,
				L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
			return hr;
		}

		// Create the pixel shader
		hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pQuadPixelShader);
		pPSBlob->Release();
		if (FAILED(hr))
			return hr;

		// Compile the pixel shader
		ID3DBlob* pPSBlob1 = nullptr;
		hr = CompileShaderFromFile(L"Tutorial07.fx", "MSQuadPS", "ps_5_0", &pPSBlob1);
		if (FAILED(hr))
		{
			MessageBox(nullptr,
				L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
			return hr;
		}

		// Create the pixel shader
		hr = g_pd3dDevice->CreatePixelShader(pPSBlob1->GetBufferPointer(), pPSBlob1->GetBufferSize(), nullptr, &g_pMSQuadPixelShader);
		pPSBlob1->Release();
		if (FAILED(hr))
			return hr;
	}

	// Create vertex buffer for the cube
	SimpleVertex vertices[] =
	{
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT2(1.0f, 0.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT2(0.0f, 0.0f) },
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },

		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT2(0.0f, 0.0f) },
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT2(1.0f, 0.0f) },
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },

		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT2(1.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT2(1.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) },

		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT2(0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT2(0.0f, 0.0f) },
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) },

		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT2(0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT2(1.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT2(1.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT2(0.0f, 0.0f) },

		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) },
	};

	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(SimpleVertex) * 24;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;
	D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = vertices;
	hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pVertexBuffer);
	if (FAILED(hr))
		return hr;

	// Set vertex buffer
	UINT stride = sizeof(SimpleVertex);
	UINT offset = 0;
	g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	// Create index buffer
	// Create vertex buffer
	WORD indices[] =
	{
		3, 1, 0,
		2, 1, 3,

		6, 4, 5,
		7, 4, 6,

		11, 9, 8,
		10, 9, 11,

		14, 12, 13,
		15, 12, 14,

		19, 17, 16,
		18, 17, 19,

		22, 20, 21,
		23, 20, 22
	};

	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(WORD) * 36;
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = 0;
	InitData.pSysMem = indices;
	hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pIndexBuffer);
	if (FAILED(hr))
		return hr;


	// Create the constant buffer
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(SharedCB);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	hr = g_pd3dDevice->CreateBuffer(&bd, nullptr, &g_pSharedCB);
	if (FAILED(hr))
		return hr;

	// Initialize the world matrix
	g_World = XMMatrixIdentity();

	// Initialize the view matrix
	XMVECTOR Eye = XMVectorSet(0.0f, 3.0f, -6.0f, 0.0f);
	XMVECTOR At = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	g_View = XMMatrixLookAtLH(Eye, At, Up);

	// Initialize the projection matrix
	//
	// For the projection matrix, the shaders know nothing about being in stereo, 
	// so this needs to be only ScreenWidth, one per eye.
	g_Projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, (float)g_ScreenWidth / (float)g_ScreenHeight, 0.01f, 100.0f);

	return S_OK;
}


//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------
void CleanupDevice()
{
	if (g_pSwapChain) g_pSwapChain->SetFullscreenState(FALSE, nullptr);

	if (g_pImmediateContext) g_pImmediateContext->ClearState();

	if (g_pSharedCB) g_pSharedCB->Release();
	if (g_pVertexBuffer) g_pVertexBuffer->Release();
	if (g_pIndexBuffer) g_pIndexBuffer->Release();
	if (g_pVertexLayout) g_pVertexLayout->Release();

	if (g_pVertexShader) g_pVertexShader->Release();
	if (g_pPixelShader) g_pPixelShader->Release();
	if (g_pDepthStencil) g_pDepthStencil->Release();
	if (g_pDepthStencilView) g_pDepthStencilView->Release();
	if (g_pRenderTargetView) g_pRenderTargetView->Release();

	if (g_pSwapChain) g_pSwapChain->Release();
	if (g_pImmediateContext) g_pImmediateContext->Release();
	if (g_pd3dDevice) g_pd3dDevice->Release();

	if (g_StereoHandle) NvAPI_Stereo_DestroyHandle(g_StereoHandle);
}


//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

		// Note that this tutorial does not handle resizing (WM_SIZE) requests,
		// so we created the window without the resize border.

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

//--------------------------------------------------------------------------------------
// Render a frame, both eyes.
//--------------------------------------------------------------------------------------

void PackFloat(FLOAT in, FLOAT* out)
{
	DWORD dw = *(DWORD*)&in;
	out[0] = dw & 255; out[1] = (dw >> 8) & 255; out[2] = (dw >> 16) & 255; out[3] = (dw >> 24);
}

void RenderFrame()
{
	g_pImmediateContext->OMSetRenderTargets(1, &g_pOffscreenTextureView, g_pDepthStencilView);
	//
	// Clear color in left & right eyes
	FLOAT clearColor[4] = { 0, 0, 128, 255 };
	g_pImmediateContext->ClearRenderTargetView(g_pOffscreenRTV_Color, clearColor);

	// Clear packed depth
	FLOAT clearDepth[4];
	PackFloat(FLT_MAX, clearDepth);
	g_pImmediateContext->ClearRenderTargetView(g_pOffscreenRTV_Depth, clearDepth);

	//
	// Clear the depth buffer to 1.0 (max depth)
	g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

	//
	// Rotate cube around the origin
	//
	g_World = XMMatrixRotationY(GetTickCount64() / 1000.0f);

	//
	// This now includes changing CBChangeOnResize each frame as well, because
	// we need to update the Projection matrix each frame, in case the user changes
	// the 3D settings.
	// The variable names are a bit misleading at present.
	//
	NvAPI_Status status;
	SharedCB cb;
	float pConvergence = 0;
	float pSeparationPercentage = 0;
	float pEyeSeparation = 0;

	if (g_StereoHandle)
	{
		NvAPI_Stereo_GetConvergence(g_StereoHandle, &pConvergence);
		NvAPI_Stereo_GetSeparation(g_StereoHandle, &pSeparationPercentage);
		NvAPI_Stereo_GetEyeSeparation(g_StereoHandle, &pEyeSeparation);
	}

	cb.mStereoParamsArray[0] = { -pEyeSeparation * pSeparationPercentage / 100, pConvergence, 0.0f, 0.0f }; //left eye
	cb.mStereoParamsArray[1] = { +pEyeSeparation * pSeparationPercentage / 100, pConvergence, 0.0f, 0.0f }; //right eye
	cb.mStereoParamsArray[2] = { 0.0f, 0.0f, 0.0f, 0.0f }; //mono


	{
		g_pImmediateContext->RSSetViewports(1, &g_Viewport);
		// Set index buffer
		g_pImmediateContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

		// Set primitive topology
		g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		cb.mWorld = XMMatrixTranspose(g_World);
		cb.mView = XMMatrixTranspose(g_View);
		cb.mProjection = XMMatrixTranspose(g_Projection);
		g_pImmediateContext->UpdateSubresource(g_pSharedCB, 0, nullptr, &cb, 0, 0);

		//
		// Render the cube
		//
		g_pImmediateContext->VSSetShader(g_pVertexShader, nullptr, 0);
		g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pSharedCB);
		g_pImmediateContext->GSSetShader(g_pGeometryShader, nullptr, 0);
		g_pImmediateContext->GSSetConstantBuffers(0, 1, &g_pSharedCB);
		g_pImmediateContext->PSSetShader(g_pPixelShader, nullptr, 0);
		g_pImmediateContext->DrawIndexed(36, 0, 0);
	}

	//
	// Copy left/right eye to back buffer
	//
	NvAPI_Stereo_SetActiveEye(g_StereoHandle, NVAPI_STEREO_EYE_LEFT);
	g_pImmediateContext->ResolveSubresource(g_pBackBuffer, 0, g_pOffscreenTexture, 0, DXGI_FORMAT_R8G8B8A8_UNORM);

	NvAPI_Stereo_SetActiveEye(g_StereoHandle, NVAPI_STEREO_EYE_RIGHT);
	g_pImmediateContext->ResolveSubresource(g_pBackBuffer, 0, g_pOffscreenTexture, 1, DXGI_FORMAT_R8G8B8A8_UNORM);

	if (::GetAsyncKeyState(VK_SPACE))
	{
		NvAPI_Stereo_SetActiveEye(g_StereoHandle, NVAPI_STEREO_EYE_MONO);

		g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

		g_pImmediateContext->VSSetShader(g_pQuadVertexShader, nullptr, 0);
		g_pImmediateContext->GSSetShader(nullptr, nullptr, 0);
		g_pImmediateContext->PSSetShader(g_isMSAA ? g_pMSQuadPixelShader : g_pQuadPixelShader, nullptr, 0);
		g_pImmediateContext->PSSetShaderResources(0, 1, &g_pPackedDepthTextureSRV);

		g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		g_pImmediateContext->Draw(4, 0);

		ID3D11ShaderResourceView* nullSRV = nullptr;
		g_pImmediateContext->PSSetShaderResources(0, 1, &nullSRV);
	}

	//
	// Present our back buffer to our front buffer
	//
	// In stereo mode, the driver knows to use the 2x width buffer, and
	// present each eye in order.
	//
	g_pSwapChain->Present(0, 0);
}
