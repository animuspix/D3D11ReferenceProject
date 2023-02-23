#include "D3DWrapper.h"
#include "Memory.h"
#include <cassert>
#include <wrl/client.h>

#include <iostream>
#include <fstream>
#include <filesystem>

template<typename T>
struct ComPtr : public Microsoft::WRL::ComPtr<T> {};

// Big huge slotmap(?) of generic resources
// Assumes we won't have more than 256 resources bound at once
template<typename D3DResrcType>
struct ResrcGeneric
{
	ComPtr<D3DResrcType> resrc;
	ComPtr<ID3D11ShaderResourceView> srv;
	ComPtr<ID3D11UnorderedAccessView> uav;
	ComPtr<ID3D11RenderTargetView> rtv;
	ComPtr<ID3D11DepthStencilView> dsv;

	void Reset()
	{
		resrc.Reset();
		srv.Reset();
		uav.Reset();
		rtv.Reset();
		dsv.Reset();
	}

	// Pretty sure I need more context here...will find out as I go
};

constexpr uint32_t numResrcSlots = 64;
ResrcGeneric<ID3D11Texture2D> textures[numResrcSlots] = {};
ResrcGeneric<ID3D11Buffer> buffers[numResrcSlots] = {};
ResrcGeneric<ID3D11Texture3D> volumes[numResrcSlots] = {};

uint32_t nextTextureSlot = 0;
uint32_t nextBufferSlot = 0;
uint32_t nextVolumeSlot = 0;

constexpr uint32_t numShaderSlots = 16;
ComPtr<ID3D11VertexShader> vtShaders[numShaderSlots] = {};
ComPtr<ID3D11PixelShader> pxShaders[numShaderSlots] = {};
ComPtr<ID3D11ComputeShader> computeShaders[numShaderSlots] = {};

uint32_t nextVtShaderSlot = 0;
uint32_t nextPxShaderSlot = 0;
uint32_t nextComputeShaderSlot = 0;

ComPtr<ID3D11Device> device;
ComPtr<ID3D11DeviceContext> context;
ComPtr<IDXGISwapChain> swapchain;
ComPtr<ID3D11RenderTargetView> backBufView;
D3DHandle starterDepthBuffer; // Basic rendering should have at least one depth buffer bound, so create this one on startup (obvi users can yeet it and replace with their own, so long as they have a way to bind it
							  // along with the back-buffer RTV in the last draw)

ComPtr<ID3D11InputLayout> ilayout3D;
ComPtr<ID3D11InputLayout> ilayout2D;

