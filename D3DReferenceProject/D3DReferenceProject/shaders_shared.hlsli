
struct Vertex
{
    float4 pos : POSITION;
    float4 mat : TEXCOORD0;
    float4 normals : TEXCOORD1;
};

struct Pixel
{
    float4 pos : SV_POSITION;
    float4 mat : TEXCOORD0;
    float4 normals : TEXCOORD1;
};