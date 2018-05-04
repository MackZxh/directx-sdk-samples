//--------------------------------------------------------------------------------------
// File: Tutorial04.cpp
//
// This application displays a 3D cube using Direct3D 11
//
// http://msdn.microsoft.com/en-us/library/windows/apps/ff729721.aspx
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "resource.h"
#include <windows.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <directxcolors.h>
using namespace DirectX;
#include <wrl.h>
using namespace Microsoft::WRL;

//#include <comdef.h>
//#include <comdefsp.h>
#include <memory>

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct SimpleVertex
{
	XMFLOAT3 Pos;
	XMFLOAT4 Color;
};


struct ConstantBuffer
{
	XMMATRIX mWorld;
	XMMATRIX mView;
	XMMATRIX mProjection;
};


//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HINSTANCE               g_hInst = nullptr;
HWND                    g_hWnd = nullptr;
D3D_DRIVER_TYPE         g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL       g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ComPtr<ID3D11Device>           g_pd3dDevice = nullptr;
ComPtr<ID3D11DeviceContext>    g_pImmediateContext = nullptr;
ComPtr<IDXGISwapChain>         g_pSwapChain = nullptr;
ComPtr<ID3D11RenderTargetView> g_pRenderTargetView = nullptr;
ComPtr<ID3D11VertexShader>     g_pVertexShader = nullptr;
ComPtr<ID3D11PixelShader>      g_pPixelShader = nullptr;
ComPtr<ID3D11InputLayout>      g_pVertexLayout = nullptr;
ComPtr<ID3D11Buffer>           g_pVertexBuffer = nullptr;
ComPtr<ID3D11Buffer>           g_pIndexBuffer = nullptr;
ComPtr<ID3D11Buffer>           g_pConstantBuffer = nullptr;
XMMATRIX                g_World;
XMMATRIX                g_View;
XMMATRIX                g_Projection;

typedef struct _InstanceType {
	XMFLOAT3 position;
}InstanceType;

namespace {
	//_COM_SMARTPTR_TYPEDEF(ID3D11Buffer, __uuidof(ID3D11Buffer));
	//ID3D11BufferPtr g_pInstanceBuffer{ nullptr };

	ComPtr<ID3D11Buffer> g_pInstanceBuffer{ nullptr };
	UINT g_InstanceCount{ 0 };
}

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow);
HRESULT InitDevice();
void CleanupDevice();
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
HRESULT InitResource();
void Update();
void Render();


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	if (FAILED(InitWindow(hInstance, nCmdShow)))
		return -1;

	if (FAILED(InitDevice()))
	{
		CleanupDevice();
		return -2;
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
			Update();
			Render();
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
	RECT rc = { 0, 0, 800, 600 };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	g_hWnd = CreateWindow(L"TutorialWindowClass", L"Direct3D 11 Tutorial 4: 3D Spaces",
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance,
		nullptr);
	if (!g_hWnd)
		return E_FAIL;

	ShowWindow(g_hWnd, nCmdShow);

	return S_OK;
}


//--------------------------------------------------------------------------------------
// Helper for compiling shaders with D3DCompile
//
// With VS 11, we could load up prebuilt .cso files instead...
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile(const WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
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

	ComPtr<ID3DBlob> pErrorBlob = nullptr;
	hr = D3DCompileFromFile(szFileName, nullptr, nullptr, szEntryPoint, szShaderModel,
		dwShaderFlags, 0, ppBlobOut, pErrorBlob.ReleaseAndGetAddressOf());
	if (FAILED(hr))
	{
		if (pErrorBlob)
		{
			OutputDebugStringA(reinterpret_cast<const char*>(pErrorBlob->GetBufferPointer()));
			//pErrorBlob->Release();
		}
		return hr;
	}
	//if (pErrorBlob) pErrorBlob->Release();

	return S_OK;
}