// Standard input element layouts, matching the standard vertex formats in D3DUtils.h
// We aren't using multiple vertex slots, and we aren't using API instancing, so we can ignore those fields here
D3D11_INPUT_ELEMENT_DESC vertex_inputs[3] =
{
  { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
  { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
  { "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

D3D11_INPUT_ELEMENT_DESC vertex_inputs_2D[3] =
{
  { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

bool using_vsync = false;

void D3DWrapper::Init(HWND hwnd, uint32_t window_width, uint32_t window_height, bool vsync)
{
	// Describe the swap-chain we want to create
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};

	swapChainDesc.BufferDesc.Width = window_width;
	swapChainDesc.BufferDesc.Height = window_height;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	// Refresh rate intentionally unspecified so that hopefully it just goes flat-out
	// (and we decide/or not to lock it down with vsync on present)
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 1;
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 60;

	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_STRETCHED;
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE; // The p in 1080p stands for progressive! In video this is the difference between showing everything at once
																					  // (even & odd scanlines) vs interlaced (even, then ~8ms later, odd). I'm not sure what it means here, but progressive is probably
																					  // what we want
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0; // No display multisampling (for now, maybe later)

	swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER | DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2; // Double-buffering, good enough for us ^^

	swapChainDesc.OutputWindow = hwnd;
	swapChainDesc.Windowed = TRUE; // Fullscreen debugging sucks

	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // Not sure if this will work on FL11.0, hopefully it does
	swapChainDesc.Flags = vsync ? 0 : DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING; // Allow tearing if vsync inactive

	// Enumerate adapters, then acquire device, context, debug-device, swapchain
	ComPtr<IDXGIFactory> dxgiFactory;
	dxgiFactory.Reset();
	CreateDXGIFactory(__uuidof(IDXGIFactory), &dxgiFactory);

	HRESULT hr;
	bool adapterFound = false;
	Microsoft::WRL::ComPtr<IDXGIAdapter> gpuHW = nullptr;

	for (UINT adapterNdx = 0;
		dxgiFactory->EnumAdapters(adapterNdx, &gpuHW) != DXGI_ERROR_NOT_FOUND;
		adapterNdx += 1) // Iterate over all available adapters
	{
		DXGI_ADAPTER_DESC tmpGPUInfo;
		hr = gpuHW->GetDesc(&tmpGPUInfo); // Get descriptions for iterated adapters

		// Keep iterating adapters until we successfully create a D3D11 device & swapchain
		//////////////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
		uint32_t deviceFlags = D3D11_CREATE_DEVICE_DEBUG;
#else
		uint32_t deviceFlags = 0;
#endif

		D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL::D3D_FEATURE_LEVEL_11_0;
		hr = D3D11CreateDeviceAndSwapChain(gpuHW.Get(), D3D_DRIVER_TYPE_UNKNOWN, NULL, deviceFlags, &featureLevel, 1, D3D11_SDK_VERSION, &swapChainDesc, &swapchain, &device, NULL, &context);
		if (SUCCEEDED(hr))
		{
			adapterFound = true;
			break;
		}
	}

	assert(adapterFound);

	// Resolve an RTV of the back-buffer so we can render to it directly
	ComPtr<ID3D11Texture2D> backBufTx;
	swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBufTx);
	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	hr = device->CreateRenderTargetView(backBufTx.Get(), &rtvDesc, &backBufView);
	assert(SUCCEEDED(hr));

	// Give users a depth-buffer for initial rendering (so nothing gets clipped unexpectedly/the debug layer doesn't get angry with us)
	starterDepthBuffer = D3DWrapper::CreateTexture(window_width, window_height, DXGI_FORMAT_D16_UNORM, RESRC_ACCESS_TYPES::GPU_ONLY, RESRC_VIEWS::DEPTH_STENCIL, nullptr, 2 * window_width * window_height);

	using_vsync = vsync;
}

void D3DWrapper::DeInit()
{
	for (uint32_t i = 0; i < numResrcSlots; i++)
	{
		textures[i].Reset();
		buffers[i].Reset();
		volumes[i].Reset();
	}

	for (uint32_t i = 0; i < numShaderSlots; i++)
	{
		vtShaders[i].Reset();
		pxShaders[i].Reset();
		computeShaders[i].Reset();
	}

	swapchain.Reset();
	context.Reset();
	device.Reset();
}

UINT ResolveTextureBindFlags(RESRC_VIEWS composed_views)
{
	UINT ret = 0;
	if (composed_views & RESRC_VIEWS::UNORDERED_GPU_WRITES) ret |= D3D11_BIND_UNORDERED_ACCESS;
	if (composed_views & RESRC_VIEWS::GENERIC_READONLY) ret |= D3D11_BIND_SHADER_RESOURCE;
	if (composed_views & RESRC_VIEWS::RENDER_TARGET) ret |= D3D11_BIND_RENDER_TARGET;
	if (composed_views & RESRC_VIEWS::DEPTH_STENCIL) ret |= D3D11_BIND_DEPTH_STENCIL;
	return ret;
}

UINT ResolveVolumeBindFlags(RESRC_VIEWS composed_views)
{
	UINT ret = 0;
	if (composed_views & RESRC_VIEWS::UNORDERED_GPU_WRITES) ret |= D3D11_BIND_UNORDERED_ACCESS;
	if (composed_views & RESRC_VIEWS::GENERIC_READONLY) ret |= D3D11_BIND_SHADER_RESOURCE;
	return ret;
}

D3D11_USAGE ResolveUsage(RESRC_ACCESS_TYPES access, RESRC_VIEWS composed_views)
{
	if (access == RESRC_ACCESS_TYPES::STAGING)
	{
		return D3D11_USAGE_STAGING;
	}
	if (access == RESRC_ACCESS_TYPES::CPU_WRITE)
	{
		return D3D11_USAGE_DYNAMIC;
	}
	if (access == RESRC_ACCESS_TYPES::GPU_ONLY)
	{
		if (composed_views & RESRC_VIEWS::GENERIC_READONLY)
		{
			return D3D11_USAGE_IMMUTABLE;
		}
		return D3D11_USAGE_DEFAULT;
	}
	else
	{
		assert(false); // Shouldn't be able to get here
		return D3D11_USAGE_DEFAULT;
	}
}

UINT ResolveCPUAccess(RESRC_ACCESS_TYPES access)
{
	if (access == RESRC_ACCESS_TYPES::STAGING)
	{
		return D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
	}
	else if (access == RESRC_ACCESS_TYPES::CPU_WRITE)
	{
		return D3D11_CPU_ACCESS_WRITE;
	}
	else// if (access == RESRC_ACCESS_TYPES::GPU_ONLY)
	{
		return NULL;
	}
}

D3DHandle D3DWrapper::CreateTexture(uint32_t width, uint32_t height, DXGI_FORMAT format, RESRC_ACCESS_TYPES access, RESRC_VIEWS composed_views, void* init_data, uint32_t data_footprint_bytes)
{
	// Layout texture creation flags
	D3D11_TEXTURE2D_DESC desc;
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = desc.ArraySize = 1; // No mips or texture arrays in our engine (yet)
	desc.Format = format;
	desc.SampleDesc.Count = 1; // No multisampling either (may change idk)
	desc.SampleDesc.Quality = 0;

	desc.Usage = ResolveUsage(access, composed_views);
	desc.BindFlags = ResolveTextureBindFlags(composed_views);
	desc.CPUAccessFlags = ResolveCPUAccess(access);
	desc.MiscFlags = NULL; // May use GENERATE_MIPS or GDI_COMPATIBLE at some point in the future, but not right now

	// Populate initial data (ignored if init_data is nullptr)
	D3D11_SUBRESOURCE_DATA subresrc;
	subresrc.pSysMem = init_data;
	subresrc.SysMemPitch = data_footprint_bytes / height;
	subresrc.SysMemSlicePitch = NULL;

	// Create texture
	ResrcGeneric<ID3D11Texture2D>& texture = textures[nextTextureSlot];
	HRESULT hr = device->CreateTexture2D(&desc, init_data != nullptr ? &subresrc : nullptr, texture.resrc.ReleaseAndGetAddressOf());
	assert(SUCCEEDED(hr));

	// Create requested views
	if (composed_views & RESRC_VIEWS::UNORDERED_GPU_WRITES)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Texture2D.MipSlice = 0; // Mipmap index for this view - we don't have mips at the moment, so this is always zero
		uavDesc.Format = format;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		hr = device->CreateUnorderedAccessView(texture.resrc.Get(), &uavDesc, texture.uav.ReleaseAndGetAddressOf());
		assert(SUCCEEDED(hr));
	}

	if (composed_views & RESRC_VIEWS::GENERIC_READONLY)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.MostDetailedMip = 0; // Another index - again, we don't have mips, so keep this at zero
		srvDesc.Format = format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		hr = device->CreateShaderResourceView(texture.resrc.Get(), &srvDesc, texture.srv.ReleaseAndGetAddressOf());
		assert(SUCCEEDED(hr));
	}

	if (composed_views & RESRC_VIEWS::RENDER_TARGET)
	{
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Texture2D.MipSlice = 0;
		rtvDesc.Format = format;
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		hr = device->CreateRenderTargetView(texture.resrc.Get(), &rtvDesc, texture.rtv.ReleaseAndGetAddressOf());
		assert(SUCCEEDED(hr));
	}

	if (composed_views & RESRC_VIEWS::DEPTH_STENCIL)
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Texture2D.MipSlice = 0;
		dsvDesc.Format = format;
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		hr = device->CreateDepthStencilView(texture.resrc.Get(), &dsvDesc, texture.dsv.ReleaseAndGetAddressOf());
		assert(SUCCEEDED(hr));
	}

	D3DHandle handle = {};
	handle.index = nextTextureSlot;
	handle.objType = D3D_OBJ_TYPES::TEXTURE;
	nextTextureSlot++;
	return handle;
}

UINT ResolveBufferBindFlags(RESRC_VIEWS composed_views)
{
	UINT ret = 0;
	if (composed_views & RESRC_VIEWS::UNORDERED_GPU_WRITES) ret |= D3D11_BIND_UNORDERED_ACCESS;
	if (composed_views & RESRC_VIEWS::GENERIC_READONLY) ret |= D3D11_BIND_SHADER_RESOURCE;
	if (composed_views & RESRC_VIEWS::CONSTANT_BUFFER) ret |= D3D11_BIND_CONSTANT_BUFFER;
	if (composed_views & RESRC_VIEWS::VERTEX) ret |= D3D11_BIND_VERTEX_BUFFER;
	if (composed_views & RESRC_VIEWS::INDEX) ret |= D3D11_BIND_INDEX_BUFFER;
	return ret;
}

D3DHandle D3DWrapper::CreateBuffer(uint32_t num_elements, DXGI_FORMAT format, RESRC_ACCESS_TYPES access, RESRC_VIEWS composed_views, bool structured, void* init_data, uint32_t data_footprint_bytes)
{
	// Layout texture creation flags
	D3D11_BUFFER_DESC desc;
	desc.ByteWidth = data_footprint_bytes;
	desc.StructureByteStride = data_footprint_bytes / num_elements;

	desc.Usage = ResolveUsage(access, composed_views);
	desc.BindFlags = ResolveBufferBindFlags(composed_views);
	desc.CPUAccessFlags = ResolveCPUAccess(access);
	desc.MiscFlags = structured ? D3D11_RESOURCE_MISC_BUFFER_STRUCTURED : NULL; // May use DRAWINDIRECT_ARGS at some point in the future, but not right now

	// Populate initial data (ignored if init_data is nullptr)
	D3D11_SUBRESOURCE_DATA subresrc;
	subresrc.pSysMem = init_data;
	subresrc.SysMemPitch = data_footprint_bytes;
	subresrc.SysMemSlicePitch = NULL;

	// Create texture
	ResrcGeneric<ID3D11Buffer>& buffer = buffers[nextBufferSlot];
	HRESULT hr = device->CreateBuffer(&desc, init_data != nullptr ? &subresrc : nullptr, buffer.resrc.ReleaseAndGetAddressOf());
	assert(SUCCEEDED(hr));

	// Create requested views
	if (composed_views & RESRC_VIEWS::UNORDERED_GPU_WRITES)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Buffer.FirstElement = 0; // No sentinel data in our engine (atm)
		uavDesc.Buffer.NumElements = num_elements;
		uavDesc.Buffer.Flags = NULL; // We aren't using counters or append/consume buffers (for now)

		uavDesc.Format = format;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		hr = device->CreateUnorderedAccessView(buffer.resrc.Get(), &uavDesc, buffer.uav.ReleaseAndGetAddressOf());
		assert(SUCCEEDED(hr));
	}

	if (composed_views & RESRC_VIEWS::GENERIC_READONLY)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Buffer.FirstElement = 0; // No weird sentinels here either
		srvDesc.Buffer.NumElements = num_elements;

		srvDesc.Format = format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		hr = device->CreateShaderResourceView(buffer.resrc.Get(), &srvDesc, buffer.srv.ReleaseAndGetAddressOf());
		assert(SUCCEEDED(hr));
	}

	// Constant buffers, vertex buffers, and index buffers just work(tm) without a view
	// (totally not janky and inconsistent at all nope)

	D3DHandle handle = {};
	handle.index = nextBufferSlot;
	handle.objType = D3D_OBJ_TYPES::BUFFER;
	nextBufferSlot++;
	return handle;
}

