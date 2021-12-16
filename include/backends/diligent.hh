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
    // float2 UV = PSIn.Pos / g_Resolution;

    PSOut.Color = g_Color;
}
)";

} // namespace solidcol

struct GradientConstants {
    enum class Type {
        Linear = 0,
        Radial = 1,
        Conic = 2
    };
    
    GradientConstants(Style& style, glm::mat4& MVP, Context& context);
    GradientConstants();
    
    Color startColor;
    Color endColor;
    glm::vec2 startPos;
    glm::vec2 endPos;
    glm::vec2 resolution;
    float radiusOrAngle;
    Type type;
};

namespace grad {

struct PSConstants {
    GradientConstants gradient;
};

static const char* PSSource = R"(
cbuffer Constants
{
    float4 g_StartColor;
    float4 g_EndColor;
    float2 g_StartPos;
    float2 g_EndPos;
    float2 g_Resolution;
    float g_RadiusOrAngle;
    int g_Type;
};

Texture2D    g_Background;
SamplerState g_Background_sampler;

struct PSInput
{
    float4 Pos   : SV_POSITION;
};
struct PSOutput
{
    float4 Color : SV_TARGET;
};

float4 linearGradient(PSInput PSIn) {
    float2 gradientStartPos = g_StartPos;
    float2 gradientEndPos = g_EndPos;
    
    float4 colorStart = g_StartColor;
    float4 colorEnd = g_EndColor;
    
    // This is the angle of the gradient in radians
    float alpha = atan2(
        gradientEndPos.y - gradientStartPos.y,
        gradientEndPos.x - gradientStartPos.x
    );
    
    float gradientStartPosRotatedX = gradientStartPos.x * cos(-alpha) -
        gradientStartPos.y * sin(-alpha);
    float gradientEndPosRotatedX = gradientEndPos.x * cos(-alpha) -
        gradientEndPos.y * sin(-alpha);
    float gradientLength = gradientEndPosRotatedX - gradientStartPosRotatedX;
    
    float2 UV = PSIn.Pos / g_Resolution;

    float LocRotatedX = UV.x * cos(-alpha) - UV.y * sin(-alpha);
    
    float t = smoothstep(
        gradientStartPosRotatedX,
        gradientStartPosRotatedX + gradientLength,
        LocRotatedX
    );

    return lerp(
        colorStart,
        colorEnd,
        t
    );
}

float4 radialGradient(PSInput PSIn) {
    float dist = length(PSIn.Pos - g_StartPos * g_Resolution);
    float t = dist / g_RadiusOrAngle;
    t = clamp(t, 0.0, 1.0);
    return lerp(
        g_StartColor,
        g_EndColor,
        t
    );
}

float2 rotate(float2 v, float angle) {
    return float2(
            v.x * cos(angle) - v.y * sin(angle),
            v.x * sin(angle) + v.y * cos(angle)
    );
}

float4 conicGradient(PSInput PSIn) {
    float pi = 3.14;

    float2 UV = PSIn.Pos / g_Resolution;
    float2 relative = rotate(UV - g_StartPos, -g_RadiusOrAngle);

    float angle = atan2(relative.x, relative.y);
    float t = (angle + pi) / 2.0 / pi;
    
    return lerp(
        g_StartColor,
        g_EndColor,
        t
    );
}

void main(in  PSInput  PSIn,
          out PSOutput PSOut)
{
    if(g_Type == 0)
        PSOut.Color = linearGradient(PSIn);
    else if(g_Type == 1)
        PSOut.Color = radialGradient(PSIn);
    else if(g_Type == 2)
        PSOut.Color = conicGradient(PSIn);
}
)";

} // namespace grad

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
    Color color;
    float distanceRange;
    bool isLinearGradient;
    bool _padding[8];
    GradientConstants gradient;
};