//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
HRESULT InitDevice()
{
	HRESULT hr = S_OK;

	RECT rc;
	GetClientRect(g_hWnd, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	UINT createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_DRIVER_TYPE driverTypes[] =
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
	};
	UINT numDriverTypes = ARRAYSIZE(driverTypes);

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	{
		for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
		{
			g_driverType = driverTypes[driverTypeIndex];
			hr = D3D11CreateDevice(nullptr, g_driverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels,
				D3D11_SDK_VERSION, g_pd3dDevice.ReleaseAndGetAddressOf(), &g_featureLevel, g_pImmediateContext.ReleaseAndGetAddressOf());

			if (hr == E_INVALIDARG)
			{
				// DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it
				hr = D3D11CreateDevice(nullptr, g_driverType, nullptr, createDeviceFlags, &featureLevels[1], numFeatureLevels - 1,
					D3D11_SDK_VERSION, g_pd3dDevice.ReleaseAndGetAddressOf(), &g_featureLevel, g_pImmediateContext.ReleaseAndGetAddressOf());
			}

			if (SUCCEEDED(hr))
				break;
		}
		if (FAILED(hr))
			return hr;

		// Obtain DXGI factory from device (since we used nullptr for pAdapter above)
		ComPtr<IDXGIFactory1> dxgiFactory = nullptr;
		{
			ComPtr<IDXGIDevice> dxgiDevice = nullptr;
			hr = g_pd3dDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(dxgiDevice.ReleaseAndGetAddressOf()));
			if (SUCCEEDED(hr))
			{
				ComPtr<IDXGIAdapter> adapter = nullptr;
				hr = dxgiDevice->GetAdapter(adapter.ReleaseAndGetAddressOf());
				if (SUCCEEDED(hr))
				{
					hr = adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(dxgiFactory.ReleaseAndGetAddressOf()));
					//adapter->Release();
				}
				//dxgiDevice->Release();
			}
		}
		if (FAILED(hr))
			return hr;

		// Create swap chain
		ComPtr<IDXGIFactory2> dxgiFactory2 = nullptr;
		hr = dxgiFactory->QueryInterface(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(dxgiFactory2.ReleaseAndGetAddressOf()));
		if (dxgiFactory2)
		{
			ComPtr<ID3D11Device1>          pd3dDevice1 = nullptr;
			ComPtr<IDXGISwapChain1>        pSwapChain1 = nullptr;
			// DirectX 11.1 or later
			hr = g_pd3dDevice->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(pd3dDevice1.ReleaseAndGetAddressOf()));
			if (SUCCEEDED(hr))
			{
				ComPtr<ID3D11DeviceContext1>   pImmediateContext1 = nullptr;
				hr = g_pImmediateContext->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(pImmediateContext1.ReleaseAndGetAddressOf()));
				if (FAILED(hr)) {
					return hr;
				}
			}

			DXGI_SWAP_CHAIN_DESC1 sd = {};
			sd.Width = width;
			sd.Height = height;
			sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			sd.SampleDesc.Count = 1;
			sd.SampleDesc.Quality = 0;
			sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			sd.BufferCount = 1;

			hr = dxgiFactory2->CreateSwapChainForHwnd(g_pd3dDevice.Get(), g_hWnd, &sd, nullptr, nullptr, pSwapChain1.ReleaseAndGetAddressOf());
			if (SUCCEEDED(hr))
			{
				hr = pSwapChain1->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(g_pSwapChain.ReleaseAndGetAddressOf()));
				//pSwapChain1->Release();
				//pSwapChain1 = nullptr;
			}

			//dxgiFactory2->Release();
		}
		else
		{
			// DirectX 11.0 systems
			DXGI_SWAP_CHAIN_DESC sd = {};
			sd.BufferCount = 1;
			sd.BufferDesc.Width = width;
			sd.BufferDesc.Height = height;
			sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			sd.BufferDesc.RefreshRate.Numerator = 60;
			sd.BufferDesc.RefreshRate.Denominator = 1;
			sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			sd.OutputWindow = g_hWnd;
			sd.SampleDesc.Count = 1;
			sd.SampleDesc.Quality = 0;
			sd.Windowed = TRUE;

			hr = dxgiFactory->CreateSwapChain(g_pd3dDevice.Get(), &sd, g_pSwapChain.ReleaseAndGetAddressOf());
		}

		// Note this tutorial doesn't handle full-screen swapchains so we block the ALT+ENTER shortcut
		dxgiFactory->MakeWindowAssociation(g_hWnd, DXGI_MWA_NO_ALT_ENTER);

		//dxgiFactory->Release();

		if (FAILED(hr))
			return hr;
	}

	{
		// Create a render target view
		ComPtr<ID3D11Texture2D> pBackBuffer = nullptr;
		hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(pBackBuffer.ReleaseAndGetAddressOf()));
		if (FAILED(hr))
			return hr;

		hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer.Get(), nullptr, g_pRenderTargetView.ReleaseAndGetAddressOf());
		//pBackBuffer->Release();
		//pBackBuffer = nullptr;
		if (FAILED(hr))
			return hr;

		g_pImmediateContext->OMSetRenderTargets(1, g_pRenderTargetView.GetAddressOf(), nullptr);

		// Setup the viewport
		D3D11_VIEWPORT vp;
		vp.Width = (FLOAT)width;
		vp.Height = (FLOAT)height;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;
		g_pImmediateContext->RSSetViewports(1, &vp);
	}

	{
		// Compile the vertex shader
		ComPtr<ID3DBlob> pVSBlob = nullptr;
		hr = CompileShaderFromFile(L"Tutorial04.fx", "VS", "vs_4_0", pVSBlob.ReleaseAndGetAddressOf());
		if (FAILED(hr))
		{
			MessageBox(nullptr,
				L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
			return hr;
		}

		// Create the vertex shader
		hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, g_pVertexShader.ReleaseAndGetAddressOf());
		if (FAILED(hr))
		{
			//pVSBlob->Release();
			//pVSBlob = nullptr;
			return hr;
		}

		// Define the input layout
		D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1},
		};
		UINT numElements = ARRAYSIZE(layout);

		// Create the input layout
		hr = g_pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(),
			pVSBlob->GetBufferSize(), &g_pVertexLayout);
		//pVSBlob->Release();
		//pVSBlob = nullptr;
		if (FAILED(hr))
			return hr;
	}

	{
		// Compile the pixel shader
		ComPtr<ID3DBlob> pPSBlob = nullptr;
		hr = CompileShaderFromFile(L"Tutorial04.fx", "PS", "ps_4_0", pPSBlob.ReleaseAndGetAddressOf());
		if (FAILED(hr))
		{
			MessageBox(nullptr,
				L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
			return hr;
		}

		// Create the pixel shader
		hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, g_pPixelShader.ReleaseAndGetAddressOf());
		//pPSBlob->Release();
		//pPSBlob = nullptr;
		if (FAILED(hr))
			return hr;
	}


	{
		// Initialize the world matrix
		g_World = XMMatrixIdentity();

		// Initialize the view matrix
		XMVECTOR Eye = XMVectorSet(0.0f, 1.0f, -5.0f, 0.0f);
		XMVECTOR At = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		g_View = XMMatrixLookAtLH(Eye, At, Up);

		// Initialize the projection matrix
		g_Projection = XMMatrixPerspectiveFovLH(XM_PIDIV2, width / (FLOAT)height, 0.01f, 100.0f);
	}

	return InitResource();
}