D3DHandle D3DWrapper::CreateVolume(uint32_t width, uint32_t height, uint32_t depth, DXGI_FORMAT format, RESRC_ACCESS_TYPES access, RESRC_VIEWS composed_views, void* init_data, uint32_t data_footprint_bytes)
{
	// Layout texture creation flags
	D3D11_TEXTURE3D_DESC desc;
	desc.Width = width;
	desc.Height = height;
	desc.Depth = depth;
	desc.Format = format;

	desc.Usage = ResolveUsage(access, composed_views);
	desc.BindFlags = ResolveTextureBindFlags(composed_views);
	desc.CPUAccessFlags = ResolveCPUAccess(access);
	desc.MiscFlags = NULL; // May use GENERATE_MIPS or GDI_COMPATIBLE at some point in the future, but not right now

	// Populate initial data (ignored if init_data is nullptr)
	D3D11_SUBRESOURCE_DATA subresrc;
	subresrc.pSysMem = init_data;
	subresrc.SysMemPitch = data_footprint_bytes / (depth * height);
	subresrc.SysMemSlicePitch = data_footprint_bytes / depth;

	// Create texture
	ResrcGeneric<ID3D11Texture3D>& volume = volumes[nextTextureSlot];
	HRESULT hr = device->CreateTexture3D(&desc, init_data != nullptr ? &subresrc : nullptr, volume.resrc.ReleaseAndGetAddressOf());
	assert(SUCCEEDED(hr));

	// Create requested views
	if (composed_views & RESRC_VIEWS::UNORDERED_GPU_WRITES)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Texture2D.MipSlice = 0; // Mipmap index for this view - we don't have mips at the moment, so this is always zero
		uavDesc.Format = format;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
		hr = device->CreateUnorderedAccessView(volume.resrc.Get(), &uavDesc, volume.uav.ReleaseAndGetAddressOf());
		assert(SUCCEEDED(hr));
	}

	if (composed_views & RESRC_VIEWS::GENERIC_READONLY)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.MostDetailedMip = 0; // Another index - again, we don't have mips, so keep this at zero
		srvDesc.Format = format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		hr = device->CreateShaderResourceView(volume.resrc.Get(), &srvDesc, volume.srv.ReleaseAndGetAddressOf());
		assert(SUCCEEDED(hr));
	}

	D3DHandle handle = {};
	handle.index = nextVolumeSlot;
	handle.objType = D3D_OBJ_TYPES::VOLUME;
	nextVolumeSlot++;
	return handle;
}

