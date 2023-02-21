#pragma once

#include "D3DUtils.h"
#include <d3d11.h>

static class D3DWrapper
{
	public:
	static void Init(HWND hwnd, uint32_t window_width, uint32_t window_height, bool vsync);
	static void DeInit();

	static D3DHandle CreateTexture(uint32_t width, uint32_t height, DXGI_FORMAT format, RESRC_ACCESS_TYPES access, RESRC_VIEWS composed_views, void* init_data, uint32_t data_footprint_bytes);
	static D3DHandle CreateBuffer(uint32_t num_elements, DXGI_FORMAT format, RESRC_ACCESS_TYPES access, RESRC_VIEWS composed_views, bool structured, void* init_data, uint32_t data_footprint_bytes);
	static D3DHandle CreateVolume(uint32_t width, uint32_t height, uint32_t depth, DXGI_FORMAT format, RESRC_ACCESS_TYPES access, RESRC_VIEWS composed_views, void* init_data, uint32_t data_footprint_bytes);
	static void		 ClearResrc(D3DHandle handle);

	static D3DHandle CreateVertShader(const char* path, bool is2D);
	static D3DHandle CreatePixelShader(const char* path);
	static D3DHandle CreateComputeShader(const char* path);

	static void SubmitDraw(D3DHandle* draw_textures, RESRC_VIEWS* textureBindings, uint32_t numTextures,
						   D3DHandle* draw_buffers, RESRC_VIEWS* bufferBindings, uint32_t numBuffers,
						   D3DHandle* draw_volumes, RESRC_VIEWS* volumeBindings, uint32_t numVolumes,
						   D3DHandle VS, D3DHandle PS, bool directToBackbuf, bool is2D, D3DHandle vbuffer, D3DHandle ibuffer, uint32_t numNdces);

	static void SubmitDispatch(D3DHandle* textures, RESRC_VIEWS* textureBindings, uint32_t numTextures,
							   D3DHandle* buffers, RESRC_VIEWS* bufferBindings, uint32_t numBuffers,
							   D3DHandle* volumes, RESRC_VIEWS* volumeBindings, uint32_t numVolumes,
							   D3DHandle CS, uint32_t dispatchX, uint32_t dispatchY, uint32_t dispatchZ);

	static void PrepareBackbuf();
	static void Present();
};

