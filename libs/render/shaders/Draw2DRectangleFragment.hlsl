struct FragmentInput
{
    [[vk::location(0)]] float4 color : COLOR0;
};

float4 MainPs(FragmentInput input) : SV_Target0
{
    return input.color;
}
