cbuffer const_data : register(b0)
{
    double physTime;
    float2 simSize;
}


struct MotionEquation
{
    float x_multiplier;
    float y_multiplier;

    float x_offset;
    float y_offset;
    
    float2 GetPosForTime(double time)
    {
        float2 finalPos;
        finalPos.x = (x_multiplier * time) / 10000 + x_offset;
        finalPos.y = (y_multiplier * time) / 10000 + y_offset;
        
        // map to the local viewport
        finalPos /= simSize;
        finalPos *= 2;
        finalPos -= float2(1, 1);
        finalPos.y *= -1;

        return finalPos;
    }
};

struct VertexInput
{
    float3 inPos : POSITION;
    float3 inColor : COLOR;
    float2 uv : TEXCOORD;
    
    //MotionEquation me : I_MOTION_EQ;
    float4 me : I_MOTION_EQ;
    float timeCreated : I_TIME_CREATED;
    float3 instColor : I_COLOR;
};

struct VertexOutput // Pixel Shader Input
{
    float4 position : SV_Position;
    float3 color : COLOR0;
    float2 uv : TEXCOORD0;
    float2 centerPos : POSITION1;
};

VertexOutput main(VertexInput vertexInput)
{
    float3 inColor = vertexInput.inColor;
    float3 inPos = vertexInput.inPos;
    float3 outColor = inColor;

    MotionEquation me = (MotionEquation)vertexInput.me;
    
    VertexOutput output;
    output.position = float4(inPos.xy + me.GetPosForTime(physTime - vertexInput.timeCreated), 1, 1.f); // float4(inPos.xy + float2(physTime/1000000, 0), 1, 1);
    output.color = float3(1, 1, 1); //vertexInput.instColor;
    output.uv = vertexInput.uv;
    output.centerPos = vertexInput.inPos;
    return output;
}