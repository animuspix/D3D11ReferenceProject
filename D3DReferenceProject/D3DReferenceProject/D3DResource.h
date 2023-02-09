#pragma once

#include <d3d11.h>
#include "D3DUtils.h"
#include "D3DWrapper.h"
#include <cassert>

template<RESOURCE_TYPES resource>
struct D3DResource
{
	static constexpr bool is_texture = resource == RESOURCE_TYPES::TEXTURE;
	static constexpr bool is_volume = resource == RESOURCE_TYPES::VOLUME;
	static constexpr bool is_buffer = resource == RESOURCE_TYPES::BUFFER;

	struct D3DResourceDesc
	{
		static constexpr uint32_t numDimensions = is_texture ? 2 : is_volume ? 3 : 1;
		uint32_t elts_per_axis[numDimensions] = {};

		void* init_data = nullptr;
		uint32_t data_footprint_bytes = 0;

		DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;

		bool structured = false; // Only true for freely-bound buffers with arbitrarily-sized members (not textures, cbuffers, or vertex buffers)
		bool presentable = false; // Only true for render-targets that could be used for final present (so not generic textures, or any kind of buffer)
	};

	void Init(D3DResourceDesc desc, RESRC_ACCESS_TYPES access, RESRC_VIEWS composed_views)
	{
		// Verify views
		if (access == RESRC_ACCESS_TYPES::STAGING)
		{
			assert(composed_views == RESRC_VIEWS::VIEWS_UNSPECIFIED); // Staging resources cannot be bound, thus cannot have views
		}
		else
		{
			assert(composed_views != RESRC_VIEWS::VIEWS_UNSPECIFIED); // Non-staging resources must specify a view (no way to bind them otherwise)
		}

		const bool isCBuffer = (composed_views & RESRC_VIEWS::CONSTANT_BUFFER) != 0;
		const bool isIndex = (composed_views & RESRC_VIEWS::INDEX) != 0;
		const bool isVertex = (composed_views & RESRC_VIEWS::VERTEX) != 0;
		if (!is_buffer)
		{
			assert(!isCBuffer); // Constant buffer is an invalid binding for textures & volumes

			if (is_volume)
			{
				// Volumes are not valid render-targets or depth-stencils (use a UAV to write out slices instead)
				assert((composed_views & RESRC_VIEWS::RENDER_TARGET) != 0);
				assert((composed_views & RESRC_VIEWS::DEPTH_STENCIL) != 0);
			}
		}
		else
		{
			if (isCBuffer)
			{
				assert(composed_views = RESRC_VIEWS::CONSTANT_BUFFER); // CBuffers can't have any other view type; not enforced by the API, but variations are either very inefficient (cbuffer -> UAV), unnecessary (cbuffer -> readonly),
														  // or extremely complicated (cbuffer -> vertex buffer). Other conversions (e.g. cbuffer -> index buffer, cbuffer -> render target) are very impractical.
			}
			else
			{
				// Buffers can't be bound with texture views
				assert(((uint32_t)composed_views & ((uint32_t)RESRC_VIEWS::RENDER_TARGET | (uint32_t)RESRC_VIEWS::DEPTH_STENCIL)) == 0);

				// Vertex buffers can't be bound as index buffers (totally different data format & layout)
				if (isVertex)
				{
					assert(!isIndex);
				}
			}
		}

		if (desc.structured)
		{
			// Structured buffers can only be accessed through UAVs (!!!)
			assert(composed_views == RESRC_VIEWS::UNORDERED_GPU_WRITES);
		}

		// Not a real DirectX restriction, but writing GPU-resident textures or buffers after initialization through Map/Unmap is very slow, as is storing heavier resources in shared/CPU memory
		// (as with D3D11_USAGE_DYNAMIC)
		// So we want to enforce that the only resources with explicit CPU_WRITE allowed are constant buffers
		// (STAGING resources implicitly have CPU read/write access)
		if (access == RESRC_ACCESS_TYPES::CPU_WRITE)
		{
			assert(isCBuffer);
		}

		// Verify format
		if (isCBuffer || desc.structured || isVertex)
		{
			assert(desc.fmt == DXGI_FORMAT_UNKNOWN);
		}
		else if (desc.presentable)
		{
			assert(desc.fmt == D3DWrapper::GetSwapchainFormat());
		}
		else if (isIndex)
		{
			assert(desc.fmt == DXGI_FORMAT_R16_UINT || desc.fmt == DXGI_FORMAT_R32_UINT);
		}
		else if ((composed_views & RESRC_VIEWS::DEPTH_STENCIL) != 0)
		{
			assert(desc.fmt == DXGI_FORMAT_D16_UNORM || desc.fmt == DXGI_FORMAT_D24_UNORM_S8_UINT || DXGI_FORMAT_D32_FLOAT || DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
		}
		// Eventually we'll whitelist a bunch of volumetric formats here

		// Verify dimensions
		for (uint32_t i = 0; i < desc.numDimensions; i++)
		{
			assert(desc.elts_per_axis[i] > 0);
		}

		// Verify data footprint
		// We could fail gracefully here & reconstruct where possible...but we need this code out quickly and that would be slow
		// (+ computing total footprint on the user side without padding isn't super horribly difficult anyway)
		assert(desc.data_footprint_bytes > 0);

		// Verification passed! ^_^
		// Onto resource creation
		if (resource == RESOURCE_TYPES::TEXTURE)
		{
			resource_handle = D3DWrapper::CreateTexture(desc.elts_per_axis[0], desc.elts_per_axis[1], desc.fmt, access, composed_views, desc.init_data, desc.data_footprint_bytes);
		}
		else if (resource == RESOURCE_TYPES::BUFFER)
		{
			resource_handle = D3DWrapper::CreateBuffer(desc.elts_per_axis[0], desc.fmt, access, composed_views, desc.structured, desc.init_data, desc.data_footprint_bytes);
		}
		else
		{
			resource_handle = D3DWrapper::CreateVolume(desc.elts_per_axis[0], desc.elts_per_axis[1], desc.elts_per_axis[2], desc.fmt, access, composed_views, desc.init_data, desc.data_footprint_bytes);
		}
	}

	void DeInit()
	{
		D3DWrapper::ClearResrc(resource_handle);
	}

	D3DHandle resource_handle;
};
