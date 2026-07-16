struct VertexOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float4 color : COLOR0;
};

VertexOutput MainVs(uint vertexId : SV_VertexID)
{
    const float2 positions[3] = {
        float2(-0.5, -0.5),
        float2(0.0, 0.5),
        float2(0.5, -0.5),
    };

    VertexOutput output;
    output.position = float4(positions[vertexId % 3], 0.0, 1.0);
    output.color = float4(1.0, 0.0, 1.0, 1.0);
    return output;
}