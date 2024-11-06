#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxmath.h>
using namespace DirectX;

#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "d3dcompiler.lib")

IDXGISwapChain* swapchain;
ID3D11Device* dev;
ID3D11DeviceContext* devcon;
ID3D11RenderTargetView* backbuffer;
ID3D11InputLayout* pLayout;
ID3D11VertexShader* pVS;
ID3D11PixelShader* pPS;
ID3D11Buffer* pVBuffer;
ID3D11Buffer* pConstantBuffer;
ID3D11DepthStencilView* depthStencilView;

void InitD3D(HWND hWnd);
void RenderFrame(void);
void CleanD3D(void);
void InitGraphics(void);
void InitPipeline(void);

struct VERTEX {
    FLOAT X, Y, Z;
    XMFLOAT4 Color;
    XMFLOAT3 Normal;
    XMFLOAT2 TexCoord;
};

struct CONSTANT_BUFFER
{
    XMMATRIX WorldViewProj;
};

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    HWND hWnd;
    WNDCLASSEX wc;

    ZeroMemory(&wc, sizeof(WNDCLASSEX));

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"WindowClass";

    RegisterClassEx(&wc);

    hWnd = CreateWindowEx(NULL,
        L"WindowClass",
        L"doom style game engine attempt",
        WS_OVERLAPPEDWINDOW,
        300, 300,
        800, 600,
        NULL,
        NULL,
        hInstance,
        NULL);

    ShowWindow(hWnd, nCmdShow);

    InitD3D(hWnd);

    MSG msg = { 0 };

    while (TRUE)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            if (msg.message == WM_QUIT)
                break;
        }

        RenderFrame();
    }

    CleanD3D();

    return msg.wParam;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

void InitD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC scd;

    ZeroMemory(&scd, sizeof(DXGI_SWAP_CHAIN_DESC));

    scd.BufferCount = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hWnd;
    scd.SampleDesc.Count = 4;
    scd.Windowed = TRUE;
    scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    D3D11CreateDeviceAndSwapChain(NULL,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        NULL,
        NULL,
        NULL,
        D3D11_SDK_VERSION,
        &scd,
        &swapchain,
        &dev,
        NULL,
        &devcon);

    // Create depth stencil buffer
    D3D11_TEXTURE2D_DESC depthStencilDesc;
    depthStencilDesc.Width = 800;
    depthStencilDesc.Height = 600;
    depthStencilDesc.MipLevels = 1;
    depthStencilDesc.ArraySize = 1;
    depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthStencilDesc.SampleDesc.Count = 4;
    depthStencilDesc.SampleDesc.Quality = 0;
    depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
    depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    depthStencilDesc.CPUAccessFlags = 0;
    depthStencilDesc.MiscFlags = 0;

    ID3D11Texture2D* depthStencilBuffer;
    dev->CreateTexture2D(&depthStencilDesc, NULL, &depthStencilBuffer);

    D3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
    ZeroMemory(&depthStencilViewDesc, sizeof(depthStencilViewDesc));
    depthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthStencilViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    depthStencilViewDesc.Texture2D.MipSlice = 0;

    ID3D11DepthStencilView* depthStencilView;
    dev->CreateDepthStencilView(depthStencilBuffer, &depthStencilViewDesc, &depthStencilView);
    depthStencilBuffer->Release();

    // Set render target
    ID3D11Texture2D* pBackBuffer;
    swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    dev->CreateRenderTargetView(pBackBuffer, NULL, &backbuffer);
    pBackBuffer->Release();
    devcon->OMSetRenderTargets(1, &backbuffer, depthStencilView);

    D3D11_VIEWPORT viewport;
    ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));

    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = 800;
    viewport.Height = 600;

    devcon->RSSetViewports(1, &viewport);

    InitPipeline();
    InitGraphics();

    // Create constant buffer
    D3D11_BUFFER_DESC cbd;
    ZeroMemory(&cbd, sizeof(cbd));
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(CONSTANT_BUFFER);
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = 0;

    dev->CreateBuffer(&cbd, NULL, &pConstantBuffer);
    devcon->VSSetConstantBuffers(0, 1, &pConstantBuffer);
}

