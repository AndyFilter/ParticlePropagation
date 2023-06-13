struct PixelInput
{
    float4 position : SV_Position;
    float3 color : COLOR0;
    float2 uv : TEXCOORD0;
    float2 centerPos : POSITION1;
};

struct PixelOutput
{
    float4 attachment0 : SV_Target0;
};

PixelOutput main(PixelInput pixelInput)
{
    PixelOutput output;
    
    float2 uv = pixelInput.uv;
    float x = uv.x;
    float y = uv.y;
    
    output.attachment0 = float4(pixelInput.color, 1.0);
    
    return output;
}