HRESULT InitResource() {
	HRESULT hr = S_OK;

	{
		// Create vertex buffer
		SimpleVertex vertices[] =
		{
			{ XMFLOAT3(-0.5f, 0.5f, -0.5f), XMFLOAT4(0.0f, 0.0f, 0.5f, 0.5f) },
		{ XMFLOAT3(0.5f, 0.5f, -0.5f), XMFLOAT4(0.0f, 0.5f, 0.0f, 0.5f) },
		{ XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT4(0.0f, 0.5f, 0.5f, 0.5f) },
		{ XMFLOAT3(-0.5f, 0.5f, 0.5f), XMFLOAT4(0.5f, 0.0f, 0.0f, 0.5f) },
		{ XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT4(0.5f, 0.0f, 0.5f, 0.5f) },
		{ XMFLOAT3(0.5f, -0.5f, -0.5f), XMFLOAT4(0.5f, 0.5f, 0.0f, 0.5f) },
		{ XMFLOAT3(0.5f, -0.5f, 0.5f), XMFLOAT4(0.5f, 0.5f, 0.5f, 0.5f) },
		{ XMFLOAT3(-0.5f, -0.5f, 0.5f), XMFLOAT4(0.0f, 0.0f, 0.0f, 0.5f) },
		};
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(SimpleVertex) * 8;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;

		D3D11_SUBRESOURCE_DATA InitData = {};
		InitData.pSysMem = vertices;
		hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, g_pVertexBuffer.ReleaseAndGetAddressOf());
		if (FAILED(hr))
			return hr;

		// Create instance buffer
		g_InstanceCount = 6;
		std::unique_ptr<InstanceType[]> instances = std::make_unique<InstanceType[]>(g_InstanceCount);
		instances.get()[0] = { { -1.0f, 0.0f, 0.0f } };
		instances.get()[1] = { { 0.0f, -1.0f, 0.0f } };
		instances.get()[2] = { { 0.0f, 0.0f, -1.0f } };
		instances.get()[3] = { { 1.0f, 0.0f, 0.0f } };
		instances.get()[4] = { { 0.0f, 1.0f, 0.0f } };
		instances.get()[5] = { { 0.0f, 0.0f, 1.0f } };
		bd.ByteWidth = sizeof(InstanceType) * g_InstanceCount;
		InitData.pSysMem = instances.get();
		hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, g_pInstanceBuffer.ReleaseAndGetAddressOf());


		// Create index buffer
		WORD indices[] =
		{
			3,1,0,
			2,1,3,

			0,5,4,
			1,5,0,

			3,4,7,
			0,4,3,

			1,6,5,
			2,6,1,

			2,7,6,
			3,7,2,

			6,4,5,
			7,4,6,
		};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(WORD) * 36;        // 36 vertices needed for 12 triangles in a triangle list
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bd.CPUAccessFlags = 0;
		InitData.pSysMem = indices;
		hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, g_pIndexBuffer.ReleaseAndGetAddressOf());
		if (FAILED(hr))
			return hr;

		// Create the constant buffer
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(ConstantBuffer);
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = 0;
		hr = g_pd3dDevice->CreateBuffer(&bd, nullptr, g_pConstantBuffer.ReleaseAndGetAddressOf());
	}
	return hr;
}

