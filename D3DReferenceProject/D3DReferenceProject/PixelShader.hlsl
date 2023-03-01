
#include "shaders_shared.hlsli"

float4 main(Pixel px) : SV_TARGET
{
    return float4(abs(px.normals.xyz), 1.0f);
}