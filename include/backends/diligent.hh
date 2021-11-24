#pragma once
#include <blazevg.hh>

#include <unordered_map>

#ifdef __APPLE__
#define PLATFORM_MACOS 1
#else
#define PLATFORM_WIN32 1
#endif
#include <Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <Graphics/GraphicsEngine/interface/SwapChain.h>
#include <Common/interface/RefCntAutoPtr.hpp>

namespace bvg {

namespace shader {

namespace solidcol {

struct PSConstants {
    Color color;
};

static const char* PSSource = R"(
cbuffer Constants
{
    float4 g_Color;
};

struct PSInput
{
    float4 Pos   : SV_POSITION;
};
struct PSOutput
{
    float4 Color : SV_TARGET;
};

void main(in  PSInput  PSIn,
          out PSOutput PSOut)
{
    PSOut.Color = g_Color;
}
)";

} // namespace solidcol

namespace lingrad {

struct PSConstants {
    Color startColor;
    Color endColor;
    glm::vec2 startPos;
    glm::vec2 endPos;
    glm::vec2 resolution;
};

static const char* PSSource = R"(
cbuffer Constants
{
    float4 g_StartColor;
    float4 g_EndColor;
    float2 g_StartPos;
    float2 g_EndPos;
    float2 g_Resolution;
};

struct PSInput
{
    float4 Pos   : SV_POSITION;
};
struct PSOutput
{
    float4 Color : SV_TARGET;
};

void main(in  PSInput  PSIn,
          out PSOutput PSOut)
{
    float2 gradient_start_pos = g_StartPos;
    float2 gradient_end_pos = g_EndPos;
    

    float4 color_start = g_StartColor;
    float4 color_end = g_EndColor;
    
    // this is the angle of the gradient in radians
    float alpha = atan2(
        gradient_end_pos.y - gradient_start_pos.y,
        gradient_end_pos.x - gradient_start_pos.x
    );
    
    float gradient_startpos_rotated_x = gradient_start_pos.x * cos(-alpha) - gradient_start_pos.y * sin(-alpha);
    float gradient_endpos_rotated_x = gradient_end_pos.x * cos(-alpha) - gradient_end_pos.y * sin(-alpha);
    float gradient_length = gradient_endpos_rotated_x - gradient_startpos_rotated_x;
    
    float2 UV = PSIn.Pos / g_Resolution;

    float x_loc_rotated = UV.x * cos(-alpha) - UV.y * sin(-alpha);
    
    float t = smoothstep(
        gradient_startpos_rotated_x,
        gradient_startpos_rotated_x + gradient_length,
        x_loc_rotated
    );

    float4 pixelColor = lerp(
        color_start,
        color_end,
        t
    );
    PSOut.Color = pixelColor;
}
)";

} // namespace lingrad

namespace msdf { // namespace msdf

static const char* GlyphVSSource = R"(
cbuffer Constants
{
    float4x4 g_ModelViewProj;
};

struct VSInput
{
    float2 Pos   : ATTRIB0;
    float2 TexCoord : ATTRIB1;
};

struct PSInput
{
    float4 Pos   : SV_POSITION;
    float2 UV    : TEX_COORD;
};

void main(in  VSInput VSIn,
          out PSInput PSIn)
{
    PSIn.Pos = mul(float4(VSIn.Pos, 0.0, 1.0), g_ModelViewProj);
    PSIn.UV = VSIn.TexCoord;
}
)";

struct PSConstants {
    glm::vec4 color;
    float distanceRange;
};

static const char* PSSource = R"(
cbuffer Constants
{
    float4 g_Color;
    float g_DistanceRange;
};

Texture2D    g_Texture;
SamplerState g_Texture_sampler;

struct PSInput
{
    float4 Pos   : SV_POSITION;
    float2 UV    : TEX_COORD;
};
struct PSOutput
{
    float4 Color : SV_TARGET;
};

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

void main(in  PSInput  PSIn,
          out PSOutput PSOut)
{
#if defined(DESKTOP_GL) || defined(GL_ES)
    float2 UV = float2(PSIn.UV.x, 1.0 - PSIn.UV.y);
#else
    float2 UV = PSIn.UV;
#endif
    float4 MSD = g_Texture.Sample(g_Texture_sampler, UV);
    float SDF = median(MSD.r, MSD.g, MSD.b);
    float Opacity = clamp(SDF * g_DistanceRange, 0.0, 1.0);
    if(Opacity < 0.5)
        discard;
    PSOut.Color = g_Color;
    PSOut.Color.a *= Opacity;
}
)";

}

struct VSConstants {
    glm::mat4 MVP;
};

static const char* VSSource = R"(
cbuffer Constants
{
    float4x4 g_ModelViewProj;
};

struct VSInput
{
    float2 Pos   : ATTRIB0;
};

struct PSInput
{
    float4 Pos   : SV_POSITION;
};