//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------
void CleanupDevice()
{
	if (g_pImmediateContext) g_pImmediateContext->ClearState();
	g_pConstantBuffer.Reset();
	g_pVertexBuffer.Reset();
	g_pIndexBuffer.Reset();
	//g_pVertexLayout.Reset();
	//g_pVertexShader.Reset();
	g_pPixelShader.Reset();
	g_pRenderTargetView.Reset();
	g_pInstanceBuffer.Reset();
	g_pSwapChain.Reset();
	g_pImmediateContext.Reset();

	//if (g_pConstantBuffer) g_pConstantBuffer->Release();
	//if (g_pVertexBuffer) g_pVertexBuffer->Release();
	//if (g_pIndexBuffer) g_pIndexBuffer->Release();
	//if (g_pVertexLayout) g_pVertexLayout->Release();
	//if (g_pVertexShader) g_pVertexShader->Release();
	//if (g_pPixelShader) g_pPixelShader->Release();
	//if (g_pRenderTargetView) g_pRenderTargetView->Release();
	//if (g_pSwapChain) g_pSwapChain->Release();
	//if (g_pImmediateContext) g_pImmediateContext->Release();
	//if (g_pd3dDevice) g_pd3dDevice->Release();
	//if (g_pInstanceBuffer) g_pInstanceBuffer->Release();
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
		//g_pd3dDevice->Check
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

void Update() {
	// Update our time
	static float t = 0.0f;
	if (g_driverType == D3D_DRIVER_TYPE_REFERENCE)
	{
		t += (float)XM_PI * 0.0125f;
	}
	else
	{
		static ULONGLONG timeStart = 0;
		ULONGLONG timeCur = GetTickCount64();
		if (timeStart == 0)
			timeStart = timeCur;
		t = (timeCur - timeStart) / 1000.0f;
	}

	//
	// Animate the cube
	//
	g_World = XMMatrixRotationY(t);
	g_World *= XMMatrixRotationX(-t);
	//
	// Update variables
	//
	ConstantBuffer cb;
	cb.mWorld = XMMatrixTranspose(g_World);
	cb.mView = XMMatrixTranspose(g_View);
	cb.mProjection = XMMatrixTranspose(g_Projection);
	g_pImmediateContext->UpdateSubresource(g_pConstantBuffer.Get(), 0, nullptr, &cb, 0, 0);
}

//--------------------------------------------------------------------------------------
// Render a frame
//--------------------------------------------------------------------------------------
void Render()
{
	//
	// Clear the back buffer
	//
	g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView.Get(), Colors::MidnightBlue);

	// Set the input layout
	g_pImmediateContext->IASetInputLayout(g_pVertexLayout.Get());

	// Set vertex buffer
	UINT stride[2] = { sizeof(SimpleVertex), sizeof(InstanceType) };
	UINT offset[2] = { 0,0 };
	ID3D11Buffer* vbs[2];
	vbs[0] = g_pVertexBuffer.Get();
	vbs[1] = g_pInstanceBuffer.Get();
	g_pImmediateContext->IASetVertexBuffers(0, 2, vbs, stride, offset);

	// Set index buffer
	g_pImmediateContext->IASetIndexBuffer(g_pIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

	// Set primitive topology
	g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//
	// Renders a triangle
	//
	g_pImmediateContext->VSSetShader(g_pVertexShader.Get(), nullptr, 0);
	g_pImmediateContext->VSSetConstantBuffers(0, 1, g_pConstantBuffer.GetAddressOf());
	g_pImmediateContext->PSSetShader(g_pPixelShader.Get(), nullptr, 0);
	//g_pImmediateContext->DrawIndexed( 36, 0, 0 );        // 36 vertices needed for 12 triangles in a triangle list
	g_pImmediateContext->DrawIndexedInstanced(36, g_InstanceCount, 0, 0, 0);
	//
	// Present our back buffer to our front buffer
	//
	HRESULT hr = g_pSwapChain->Present(0, 0);
	if (FAILED(hr)) {
		CleanupDevice();
		InitResource();
	}
}

