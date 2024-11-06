cbuffer ConstantBuffer : register(b0)
{
    matrix WorldViewProj;
}

struct VOut
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

VOut VShader(float4 position : POSITION, float4 color : COLOR)
{
    VOut output;
    output.position = mul(position, WorldViewProj);
    output.color = color;
    return output;
}

float4 PShader(VOut input) : SV_TARGET
{
    return input.color;
}