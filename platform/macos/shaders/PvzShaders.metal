#include <metal_stdlib>

using namespace metal;

struct SpriteVertex
{
    float2 mPosition;
    float2 mTextureCoordinate;
    float4 mColor;
};

struct RasterVertex
{
    float4 mPosition [[position]];
    float2 mTextureCoordinate;
    float4 mColor;
};

vertex RasterVertex pvz_sprite_vertex(
    const device SpriteVertex* theVertices [[buffer(0)]],
    constant float2& theLogicalSize [[buffer(1)]],
    uint theVertexIndex [[vertex_id]])
{
    const SpriteVertex aVertex = theVertices[theVertexIndex];
    RasterVertex anOutput;
    anOutput.mPosition = float4(
        aVertex.mPosition.x * (2.0f / theLogicalSize.x) - 1.0f,
        1.0f - aVertex.mPosition.y * (2.0f / theLogicalSize.y),
        0.0f,
        1.0f);
    anOutput.mTextureCoordinate = aVertex.mTextureCoordinate;
    anOutput.mColor = aVertex.mColor;
    return anOutput;
}

fragment half4 pvz_sprite_fragment(
    RasterVertex theInput [[stage_in]],
    texture2d<half> theTexture [[texture(0)]],
    sampler theSampler [[sampler(0)]])
{
    return theTexture.sample(
               theSampler,
               theInput.mTextureCoordinate) *
           half4(theInput.mColor);
}

struct PresentationVertex
{
    float4 mPosition [[position]];
    float2 mTextureCoordinate;
};

vertex PresentationVertex pvz_present_vertex(
    uint theVertexIndex [[vertex_id]])
{
    constexpr float2 kPositions[] = {
        {-1.0f, -1.0f},
        {1.0f, -1.0f},
        {-1.0f, 1.0f},
        {1.0f, 1.0f},
    };
    constexpr float2 kTextureCoordinates[] = {
        {0.0f, 1.0f},
        {1.0f, 1.0f},
        {0.0f, 0.0f},
        {1.0f, 0.0f},
    };

    PresentationVertex anOutput;
    anOutput.mPosition =
        float4(kPositions[theVertexIndex], 0.0f, 1.0f);
    anOutput.mTextureCoordinate =
        kTextureCoordinates[theVertexIndex];
    return anOutput;
}

fragment half4 pvz_present_fragment(
    PresentationVertex theInput [[stage_in]],
    texture2d<half> theTexture [[texture(0)]],
    sampler theSampler [[sampler(0)]])
{
    return theTexture.sample(
        theSampler,
        theInput.mTextureCoordinate);
}
