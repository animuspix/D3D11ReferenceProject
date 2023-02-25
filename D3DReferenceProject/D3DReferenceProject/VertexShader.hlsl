
#include "shaders_shared.hlsli"

Pixel main( Vertex vt )
{
    // Convert model SQT to model matrix and multiply against view * projection product
    // Apply computed MVP to vertex position before passing on to the pixel-shader stage
    ////////////////////////////////////////////////////////////////////////////////////

    // Just a random filler transform for now, so we have something to compile
    //vt.pos += vt.normals.wxyz;
    vt.pos.z = 0.2f;
	return vt;
}