// Compute shader: linear (box) blur with a variable radius. Reads one PingPong
// texture and writes into the other. The radius is driven from the CPU so the
// blur amount can be animated (e.g. abs(sin(t))).

cbuffer BlurParams : register(b0)
{
    int blurRadius;
};

Texture2D<float4> InputTexture : register(t0);
RWTexture2D<float4> OutputTexture : register(u0);

[numthreads(16, 16, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    uint2 texDim;
    InputTexture.GetDimensions(texDim.x, texDim.y);

    if (dtid.x >= texDim.x || dtid.y >= texDim.y)
        return;

    float4 color = float4(0.0f, 0.0f, 0.0f, 0.0f);
    int count = 0;

    for (int y = -blurRadius; y <= blurRadius; ++y)
    {
        for (int x = -blurRadius; x <= blurRadius; ++x)
        {
            int2 coord = int2(dtid.x + x, dtid.y + y);
            coord = clamp(coord, int2(0, 0), int2(texDim.x - 1, texDim.y - 1));
            color += InputTexture[coord];
            ++count;
        }
    }

    OutputTexture[dtid.xy] = color / (float)count;
}

// Graphics shader: fullscreen quad that samples the currently blurred texture.

struct VSInput
{
    float2 position : POSITION;
    float2 uv : TEXCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

PSInput VSMain(VSInput input)
{
    PSInput result;
    result.position = float4(input.position, 0.0f, 1.0f);
    result.uv = input.uv;
    return result;
}

Texture2D<float4> ScreenTexture : register(t0);
SamplerState LinearSampler : register(s0);

float4 PSMain(PSInput input) : SV_TARGET
{
    return ScreenTexture.Sample(LinearSampler, input.uv);
}