void D3DWrapper::ClearResrc(D3DHandle handle)
{
	if (handle.objType == D3D_OBJ_TYPES::TEXTURE)
	{
		textures[handle.index].Reset();
	}

	if (handle.objType == D3D_OBJ_TYPES::TEXTURE)
	{
		textures[handle.index].Reset();
	}

	if (handle.objType == D3D_OBJ_TYPES::TEXTURE)
	{
		textures[handle.index].Reset();
	}
}

bool resolved3DInputs = false;
bool resolved2DInputs = false;

struct ShaderBuilder
{
	uint8_t* data = nullptr;
	uint64_t size = 0;
	ShaderBuilder(const char* path, SHADER_TYPES type)
	{
		// Resolve shader size, allocate temp storage
		size = std::filesystem::file_size(path);
		data = (uint8_t*)malloc(size);

		// Open a stream to the shader & copy it into temp storage
		std::fstream strm(path);
		strm >> data;

		// Shader creation
		if (type == SHADER_TYPES::VS)
		{
			HRESULT hr = device->CreateVertexShader(data, size, nullptr, vtShaders[nextVtShaderSlot].ReleaseAndGetAddressOf());
			assert(SUCCEEDED(hr));
		}
		else if (type == SHADER_TYPES::PS)
		{
			HRESULT hr = device->CreatePixelShader(data, size, nullptr, pxShaders[nextPxShaderSlot].ReleaseAndGetAddressOf());
			assert(SUCCEEDED(hr));
		}
		else if (type == SHADER_TYPES::CS)
		{
			HRESULT hr = device->CreateComputeShader(data, size, nullptr, computeShaders[nextComputeShaderSlot].ReleaseAndGetAddressOf());
			assert(SUCCEEDED(hr));
		}
	}