void main(in  VSInput VSIn,
          out PSInput PSIn)
{
    PSIn.Pos = mul(float4(VSIn.Pos, 0.0, 1.0), g_ModelViewProj);
}
)";

} // namespace shader

class DiligentContext;

namespace render {
    
class Shape {
public:
    Shape(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice,
          factory::ShapeMesh& mesh);
    
    void draw(DiligentContext& context, Style& style);
    
private:
    Diligent::RefCntAutoPtr<Diligent::IBuffer> vertexBuffer;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> indexBuffer;
    int numIndices = 0;
};

class SolidColorPipelineState {
public:
    SolidColorPipelineState(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice,
                            Diligent::TEXTURE_FORMAT colorBufferFormat,
                            Diligent::TEXTURE_FORMAT depthBufferFormat);
    SolidColorPipelineState();
    
    bool isInitialized = false;
    
    void recreate(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice,
                  Diligent::TEXTURE_FORMAT colorBufferFormat,
                  Diligent::TEXTURE_FORMAT depthBufferFormat);
    
    Diligent::RefCntAutoPtr<Diligent::IBuffer> VSConstants;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> PSConstants;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> PSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> SRB;
    Diligent::RefCntAutoPtr<Diligent::IShader> PS;
    Diligent::RefCntAutoPtr<Diligent::IShader> VS;
    
private:
    void createShaders(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice);
};

class LinearGradientPipelineState {
public:
    LinearGradientPipelineState(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice,
                            Diligent::TEXTURE_FORMAT colorBufferFormat,
                            Diligent::TEXTURE_FORMAT depthBufferFormat);
    LinearGradientPipelineState();
    
    bool isInitialized = false;
    
    void recreate(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice,
                  Diligent::TEXTURE_FORMAT colorBufferFormat,
                  Diligent::TEXTURE_FORMAT depthBufferFormat);
    
    Diligent::RefCntAutoPtr<Diligent::IBuffer> VSConstants;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> PSConstants;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> PSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> SRB;
    Diligent::RefCntAutoPtr<Diligent::IShader> PS;
    Diligent::RefCntAutoPtr<Diligent::IShader> VS;
    
private:
    void createShaders(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice);
};

static Diligent::Uint32 GlyphQuadIndices[] =
{
        0,1,2, 2,3,0
};

class GlyphMSDFShaders {
public:
    GlyphMSDFShaders(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice);
    GlyphMSDFShaders();
    
    Diligent::RefCntAutoPtr<Diligent::IBuffer> VSConstants;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> PSConstants;
    Diligent::RefCntAutoPtr<Diligent::IShader> PS;
    Diligent::RefCntAutoPtr<Diligent::IShader> VS;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> quadIndexBuffer;
};

struct PipelineConfiguration {
    BlendingMode blendingMode;
};

class CharacterQuad {
public:
    CharacterQuad(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice,
                  Font::Character& c, int size);
    CharacterQuad();
    
    int advance = 0, height = 0;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> vertexBuffer;
};

} // namespace render

class DiligentFont : public Font {
public:
    DiligentFont(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice,
                 Diligent::TEXTURE_FORMAT colorBufferFormat,
                 Diligent::TEXTURE_FORMAT depthBufferFormat,
                 std::string& json,
                 void* imageData,
                 int width,
                 int height,
                 int numChannels,
                 DiligentContext& context);
    
    std::unordered_map<int, render::CharacterQuad> chars;
    
    void loadCharacter(Character& character);
    
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> PSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> SRB;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> textureSRV;
    
    void recreatePipelineState(Diligent::TEXTURE_FORMAT colorBufferFormat,
                               Diligent::TEXTURE_FORMAT depthBufferFormat);
    
private:
    void createTexture(Diligent::TEXTURE_FORMAT colorBufferFormat,
                       void* imageData,
                       int width,
                       int height,
                       int numChannels);
    
    Diligent::RefCntAutoPtr<Diligent::ITexture> mTexture;
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> mRenderDevice;
    DiligentContext& mContext;
};

class DiligentContext : public Context {
public:
    friend DiligentFont;
    friend render::Shape;
    
    DiligentContext(float width, float height,
                    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice,
                    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> deviceContext,
                    Diligent::TEXTURE_FORMAT colorBufferFormat,
                    Diligent::TEXTURE_FORMAT depthBufferFormat);
    
    DiligentContext();
    
    void convexFill();
    void stroke();
    
    void loadFontFromMemory(std::string& json,
                            std::string fontName,
                            void* imageData,
                            int width,
                            int height,
                            int numChannels);
    
private:
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> mRenderDevice;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> mDeviceContext;
    Diligent::TEXTURE_FORMAT mColorBufferFormat;
    Diligent::TEXTURE_FORMAT mDepthBufferFormat;
    
    render::LinearGradientPipelineState mLinearGradientPSO;
    render::SolidColorPipelineState mSolidColorPSO;
    render::GlyphMSDFShaders mGlyphShaders;
    
    void initPipelineState();
};

} // namespace bvg
