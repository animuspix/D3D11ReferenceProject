#pragma once

#include <stdint.h>
#include <DirectXMath.h>

enum class RESOURCE_TYPES
{
	TEXTURE,
	BUFFER,
	VOLUME
};

enum class RESRC_ACCESS_TYPES
{
	CPU_WRITE, // With Map/Unmap, CPU_READ is possible but unsupported (inefficient compared to copies through a staging resource)
	GPU_ONLY, // Most performant for most resources, except constant buffers
	STAGING // Special resource equally accessible from CPU and GPU, but only through copies; used for efficient resource downloads & uploads
			// More specifically, the only writes from CPU to GPU allowed for staging resources are copy operations. Staging resources implicitly support CPU read & write accesses.
};

enum RESRC_VIEWS // Multiple supported, since multiple may be used in practice with the same resource
{
	UNORDERED_GPU_WRITES = 1, // A resource that can be written from the GPU at any time during each dispatch/draw, and have arbitrarily-sized elements
	// Buffers that do not correspond to a regular format layout (i.e. do not resemble a vector with up to 4 elements up to 32 bits each) have to use this view,
	// even if they aren't written during the frame, due to a quirk in how D3D11 was specified & implemented by drivers. Arbitrarily-sized buffers created this
	// way are referred to as "StructuredBuffer"s from HLSL

	GENERIC_READONLY = 1 << 1, // A view that allows resources on the GPU but not written; those resources are either initialized on creation, updated from the CPU each frame, or initially updated on the GPU
							   // (through a uav/unordered access view) before being accessed through this view for better performance

	CONSTANT_BUFFER = 1 << 2, // Similar to GENERIC_READONLY, but specifically for small structured arrays of constant data. Each cbuffer contains just one instance of one structure, the members of
	// which are visisble to all shader instances during the draw in which the cbuffer was bound.
	// In D3D11, I believe the cbuffer structure only has to align to 16-byte boundaries (i.e. every member has to be a float4/vector). It's possible that the entire structure
	// has to be at least 256 bytes, but that might be specific to D3D12.

	RENDER_TARGET = 1 << 3, // Only applicable to textures
	DEPTH_STENCIL = 1 << 4, // Only applicable to textures, as with RESRC_VIEWS::RENDER_TARGET, and only supported formats are D24_UNORM_S8_UINT, and D16_UNORM
	// D24_UNORM_S8_UINT has 24 bits of depth with 8 bits of stencil; D16_UNORM has 16 bits of depth and no stencil value

	VERTEX = 1 << 5, // Technically not a view, but mostly behaves like one, and converted to a proper view in D3D12. Only applicable to buffers, contains arbitrarily-sized data that must match an "input layout" provided to the
	// input assembler on startup. After binding, draws take the data provided by the vertex buffer and process each structure using the graphics pipeline (assemble positions, apply VS, depth-clip, perform HSR if no
	// blending, apply PS, rasterize surviving pixels to the target & perform output merger)

	INDEX = 1 << 6, // Similar to RESRC_VIEWS::VERTEX, not a view, but behaves like a view over a buffer of integers (must be R32_UINT or R16_UINT). Used to define custom topologies for vertex buffers, which can save space/time by reusing the
	// same vertices in different triangles (imagine a cube, with 8 vertices and 12 tris - without an index buffer, every triangle would need unique verts, so you would have 36 verts total and most of them would
	// overlap)

	VIEWS_UNSPECIFIED = 1 << 7, // Staging resources cannot be boumd, so cannot have any views specified
	NUM_VIEWS = 7
};

enum class D3D_OBJ_TYPES
{
	TEXTURE,
	BUFFER,
	VOLUME,
	COMPUTE_SHADER,
	VERTEX_SHADER,
	PIXEL_SHADER
};

enum class SHADER_TYPES
{
	VS,
	PS,
	CS
};

struct D3DHandle
{

	uint16_t index;
	D3D_OBJ_TYPES objType;
};

struct Vertex3D
{
	DirectX::XMFLOAT4 pos; // W is unused
	DirectX::XMFLOAT4 mat; // UVs in x,y, material ID in z, model ID in w
	DirectX::XMFLOAT4 normals; // W is unused

	// Vertices are aligned to 16-byte boundaries!
	// Thus better to reserve spare data and maybe use them later than have the space mysteriously filled in by the driver at runtime

private:
	bool EpsEquality(float x, float y, float eps) // Larger epsilon can allow for vertex deduplication, but we want to do that separately to indexing
	{
		return fabs(x - y) < eps;
	}

	bool VectorCompare(DirectX::XMFLOAT4 a, DirectX::XMFLOAT4 b, float eps)
	{
		return EpsEquality(a.x, b.x, eps) && EpsEquality(a.y, b.y, eps) &&
			   EpsEquality(a.z, b.z, eps) && EpsEquality(a.w, b.w, eps);
	}

public:

	bool operator==(const Vertex3D& rhs)
	{
		constexpr float equality_eps = 0.00001f;
		bool posEqual = VectorCompare(pos, rhs.pos, equality_eps);
		bool matEqual = VectorCompare(mat, rhs.mat, equality_eps);
		bool normalsEqual = VectorCompare(normals, rhs.normals, equality_eps);
		return posEqual && matEqual && normalsEqual;
	}
};

struct Vertex2D
{
	DirectX::XMFLOAT4 pos; // XY are positions, ZW are UVs
};

struct SQT_Transform
{
	DirectX::XMFLOAT4 q; // sin(theta) * axis, cos*theta)
	DirectX::XMFLOAT4 ts; // scale in w, translation in XYZ
};

enum MATERIAL_TYPES
{
	DIFFUSE, // Lambert
	DIFFUSE_PBR, // Oren-Nayar
	SPECULAR_SHINY, // Needs GI to implement cleanly
	SPECULAR_TRANSLUCENT // Needs GI to implement
};

// DirectX::XMMATRIX SQT_to_Matrix()
// {
//
// }