	~ShaderBuilder()
	{
		free(data);
	}
};

D3DHandle D3DWrapper::CreateVertShader(const char* path, bool is2D)
{
	ShaderBuilder vs(path, SHADER_TYPES::VS);

	// Construct input layouts
	//////////////////////////

	if (!resolved3DInputs && !is2D)
	{
		HRESULT hr = device->CreateInputLayout(vertex_inputs, 3, vs.data, vs.size, ilayout3D.ReleaseAndGetAddressOf());
		assert(SUCCEEDED(hr));

		resolved3DInputs = true;
	}

	if (!resolved2DInputs && is2D)
	{
		HRESULT hr = device->CreateInputLayout(vertex_inputs_2D, 1, vs.data, vs.size, ilayout2D.ReleaseAndGetAddressOf());
		assert(SUCCEEDED(hr));

		resolved2DInputs = true;
	}

	D3DHandle handle = {};
	handle.index = nextVtShaderSlot;
	handle.objType = D3D_OBJ_TYPES::VERTEX_SHADER;
	nextVtShaderSlot++;
	return handle;
}

D3DHandle D3DWrapper::CreatePixelShader(const char* path)
{
	ShaderBuilder ps(path, SHADER_TYPES::PS);

	D3DHandle handle = {};
	handle.index = nextPxShaderSlot;
	handle.objType = D3D_OBJ_TYPES::PIXEL_SHADER;
	nextPxShaderSlot++;
	return handle;
}