void RenderFrame(void)
{
    const float clearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };
    devcon->ClearRenderTargetView(backbuffer, clearColor);
    devcon->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0); // Ensure depth buffer is cleared

    static float angle = 0.0f;
    angle += 0.00005f;

    XMMATRIX matRotate = XMMatrixRotationY(angle);
    XMMATRIX matView = XMMatrixLookAtLH(XMVectorSet(0.0f, 2.0f, -5.0f, 0.0f), XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    XMMATRIX matProj = XMMatrixPerspectiveFovLH(XM_PIDIV4, 800.0f / 600.0f, 0.01f, 100.0f);
    XMMATRIX matFinal = matRotate * matView * matProj;

    CONSTANT_BUFFER cb;
    cb.WorldViewProj = XMMatrixTranspose(matFinal);

    devcon->UpdateSubresource(pConstantBuffer, 0, NULL, &cb, 0, 0);

    devcon->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    devcon->DrawIndexed(36, 0, 0);

    swapchain->Present(0, 0);
}



void CleanD3D(void)
{
    swapchain->SetFullscreenState(FALSE, NULL);

    pLayout->Release();
    pVS->Release();
    pPS->Release();
    pVBuffer->Release();
    pConstantBuffer->Release();
    swapchain->Release();
    backbuffer->Release();
    dev->Release();
    devcon->Release();
}

