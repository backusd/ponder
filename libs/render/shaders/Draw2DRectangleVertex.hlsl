struct Draw2DFrameConstants
{
    uint2 pixelExtent;
};

[[vk::push_constant]]
ConstantBuffer<Draw2DFrameConstants> draw2DFrame;

struct VertexInput
{
    [[vk::location(0)]] float2 pixelPosition : POSITION0;
    [[vk::location(1)]] float4 color : COLOR0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    [[vk::location(0)]] float4 color : COLOR0;
};

VertexOutput MainVs(VertexInput input)
{
    const float2 reciprocalExtent = 1.0 / float2(draw2DFrame.pixelExtent);
    const float2 clipPosition = input.pixelPosition * reciprocalExtent * 2.0 - 1.0;

    VertexOutput output;
    output.position = float4(clipPosition, 0.0, 1.0);
    output.color = input.color;
    return output;
}