D3DHandle D3DWrapper::CreateComputeShader(const char* path)
{
	ShaderBuilder cs(path, SHADER_TYPES::CS);

	D3DHandle handle = {};
	handle.index = nextComputeShaderSlot;
	handle.objType = D3D_OBJ_TYPES::COMPUTE_SHADER;
	nextComputeShaderSlot++;
	return handle;
}

void SearchForResources(uint32_t start, D3DHandle* resources, RESRC_VIEWS* resrcBindings, SHADER_TYPES* bindFor, uint32_t numResources, RESRC_VIEWS viewSearching,
						D3DHandle*& out_matchingResources, bool*& out_bindState, uint32_t& out_matchCtr)
{
	uint32_t matchCtr = 0;
	for (uint32_t k = start; k < numResources; k++)
	{
		if (resrcBindings[k] == viewSearching && bindFor[k] == bindFor[start])
		{
			out_matchingResources[matchCtr] = resources[k];
			out_bindState[k] = true;
			matchCtr++;
		}
	}
	out_matchCtr = matchCtr;
}

template<typename viewType>
struct BindableViewList
{
	viewType** views = nullptr;
	BindableViewList(uint32_t numViews, D3DHandle* handles)
	{
		views = Memory::AllocateArray<viewType*>(numViews);
		for (uint32_t k = 0; k < numViews; k++)
		{
			if (std::is_same_v<viewType, ID3D11UnorderedAccessView>)
			{
				views[k] = textures[handles[k]].uav.Get();
			}
			else if (std::is_same_v<viewType, ID3D11ShaderResourceView>)
			{
				views[k] = textures[handles[k]].srv.Get();
			}
			else if (std::is_same_v<viewType, ID3D11RenderTargetView>)
			{
				views[k] = textures[handles[k]].rtv.Get();
			}
			else if (std::is_same_v<viewType, ID3D11DepthStencilView>)
			{
				views[k] = textures[handles[k]].dsv.Get();
			}
			else // if (std::is_same_v<viewType, ID3D11Buffer>)
			{
				views[k] = buffers[handles[k]].resrc.Get();
			}
		}
	}

	~BindableViewList()
	{
		Memory::FreeToAddress(views);
	}
};