void InitGraphics()
{
    VERTEX vertices[] =
    {
        // Front face
        { -1.0f,  1.0f, -1.0f, XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f), XMFLOAT3(0, 0, -1), XMFLOAT2(0, 0) },
        {  1.0f,  1.0f, -1.0f, XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f), XMFLOAT3(0, 0, -1), XMFLOAT2(1, 0) },
        {  1.0f, -1.0f, -1.0f, XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f), XMFLOAT3(0, 0, -1), XMFLOAT2(1, 1) },
        { -1.0f, -1.0f, -1.0f, XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT3(0, 0, -1), XMFLOAT2(0, 1) },

        // Back face
        { -1.0f,  1.0f,  1.0f, XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f), XMFLOAT3(0, 0, 1), XMFLOAT2(0, 0) },
        {  1.0f,  1.0f,  1.0f, XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f), XMFLOAT3(0, 0, 1), XMFLOAT2(1, 0) },
        {  1.0f, -1.0f,  1.0f, XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f), XMFLOAT3(0, 0, 1), XMFLOAT2(1, 1) },
        { -1.0f, -1.0f,  1.0f, XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT3(0, 0, 1), XMFLOAT2(0, 1) },

        // Top face
        { -1.0f,  1.0f, -1.0f, XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f), XMFLOAT3(0, 1, 0), XMFLOAT2(0, 0) },
        {  1.0f,  1.0f, -1.0f, XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f), XMFLOAT3(0, 1, 0), XMFLOAT2(1, 0) },
        {  1.0f,  1.0f,  1.0f, XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f), XMFLOAT3(0, 1, 0), XMFLOAT2(1, 1) },
        { -1.0f,  1.0f,  1.0f, XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT3(0, 1, 0), XMFLOAT2(0, 1) },

        // Bottom face
        { -1.0f, -1.0f, -1.0f, XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f), XMFLOAT3(0, -1, 0), XMFLOAT2(0, 0) },
        {  1.0f, -1.0f, -1.0f, XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f), XMFLOAT3(0, -1, 0), XMFLOAT2(1, 0) },
        {  1.0f, -1.0f,  1.0f, XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f), XMFLOAT3(0, -1, 0), XMFLOAT2(1, 1) },
        { -1.0f, -1.0f,  1.0f, XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT3(0, -1, 0), XMFLOAT2(0, 1) },

        // Left face
        { -1.0f,  1.0f,  1.0f, XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f), XMFLOAT3(-1, 0, 0), XMFLOAT2(0, 0) },
        { -1.0f,  1.0f, -1.0f, XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f), XMFLOAT3(-1, 0, 0), XMFLOAT2(1, 0) },
        { -1.0f, -1.0f, -1.0f, XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f), XMFLOAT3(-1, 0, 0), XMFLOAT2(1, 1) },
        { -1.0f, -1.0f,  1.0f, XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT3(-1, 0, 0), XMFLOAT2(0, 1) },

        // Right face
        { 1.0f,  1.0f,  1.0f, XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f), XMFLOAT3(1, 0, 0), XMFLOAT2(0, 0) },
        { 1.0f,  1.0f, -1.0f, XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f), XMFLOAT3(1, 0, 0), XMFLOAT2(1, 0) },
        { 1.0f, -1.0f, -1.0f, XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f), XMFLOAT3(1, 0, 0), XMFLOAT2(1, 1) },
        { 1.0f, -1.0f,  1.0f, XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f), XMFLOAT3(1, 0, 0), XMFLOAT2(0, 1) },
    };



    DWORD indices[] = {
        // Front face
        0, 2, 1, 2, 0, 3,
        // Back face
        4, 6, 5, 6, 4, 7,
        // Top face
        8, 10, 9, 10, 8, 11,
        // Bottom face
        12, 14, 13, 14, 12, 15,
        // Left face
        16, 18, 17, 18, 16, 19,
        // Right face
        20, 22, 21, 22, 20, 23,
    };






    // Create Vertex Buffer
    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(bd));

    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(vertices);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    dev->CreateBuffer(&bd, NULL, &pVBuffer);

    D3D11_MAPPED_SUBRESOURCE ms;
    devcon->Map(pVBuffer, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms);
    memcpy(ms.pData, vertices, sizeof(vertices));
    devcon->Unmap(pVBuffer, NULL);

    UINT stride = sizeof(VERTEX);
    UINT offset = 0;
    devcon->IASetVertexBuffers(0, 1, &pVBuffer, &stride, &offset);

    // Create Index Buffer
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(indices);
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    ID3D11Buffer* pIBuffer;
    dev->CreateBuffer(&bd, NULL, &pIBuffer);

    devcon->Map(pIBuffer, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms);
    memcpy(ms.pData, indices, sizeof(indices));
    devcon->Unmap(pIBuffer, NULL);

    devcon->IASetIndexBuffer(pIBuffer, DXGI_FORMAT_R32_UINT, 0);
}

void InitPipeline()
{
    ID3D10Blob* VS, * PS;
    D3DCompileFromFile(L"shaders.hlsl", 0, 0, "VShader", "vs_4_0", 0, 0, &VS, 0);
    D3DCompileFromFile(L"shaders.hlsl", 0, 0, "PShader", "ps_4_0", 0, 0, &PS, 0);

    dev->CreateVertexShader(VS->GetBufferPointer(), VS->GetBufferSize(), NULL, &pVS);
    dev->CreatePixelShader(PS->GetBufferPointer(), PS->GetBufferSize(), NULL, &pPS);

    devcon->VSSetShader(pVS, 0, 0);
    devcon->PSSetShader(pPS, 0, 0);

    D3D11_INPUT_ELEMENT_DESC ied[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    dev->CreateInputLayout(ied, ARRAYSIZE(ied), VS->GetBufferPointer(), VS->GetBufferSize(), &pLayout);
    devcon->IASetInputLayout(pLayout);

    VS->Release();
    PS->Release();

    // Create rasterizer state for wireframe mode
    D3D11_RASTERIZER_DESC rd;
    ZeroMemory(&rd, sizeof(rd));
    rd.FillMode = D3D11_FILL_WIREFRAME;
    rd.CullMode = D3D11_CULL_NONE;

    ID3D11RasterizerState* rasterState;
    dev->CreateRasterizerState(&rd, &rasterState);
    devcon->RSSetState(rasterState);
}


