#pragma once
#include <blazevg.hh>

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
};

static const char* PSSource = R"(
cbuffer Constants
{
    float4 g_StartColor;
    float4 g_EndColor;
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
    PSOut.Color = g_StartColor;
}
)";

} // namespace lingrad

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
    
    void draw(DiligentContext& context, Style style);
    
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

} // namespace render

class DiligentContext : public Context {
public:
    friend render::Shape;
    
    DiligentContext(float width, float height,
                    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice,
                    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> deviceContext,
                    Diligent::TEXTURE_FORMAT colorBufferFormat,
                    Diligent::TEXTURE_FORMAT depthBufferFormat);
    
    DiligentContext();
    
    void convexFill();
    void stroke();
    
private:
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> mRenderDevice;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> mDeviceContext;
    Diligent::TEXTURE_FORMAT mColorBufferFormat;
    Diligent::TEXTURE_FORMAT mDepthBufferFormat;
    
    render::SolidColorPipelineState mSolidColorPSO;
    
    void initPipelineState();
};

} // namespace bvg