static const char* PSSource = R"(
cbuffer Constants
{
    float4 g_Color;
    float g_DistanceRange;
    bool g_IsLinearGradient;
    float4 g_StartColor;
    float4 g_EndColor;
    float2 g_StartPos;
    float2 g_EndPos;
    float2 g_Resolution;
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

float4 linearGradient(PSInput PSIn) {
    float2 gradientStartPos = g_StartPos;
    float2 gradientEndPos = g_EndPos;
    
    float4 colorStart = g_StartColor;
    float4 colorEnd = g_EndColor;
    
    // This is the angle of the gradient in radians
    float alpha = atan2(
        gradientEndPos.y - gradientStartPos.y,
        gradientEndPos.x - gradientStartPos.x
    );
    
    float gradientStartPosRotatedX = gradientStartPos.x * cos(-alpha) -
        gradientStartPos.y * sin(-alpha);
    float gradientEndPosRotatedX = gradientEndPos.x * cos(-alpha) -
        gradientEndPos.y * sin(-alpha);
    float gradientLength = gradientEndPosRotatedX - gradientStartPosRotatedX;
    
    float2 UV = PSIn.Pos / g_Resolution;

    float LocRotatedX = UV.x * cos(-alpha) - UV.y * sin(-alpha);
    
    float t = smoothstep(
        gradientStartPosRotatedX,
        gradientStartPosRotatedX + gradientLength,
        LocRotatedX
    );

    return lerp(
        colorStart,
        colorEnd,
        t
    );
}

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
    if(g_IsLinearGradient) {
        PSOut.Color = linearGradient(PSIn);
    }
    else {
        PSOut.Color = g_Color;
    }
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

struct PipelineStateConfiguration {
    std::string name = "Pipeline state object";
    Diligent::RefCntAutoPtr<Diligent::IShader> vertexShader;
    Diligent::RefCntAutoPtr<Diligent::IShader> pixelShader;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> VSConstants;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> PSConstants;
    Diligent::TEXTURE_FORMAT colorBufferFormat;
    Diligent::TEXTURE_FORMAT depthBufferFormat;
    int numSamples = 1;
    bool isClippingMask = false;
};

struct PipelineState {
    PipelineState(PipelineStateConfiguration conf,
                  Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice);
    PipelineState();
    
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> PSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> SRB;
};

class SolidColorPipelineStates {
public:
    
    SolidColorPipelineStates(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice,
                            Diligent::TEXTURE_FORMAT colorBufferFormat,
                            Diligent::TEXTURE_FORMAT depthBufferFormat,
                            int numSamples = 1);
    SolidColorPipelineStates();
    
    bool isInitialized = false;
    
    PipelineState normalPSO;
    PipelineState clipPSO;
    
    void recreate(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice,
                  Diligent::TEXTURE_FORMAT colorBufferFormat,
                  Diligent::TEXTURE_FORMAT depthBufferFormat,
                  BlendingMode blendingMode,
                  int numSamples = 1);
    
    Diligent::RefCntAutoPtr<Diligent::IBuffer> VSConstants;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> PSConstants;
    Diligent::RefCntAutoPtr<Diligent::IShader> PS;
    Diligent::RefCntAutoPtr<Diligent::IShader> VS;
    int numSamples = 1;
    
private:
    void createShaders(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice);
};

class GradientPipelineStates {
public:
    GradientPipelineStates(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice,
                          Diligent::TEXTURE_FORMAT colorBufferFormat,
                          Diligent::TEXTURE_FORMAT depthBufferFormat,
                          int numSamples = 1);
    GradientPipelineStates();
    
    bool isInitialized = false;
    
    void recreate(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice,
                  Diligent::TEXTURE_FORMAT colorBufferFormat,
                  Diligent::TEXTURE_FORMAT depthBufferFormat,
                  BlendingMode blendingMode,
                  int numSamples = 1);
    
    Diligent::RefCntAutoPtr<Diligent::IBuffer> VSConstants;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> PSConstants;
    Diligent::RefCntAutoPtr<Diligent::IShader> PS;
    Diligent::RefCntAutoPtr<Diligent::IShader> VS;
    int numSamples = 1;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> PSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> SRB;
    
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
                 int numSamples,
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
                               Diligent::TEXTURE_FORMAT depthBufferFormat,
                               int numSamples);
    
private:
    void createTexture(Diligent::TEXTURE_FORMAT colorBufferFormat,
                       void* imageData,
                       int width,
                       int height,
                       int numChannels);
    
    Diligent::RefCntAutoPtr<Diligent::ITexture> mTexture;
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> mRenderDevice;
    DiligentContext& mContext;
    Diligent::TEXTURE_FORMAT mColorBufferFormat;
    Diligent::TEXTURE_FORMAT mDepthBufferFormat;
    int mNumSamples = 1;
};

class DiligentContext : public Context {
public:
    friend DiligentFont;
    friend render::Shape;
    
    DiligentContext(float width, float height,
                    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice,
                    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> deviceContext,
                    Diligent::TEXTURE_FORMAT colorBufferFormat,
                    Diligent::TEXTURE_FORMAT depthBufferFormat,
                    int numSamples = 1);
    
    DiligentContext();
    
    void beginClip();
    void endClip();
    void clearClip();
    
    void convexFill();
    void fill();
    void stroke();
    
    void textFill(std::wstring str, float x, float y);
    void textFillOnPath(std::wstring str, float x = 0, float y = 0);
    
    float measureTextWidth(std::wstring str);
    float measureTextHeight();
    
    void loadFontFromMemory(std::string& json,
                            std::string fontName,
                            void* imageData,
                            int width,
                            int height,
                            int numChannels);
    
    void setupPipelineStates(Diligent::TEXTURE_FORMAT colorBufferFormat,
                             Diligent::TEXTURE_FORMAT depthBufferFormat,
                             int numSamples);
    
    void specifyTextureViews(Diligent::ITextureView* RTV,
                             Diligent::ITextureView* DSV);
    
private:
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> mRenderDevice;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> mDeviceContext;
    Diligent::TEXTURE_FORMAT mColorBufferFormat;
    Diligent::TEXTURE_FORMAT mDepthBufferFormat;
    
    render::GradientPipelineStates mGradientPSO;
    render::SolidColorPipelineStates mSolidColorPSO;
    render::GlyphMSDFShaders mGlyphShaders;
    
    Diligent::ITextureView* mDSV = nullptr;
    
    bool mIsClipping = false;
    
    int mNumSamples = 1;
    
    glm::mat4 bringToFrontMatrix(glm::mat4 MVP);
    glm::mat4 getMatrix3D();
    
    void initPipelineState();
};

} // namespace bvg