void BindResources(D3DHandle* resources, RESRC_VIEWS* resrcBindings, SHADER_TYPES* bindFor, uint32_t numResources)
{
	bool* resource_bound = Memory::AllocateArray<bool>(numResources);
	for (uint32_t i = 0; i < numResources; i++)
	{
		if (!resource_bound[i])
		{
			// Scan all views equal to [resrcBindings[i]] into a local buffer and bind together for fewer state changes; flag the selected textures/bindings

			D3DHandle* matchingResources = Memory::AllocateArray<D3DHandle>(numResources - i);
			uint32_t matchCtr = 0;

			SearchForResources(i, resources, resrcBindings, bindFor, numResources, resrcBindings[i], matchingResources, resource_bound, matchCtr);

			// Choose an appropriate binding for the current view type & requested shader stage
			// (could fill this in now but spent enough time already, and I'd immediately think of smoething else afterward ^_^')
			switch (resrcBindings[i])
			{
				SHADER_TYPES stage = bindFor[i];
				case UNORDERED_GPU_WRITES:
					if (stage == SHADER_TYPES::VS || stage == SHADER_TYPES::PS)
					{
						assert(("FL11.0 only supports UAVs in compute shaders", false));
					}
					else
					{
						BindableViewList<ID3D11UnorderedAccessView> bindings(matchCtr, matchingResources);
						context->CSSetUnorderedAccessViews(0, matchCtr, bindings.views->GetAddressOf(), nullptr);
					}
					break;

				case GENERIC_READONLY:
					if (stage == SHADER_TYPES::VS)
					{
						BindableViewList<ID3D11ShaderResourceView> bindings(matchCtr, matchingResources);
						context->VSSetShaderResources(0, matchCtr, bindings.views);
					}
					else if (stage == SHADER_TYPES::PS)
					{
						BindableViewList<ID3D11ShaderResourceView> bindings(matchCtr, matchingResources);
						context->PSSetShaderResources(0, matchCtr, bindings.views);
					}
					else // if (stage == SHADER_TYPES::CS)
					{
						BindableViewList<ID3D11ShaderResourceView> bindings(matchCtr, matchingResources);
						context->CSSetShaderResources(0, matchCtr, bindings.views);
					}
					break;

				case CONSTANT_BUFFER:
					if (stage == SHADER_TYPES::VS)
					{
						BindableViewList<ID3D11Buffer> bindings(matchCtr, matchingResources);
						context->VSSetConstantBuffers(0, matchCtr, bindings.views);
					}
					else if (stage == SHADER_TYPES::PS)
					{
						BindableViewList<ID3D11Buffer> bindings(matchCtr, matchingResources);
						context->PSSetConstantBuffers(0, matchCtr, bindings.views);
					}
					else // if (stage == SHADER_TYPES::CS)
					{
						BindableViewList<ID3D11Buffer> bindings(matchCtr, matchingResources);
						context->CSSetConstantBuffers(0, matchCtr, bindings.views);
					}
					break;

				case RENDER_TARGET:
				case DEPTH_STENCIL:
					if (resrcBindings[i] == RENDER_TARGET)
					{
						assert(("Render-targets can't be boound for compute shader dispatch - prefer a UAV (GPU_UNORDERED_WRITES)", stage != SHADER_TYPES::CS));

						// Resolve RTV bindlist
						BindableViewList<ID3D11RenderTargetView> rtvBindings(matchCtr, matchingResources);

						uint32_t numDepthBuffers = 0;
						D3DHandle* depthResources = Memory::AllocateArray<D3DHandle>(numResources - i);
						SearchForResources(i, resources, resrcBindings, bindFor, numResources, DEPTH_STENCIL, depthResources, resource_bound, numDepthBuffers);

						if (numDepthBuffers > 0)
						{
							assert(("Only one depth buffer can be bound for each draw", false));
							context->OMSetRenderTargets(matchCtr, rtvBindings.views, textures[depthResources[0].index].dsv.Get());
						}
						else
						{
							context->OMSetRenderTargets(matchCtr, rtvBindings.views, nullptr);
						}

						Memory::FreeToAddress(depthResources);
					}
					else
					{
						assert(("Depth-stencils can't be boound for compute shader dispatch - prefer a UAV (GPU_UNORDERED_WRITES)", stage != SHADER_TYPES::CS));

						// Resolve DSV bindlist
						BindableViewList<ID3D11DepthStencilView> depthBindings(matchCtr, matchingResources);

						uint32_t numRTVs = 0;
						D3DHandle* rtvResources = Memory::AllocateArray<D3DHandle>(numResources - i);
						SearchForResources(i, resources, resrcBindings, bindFor, numResources, RENDER_TARGET, rtvResources, resource_bound, numRTVs);

						BindableViewList<ID3D11RenderTargetView> rtvBindings(numRTVs, rtvResources);

						if (numRTVs > 0)
						{
							assert(("Only one depth buffer can be bound for each draw", false));
							context->OMSetRenderTargets(matchCtr, rtvBindings.views, depthBindings.views[0]);
						}
						else
						{
							context->OMSetRenderTargets(matchCtr, nullptr, depthBindings.views[0]);
						}

						Memory::FreeToAddress(rtvResources);
					}
					break;

				default:
					assert(("Invalid/unsupported resource binding", false));
					break;
			}

			// Free memory laon
			Memory::FreeToAddress(matchingResources);
		}
	}
	Memory::FreeToAddress(resource_bound);
}

void D3DWrapper::SubmitDraw(D3DHandle* draw_textures, RESRC_VIEWS* textureBindings, SHADER_TYPES* bindTexturesFor, uint32_t numTextures,
							D3DHandle* draw_buffers, RESRC_VIEWS* bufferBindings, SHADER_TYPES* bindBuffersFor, uint32_t numBuffers,
							D3DHandle* draw_volumes, RESRC_VIEWS* volumeBindings, SHADER_TYPES* bindVolumesFor, uint32_t numVolumes,
							D3DHandle VS, D3DHandle PS, bool directToBackbuf, bool is2D, D3DHandle vbuffer, D3DHandle ibuffer, uint32_t numNdces)
{
#ifdef _DEBUG
	for (uint32_t i = 0; i < numTextures; i++)
	{
		assert(("Direct write to back-buffer expected, but render-target view provided to D3DWrapper::SubmitDraw", !(directToBackbuf && textureBindings[i] == RENDER_TARGET)));
	}
#endif

	BindResources(draw_textures, textureBindings, bindTexturesFor, numTextures);

	if (directToBackbuf)
	{
		context->OMSetRenderTargets(1, backBufView.GetAddressOf(), textures[starterDepthBuffer.index].dsv.Get());
	}

	uint32_t vbufStride = is2D ? sizeof(Vertex2D) : sizeof(Vertex3D);
	context->IASetVertexBuffers(0, 1, buffers[vbuffer.index].resrc.GetAddressOf(), &vbufStride, 0);
	context->IASetIndexBuffer(buffers[ibuffer.index].resrc.Get(), DXGI_FORMAT_R32_UINT, 0);

	context->VSSetShader(vtShaders[VS.index].Get(), nullptr, 0);
	context->PSSetShader(pxShaders[PS.index].Get(), nullptr, 0);
	context->DrawIndexed(numNdces, 0, 0);
}

void D3DWrapper::SubmitDispatch(D3DHandle* dispatch_textures, RESRC_VIEWS* textureBindings, uint32_t numTextures,
								D3DHandle* dispatch_buffers, RESRC_VIEWS* bufferBindings, uint32_t numBuffers,
								D3DHandle* dispatch_volumes, RESRC_VIEWS* volumeBindings, uint32_t numVolumes,
								D3DHandle CS, uint32_t dispatchX, uint32_t dispatchY, uint32_t dispatchZ)
{
	SHADER_TYPES shaderType = SHADER_TYPES::CS;
	BindResources(dispatch_textures, textureBindings, &shaderType, numTextures);

	context->CSSetShader(computeShaders[CS.index].Get(), nullptr, 0);
	context->Dispatch(dispatchX, dispatchY, dispatchZ);
}

void D3DWrapper::PrepareBackbuf()
{
	// Clear the back-buffer & depth-buffer
	const FLOAT debug_red[4] = { 1, 0, 0, 1 };
	context->ClearRenderTargetView(backBufView.Get(), debug_red);
	context->ClearDepthStencilView(textures[starterDepthBuffer.index].dsv.Get(), 0, 0, 0);
}

void D3DWrapper::Present()
{
	swapchain->Present(using_vsync ? 4 : 0, // If vsync, try to synchronize for at least 4 frames (I suspect d3d11.1-3 have cleaner interfaces than this but api upgrade scary)
					   using_vsync ? 0 : DXGI_PRESENT_ALLOW_TEARING); // Allow tearing if no vsync
}
