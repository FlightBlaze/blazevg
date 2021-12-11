#include <backends/diligent.hh>
#include <Graphics/GraphicsTools/interface/CommonlyUsedStates.h>
#include <Graphics/GraphicsTools/interface/MapHelper.hpp>
#include <glm/gtx/transform.hpp>

namespace bvg {

DiligentContext::DiligentContext(float width, float height,
                Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice,
                Diligent::RefCntAutoPtr<Diligent::IDeviceContext> deviceContext,
                Diligent::TEXTURE_FORMAT colorBufferFormat,
                Diligent::TEXTURE_FORMAT depthBufferFormat,
                int numSamples):
    Context(width, height),
    mRenderDevice(renderDevice),
    mDeviceContext(deviceContext),
    mColorBufferFormat(colorBufferFormat),
    mDepthBufferFormat(depthBufferFormat),
    mNumSamples(numSamples)
{
    initPipelineState();
}

DiligentContext::DiligentContext()
{
}

void DiligentContext::initPipelineState() {
    mSolidColorPSO = render::SolidColorPipelineStates(mRenderDevice,
                                                     mColorBufferFormat,
                                                     mDepthBufferFormat,
                                                     mNumSamples);
    
    mGradientPSO = render::GradientPipelineStates(mRenderDevice,
                                                 mColorBufferFormat,
                                                 mDepthBufferFormat,
                                                 mNumSamples);
    
    mGlyphShaders = render::GlyphMSDFShaders(mRenderDevice);
}

namespace shader {

GradientConstants::GradientConstants(Style& style, glm::mat4& MVP, Context& context) {
    switch(style.type) {
        case Style::Type::LinearGradient:
            this->type = Type::Linear;
            this->startColor = style.linear.startColor;
            this->endColor = style.linear.endColor;
            this->startPos = glm::vec2(MVP * glm::vec4(style.linear.startX,
                                                       style.linear.startY,
                                                       0.0f, 1.0f));
            this->endPos = glm::vec2(MVP * glm::vec4(style.linear.endX,
                                                     style.linear.endY,
                                                     0.0f, 1.0f));
            // Convert range (-1.0, 1.0) to (0.0, 1.0)
            // and invert Y coordinate
            this->startPos = (this->startPos + 1.0f) / 2.0f;
            this->endPos = (this->endPos + 1.0f) / 2.0f;
            this->startPos.y = 1.0f - this->startPos.y;
            this->endPos.y = 1.0f - this->endPos.y;
            break;
        case Style::Type::RadialGradient:
            this->type = Type::Radial;
            this->startColor = style.radial.startColor;
            this->endColor = style.radial.endColor;
            this->startPos = glm::vec2(MVP * glm::vec4(style.radial.x,
                                                       style.radial.y,
                                                       0.0f, 1.0f));
            this->startPos = (this->startPos + 1.0f) / 2.0f;
            this->startPos.y = 1.0f - this->startPos.y;
            this->radiusOrAngle = style.radial.radius;
            break;
        case Style::Type::ConicGradient:
            this->type = Type::Conic;
            this->startColor = style.conic.startColor;
            this->endColor = style.conic.endColor;
            this->startPos = glm::vec2(MVP * glm::vec4(style.conic.x,
                                                       style.conic.y,
                                                       0.0f, 1.0f));
            this->startPos = (this->startPos + 1.0f) / 2.0f;
            this->startPos.y = 1.0f - this->startPos.y;
        {
            // Rotate angle with MVP matrix. Angle will be non zero
            // if we deal with rotated matrix
            glm::vec2 point1 = glm::vec2(MVP * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
            glm::vec2 point2 = glm::vec2(MVP * glm::vec4(0.0f, -1.0f, 0.0f, 1.0f));
            glm::vec2 relative = point2 - point1;
            float addAngle = atan2f(relative.x, relative.y);
            
            this->radiusOrAngle = style.conic.angle + addAngle;
        }
            break;
        default:
            break;
    }
    this->resolution = glm::vec2(context.width, context.height) * context.contentScale;
}

shader::GradientConstants::GradientConstants()
{
}

} // namespace shader

namespace render {

Diligent::InputLayoutDesc createInputLayoutDesc() {
    Diligent::InputLayoutDesc Desc;
    Diligent::LayoutElement LayoutElems[] =
    {
        // Attribute 0 - vertex position 2D
        Diligent::LayoutElement{0, 0, 2, Diligent::VT_FLOAT32, Diligent::False}
    };
    Desc.LayoutElements = LayoutElems;
    Desc.NumElements = _countof(LayoutElems);
    return Desc;
}

PipelineState::PipelineState(PipelineStateConfiguration conf,
                             Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice)
{
    Diligent::GraphicsPipelineStateCreateInfo PSOCreateInfo;
    PSOCreateInfo.PSODesc.Name = conf.name.c_str();
    PSOCreateInfo.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;
    PSOCreateInfo.GraphicsPipeline.SmplDesc.Count = conf.numSamples;
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = conf.colorBufferFormat;
    PSOCreateInfo.GraphicsPipeline.DSVFormat = conf.depthBufferFormat;
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = Diligent::CULL_MODE_NONE;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.StencilEnable = Diligent::True;
    Diligent::StencilOpDesc StencilDesc;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.FrontFace = StencilDesc;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.BackFace = StencilDesc;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = Diligent::True;
    if(!conf.isClippingMask) {
        PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthFunc =
            Diligent::COMPARISON_FUNC_LESS;
    } else {
        PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthFunc =
            Diligent::COMPARISON_FUNC_ALWAYS;
    }

    Diligent::BlendStateDesc BlendState;
    BlendState.RenderTargets[0].BlendEnable = Diligent::True;
    BlendState.RenderTargets[0].SrcBlend = Diligent::BLEND_FACTOR_SRC_ALPHA;
    BlendState.RenderTargets[0].DestBlend = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
    PSOCreateInfo.GraphicsPipeline.BlendDesc = BlendState;

    PSOCreateInfo.pVS = conf.vertexShader;
    PSOCreateInfo.pPS = conf.pixelShader;

    Diligent::LayoutElement LayoutElems[] =
    {
        // Attribute 0 - vertex position 2D
        Diligent::LayoutElement{0, 0, 2, Diligent::VT_FLOAT32, Diligent::False}
    };
    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElems);
    
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    
    PSO.Release();
    SRB.Release();

    renderDevice->CreateGraphicsPipelineState(PSOCreateInfo, &PSO);

    if(conf.VSConstants != nullptr) {
        PSO->GetStaticVariableByName(Diligent::SHADER_TYPE_VERTEX, "Constants")->
            Set(conf.VSConstants);
    }
    if(conf.PSConstants != nullptr) {
        PSO->GetStaticVariableByName(Diligent::SHADER_TYPE_PIXEL, "Constants")->
            Set(conf.PSConstants);
    }

    PSO->CreateShaderResourceBinding(&SRB, true);
}

PipelineState::PipelineState()
{
}

SolidColorPipelineStates::
SolidColorPipelineStates(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice,
                        Diligent::TEXTURE_FORMAT colorBufferFormat,
                        Diligent::TEXTURE_FORMAT depthBufferFormat,
                        int numSamples) {
    createShaders(renderDevice);
    recreate(renderDevice, colorBufferFormat, depthBufferFormat,
             BlendingMode::Normal,numSamples);
    this->isInitialized = true;
}

void SolidColorPipelineStates::
createShaders(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice) {
    Diligent::ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.UseCombinedTextureSamplers = Diligent::True;
    
    // Create a vertex shader
    {
        ShaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "blazevg vertex shader";
        ShaderCI.Source = shader::VSSource;
        renderDevice->CreateShader(ShaderCI, &VS);

        Diligent::BufferDesc CBDesc;
        CBDesc.Name = "blazevg VS constants CB";
        CBDesc.Size = sizeof(shader::VSConstants);
        CBDesc.Usage = Diligent::USAGE_DYNAMIC;
        CBDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
        CBDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
        renderDevice->CreateBuffer(CBDesc, nullptr, &VSConstants);
    }

    // Create a pixel shader
    {
        ShaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "blazevg solid color pixel shader";
        ShaderCI.Source = shader::solidcol::PSSource;
        renderDevice->CreateShader(ShaderCI, &PS);

        Diligent::BufferDesc CBDesc;
        CBDesc.Name = "blazevg solid color PS constants CB";
        CBDesc.Size = sizeof(shader::solidcol::PSConstants);
        CBDesc.Usage = Diligent::USAGE_DYNAMIC;
        CBDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
        CBDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
        renderDevice->CreateBuffer(CBDesc, nullptr, &PSConstants);
    }
}

void SolidColorPipelineStates::
recreate(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice,
         Diligent::TEXTURE_FORMAT colorBufferFormat,
         Diligent::TEXTURE_FORMAT depthBufferFormat,
         BlendingMode blendingMode,
         int numSamples)
{
    PipelineStateConfiguration conf;
    conf.name = "Normal PSO";
    conf.pixelShader = PS;
    conf.vertexShader = VS;
    conf.PSConstants = PSConstants;
    conf.VSConstants = VSConstants;
    conf.colorBufferFormat = colorBufferFormat;
    conf.depthBufferFormat = depthBufferFormat;
    conf.isClippingMask = false;
    conf.numSamples = numSamples;
    normalPSO = PipelineState(conf, renderDevice);
    conf.name = "Clip PSO";
    conf.isClippingMask = true;
    clipPSO = PipelineState(conf, renderDevice);
}

SolidColorPipelineStates::SolidColorPipelineStates()
{
}

GradientPipelineStates::
GradientPipelineStates(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice,
                      Diligent::TEXTURE_FORMAT colorBufferFormat,
                      Diligent::TEXTURE_FORMAT depthBufferFormat,
                      int numSamples) {
    createShaders(renderDevice);
    recreate(renderDevice, colorBufferFormat, depthBufferFormat,
             BlendingMode::Normal, numSamples);
    this->isInitialized = true;
}

void GradientPipelineStates::
createShaders(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice) {
    Diligent::ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.UseCombinedTextureSamplers = Diligent::True;
    
    // Create a vertex shader
    {
        ShaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "blazevg vertex shader";
        ShaderCI.Source = shader::VSSource;
        renderDevice->CreateShader(ShaderCI, &VS);

        Diligent::BufferDesc CBDesc;
        CBDesc.Name = "blazevg VS constants CB";
        CBDesc.Size = sizeof(shader::VSConstants);
        CBDesc.Usage = Diligent::USAGE_DYNAMIC;
        CBDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
        CBDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
        renderDevice->CreateBuffer(CBDesc, nullptr, &VSConstants);
    }

    // Create a pixel shader
    {
        ShaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "blazevg linear gradient pixel shader";
        ShaderCI.Source = shader::grad::PSSource;
        renderDevice->CreateShader(ShaderCI, &PS);

        Diligent::BufferDesc CBDesc;
        CBDesc.Name = "blazevg linear gradient PS constants CB";
        CBDesc.Size = sizeof(shader::grad::PSConstants);
        CBDesc.Usage = Diligent::USAGE_DYNAMIC;
        CBDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
        CBDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
        renderDevice->CreateBuffer(CBDesc, nullptr, &PSConstants);
    }
}

void GradientPipelineStates::
recreate(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice,
         Diligent::TEXTURE_FORMAT colorBufferFormat,
         Diligent::TEXTURE_FORMAT depthBufferFormat,
         BlendingMode blendingMode,
         int numSamples)
{
    Diligent::GraphicsPipelineStateCreateInfo PSOCreateInfo;
    PSOCreateInfo.PSODesc.Name = "blazevg linear gradient PSO";
    PSOCreateInfo.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;
    PSOCreateInfo.GraphicsPipeline.SmplDesc.Count = numSamples;
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = colorBufferFormat;
    PSOCreateInfo.GraphicsPipeline.DSVFormat = depthBufferFormat;
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = Diligent::CULL_MODE_NONE;
//    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.StencilEnable = Diligent::True;
//    Diligent::StencilOpDesc StencilDesc;
//    StencilDesc.StencilFunc = Diligent::COMPARISON_FUNC_NOT_EQUAL;
//    StencilDesc.StencilPassOp = Diligent::STENCIL_OP_REPLACE;
//    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.FrontFace = StencilDesc;
//    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.BackFace = StencilDesc;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = Diligent::True;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthFunc = Diligent::COMPARISON_FUNC_LESS;

    Diligent::BlendStateDesc BlendState;
    BlendState.RenderTargets[0].BlendEnable = Diligent::True;
    BlendState.RenderTargets[0].SrcBlend = Diligent::BLEND_FACTOR_SRC_ALPHA;
    BlendState.RenderTargets[0].DestBlend = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
    PSOCreateInfo.GraphicsPipeline.BlendDesc = BlendState;

    PSOCreateInfo.pVS = VS;
    PSOCreateInfo.pPS = PS;

    Diligent::LayoutElement LayoutElems[] =
    {
        // Attribute 0 - vertex position 2D
        Diligent::LayoutElement{0, 0, 2, Diligent::VT_FLOAT32, Diligent::False}
    };
    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElems);
    
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    PSO.Release();
    SRB.Release();
    
    renderDevice->CreateGraphicsPipelineState(PSOCreateInfo, &PSO);

    PSO->GetStaticVariableByName(Diligent::SHADER_TYPE_VERTEX, "Constants")->Set(VSConstants);
    PSO->GetStaticVariableByName(Diligent::SHADER_TYPE_PIXEL, "Constants")->Set(PSConstants);

    PSO->CreateShaderResourceBinding(&SRB, true);
}

GradientPipelineStates::GradientPipelineStates()
{
}

Shape::Shape(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice,
             factory::ShapeMesh& mesh) {
    size_t verticesSize = mesh.vertices.size() * sizeof(glm::vec2);
    size_t indicesSize = mesh.indices.size() * sizeof(factory::TriangeIndices);
    
    Diligent::BufferDesc VertBuffDesc;
    VertBuffDesc.Name = "blazevg vertex buffer";
    VertBuffDesc.Usage = Diligent::USAGE_IMMUTABLE;
    VertBuffDesc.BindFlags = Diligent::BIND_VERTEX_BUFFER;
    VertBuffDesc.Size = verticesSize;
    Diligent::BufferData VBData;
    VBData.pData = mesh.vertices.data();
    VBData.DataSize = verticesSize;
    renderDevice->CreateBuffer(VertBuffDesc, &VBData, &this->vertexBuffer);

    Diligent::BufferDesc IndBuffDesc;
    IndBuffDesc.Name = "blazevg index buffer";
    IndBuffDesc.Usage = Diligent::USAGE_IMMUTABLE;
    IndBuffDesc.BindFlags = Diligent::BIND_INDEX_BUFFER;
    IndBuffDesc.Size = indicesSize;
    Diligent::BufferData IBData;
    IBData.pData = mesh.indices.data();
    IBData.DataSize = indicesSize;
    renderDevice->CreateBuffer(IndBuffDesc, &IBData, &this->indexBuffer);
    this->numIndices = (int)mesh.indices.size() * 3;
}

void Shape::draw(DiligentContext& context, Style& style) {
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> deviceCtx = context.mDeviceContext;
    
    Diligent::Uint64   offset = 0;
    Diligent::IBuffer* pBuffs[] = { this->vertexBuffer };
    deviceCtx->SetVertexBuffers(0, 1, pBuffs, &offset,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
        Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
    deviceCtx->SetIndexBuffer(this->indexBuffer, 0,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    glm::mat4 MVP = context.viewProj * math::toMatrix3D(context.matrix);
    
    // Draw the shape in front of other one because depth buffer is enabled.
    // When rendering, depth buffer compares current depth pixel values
    // with those written to it. If they are the same, then it will discard
    // them, so we need to constantly increment the Z coordinate
    MVP = glm::translate(glm::mat4(1.0f),
                         glm::vec3(0.0f, 0.0f, 1.0f -
                                   ((float)context.mShapeDrawCounter + 1.0f) * 0.000001f)
                         ) * MVP;
    
    if(context.mIsClipping) {
        {
            Diligent::MapHelper<shader::VSConstants> CBConstants(deviceCtx,
                                                                 context.mSolidColorPSO
                                                                        .VSConstants,
                                                                 Diligent::MAP_WRITE,
                                                                 Diligent::MAP_FLAG_DISCARD);
            shader::VSConstants c;
            c.MVP = glm::transpose(MVP);
            *CBConstants = c;
        }
        {
            Diligent::MapHelper<shader::solidcol::PSConstants> CBConstants(deviceCtx,
                                                                 context.mSolidColorPSO
                                                                           .PSConstants,
                                                                 Diligent::MAP_WRITE,
                                                                 Diligent::MAP_FLAG_DISCARD);
            shader::solidcol::PSConstants c;
            Color transparent = Color(0.0f, 0.0f, 0.0f, 0.0f);
            c.color = transparent;
            *CBConstants = c;
        }
        deviceCtx->SetPipelineState(context.mSolidColorPSO.clipPSO.PSO);
        deviceCtx->CommitShaderResources(context.mSolidColorPSO.clipPSO.SRB, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    } else {
        switch(style.type) {
        case Style::Type::SolidColor:
        {
            {
                Diligent::MapHelper<shader::VSConstants> CBConstants(deviceCtx,
                                                                     context.mSolidColorPSO
                                                                            .VSConstants,
                                                                     Diligent::MAP_WRITE,
                                                                     Diligent::MAP_FLAG_DISCARD);
                shader::VSConstants c;
                c.MVP = glm::transpose(MVP);
                *CBConstants = c;
            }
            {
                Diligent::MapHelper<shader::solidcol::PSConstants> CBConstants(deviceCtx,
                                                                     context.mSolidColorPSO
                                                                               .PSConstants,
                                                                     Diligent::MAP_WRITE,
                                                                     Diligent::MAP_FLAG_DISCARD);
                shader::solidcol::PSConstants c;
                c.color = style.color;
                *CBConstants = c;
            }
            deviceCtx->SetPipelineState(context.mSolidColorPSO.normalPSO.PSO);
            deviceCtx->CommitShaderResources(context.mSolidColorPSO.normalPSO.SRB, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }
            break;
        case Style::Type::LinearGradient:
        case Style::Type::RadialGradient:
        case Style::Type::ConicGradient:
        {
            {
                Diligent::MapHelper<shader::VSConstants> CBConstants(deviceCtx,
                                                                     context.mGradientPSO
                                                                            .VSConstants,
                                                                     Diligent::MAP_WRITE,
                                                                     Diligent::MAP_FLAG_DISCARD);
                shader::VSConstants c;
                c.MVP = glm::transpose(MVP);
                *CBConstants = c;
            }
            {
                Diligent::MapHelper<shader::grad::PSConstants> CBConstants(deviceCtx,
                                                                     context.mGradientPSO
                                                                               .PSConstants,
                                                                     Diligent::MAP_WRITE,
                                                                     Diligent::MAP_FLAG_DISCARD);
                shader::grad::PSConstants c;
                c.gradient = shader::GradientConstants(style, MVP, context);
                *CBConstants = c;
            }
            deviceCtx->SetPipelineState(context.mGradientPSO.PSO);
            deviceCtx->CommitShaderResources(context.mGradientPSO.SRB, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }
            break;
        default:
            return;
        }
    }

    Diligent::DrawIndexedAttribs DrawAttrs;
    DrawAttrs.IndexType = Diligent::VT_UINT32;
    DrawAttrs.NumIndices = this->numIndices;
    DrawAttrs.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;

    deviceCtx->SetStencilRef(1);
    
    deviceCtx->DrawIndexed(DrawAttrs);
    
    context.mShapeDrawCounter++;
}

struct CharVertex {
    glm::vec2 position;
    glm::vec2 texCoord;
};

CharacterQuad::CharacterQuad(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice,
                             Font::Character& c, int size)
{
    this->advance = c.advance;
    
    glm::vec2 start = glm::vec2(c.planeBounds.left, c.planeBounds.top) * (float)size;
    glm::vec2 end = glm::vec2(c.planeBounds.right, c.planeBounds.bottom) * (float)size;
    
    this->height = end.y - start.y;
    
    glm::vec2 texStart = glm::vec2(c.atlasBounds.left, c.atlasBounds.top);
    glm::vec2 texEnd = glm::vec2(c.atlasBounds.right, c.atlasBounds.bottom);
    
    CharVertex vertices[] = {
        { start, texStart },
        { glm::vec2(end.x, start.y), glm::vec2(texEnd.x, texStart.y) },
        { glm::vec2(end.x, end.y), glm::vec2(texEnd.x, texEnd.y) },
        { glm::vec2(start.x, end.y), glm::vec2(texStart.x, texEnd.y) }
    };
    
    Diligent::BufferDesc VertBuffDesc;
    VertBuffDesc.Name = "blazevg font character vertex buffer";
    VertBuffDesc.Usage = Diligent::USAGE_IMMUTABLE;
    VertBuffDesc.BindFlags = Diligent::BIND_VERTEX_BUFFER;
    VertBuffDesc.Size = sizeof(vertices);
    Diligent::BufferData VBData;
    VBData.pData = vertices;
    VBData.DataSize = sizeof(vertices);
    renderDevice->CreateBuffer(VertBuffDesc, &VBData, &this->vertexBuffer);
}

CharacterQuad::CharacterQuad()
{
}

GlyphMSDFShaders::GlyphMSDFShaders(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice) {
    Diligent::ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.UseCombinedTextureSamplers = Diligent::True;
    
    // Create a vertex shader
    {
        ShaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "blazevg glyph msdf vertex shader";
        ShaderCI.Source = shader::msdf::GlyphVSSource;
        renderDevice->CreateShader(ShaderCI, &VS);

        Diligent::BufferDesc CBDesc;
        CBDesc.Name = "blazevg VS constants CB";
        CBDesc.Size = sizeof(shader::VSConstants);
        CBDesc.Usage = Diligent::USAGE_DYNAMIC;
        CBDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
        CBDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
        renderDevice->CreateBuffer(CBDesc, nullptr, &VSConstants);
    }

    // Create a pixel shader
    {
        ShaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "blazevg glyph msdf pixel shader";
        ShaderCI.Source = shader::msdf::PSSource;
        renderDevice->CreateShader(ShaderCI, &PS);

        Diligent::BufferDesc CBDesc;
        CBDesc.Name = "blazevg glyph msdf PS constants CB";
        CBDesc.Size = sizeof(shader::msdf::PSConstants);
        CBDesc.Usage = Diligent::USAGE_DYNAMIC;
        CBDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
        CBDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
        renderDevice->CreateBuffer(CBDesc, nullptr, &PSConstants);
    }
    
    Diligent::BufferDesc IndBuffDesc;
    IndBuffDesc.Name = "blazevg glyph quad index buffer";
    IndBuffDesc.Usage = Diligent::USAGE_IMMUTABLE;
    IndBuffDesc.BindFlags = Diligent::BIND_INDEX_BUFFER;
    IndBuffDesc.Size = sizeof(GlyphQuadIndices);
    Diligent::BufferData IBData;
    IBData.pData = GlyphQuadIndices;
    IBData.DataSize = sizeof(GlyphQuadIndices);
    renderDevice->CreateBuffer(IndBuffDesc, &IBData, &quadIndexBuffer);
}

GlyphMSDFShaders::GlyphMSDFShaders()
{
}

} // namespace render

void DiligentContext::convexFill() {
    this->assertDrawingIsBegan();
    factory::ShapeMesh mesh = internalConvexFill();
    render::Shape shape = render::Shape(mRenderDevice, mesh);
    shape.draw(*this, this->fillStyle);
}

void DiligentContext::fill() {
    this->assertDrawingIsBegan();
    factory::ShapeMesh mesh = internalFill();
    render::Shape shape = render::Shape(mRenderDevice, mesh);
    shape.draw(*this, this->fillStyle);
    // this->debugTriangulate(mesh.vertices, true);
}

void DiligentContext::stroke() {
    this->assertDrawingIsBegan();
    factory::ShapeMesh mesh = internalStroke();
    render::Shape shape = render::Shape(mRenderDevice, mesh);
    shape.draw(*this, this->strokeStyle);
}

void DiligentContext::textFill(std::wstring str, float x, float y) {
    this->assertDrawingIsBegan();
    assert(this->font != nullptr);
    
    float scale = fontSize / (float)font->size;
    glm::vec2 pos = glm::vec2(x, y);
    glm::mat4 transform = this->viewProj * math::toMatrix3D(this->matrix);
    
    DiligentFont* fnt = static_cast<DiligentFont*>(this->font);
    fnt->recreatePipelineState(mColorBufferFormat,
                               mDepthBufferFormat,
                               mNumSamples);
    
    for (int i = 0; i < str.size(); i++)
    {
        int symbol = str[i];
        if (symbol == '\n')
        {
            pos.y += font->lineHeight * scale;
            pos.x = x;
            continue;
        }
        
        render::CharacterQuad& character = fnt->chars[symbol];

        if (symbol == ' ')
        {
            pos.x += (float)character.advance * scale;
            continue;
        }
        
        if (character.vertexBuffer == nullptr)
            continue;
        
        glm::mat4 MVP = transform * glm::scale(
            glm::translate(glm::identity<glm::mat4>(), glm::vec3(pos, 0.0f)),
            glm::vec3(scale));
        {
            Diligent::MapHelper<shader::VSConstants> CBConstants(mDeviceContext,
                                                                 mGlyphShaders.VSConstants,
                                                                 Diligent::MAP_WRITE,
                                                                 Diligent::MAP_FLAG_DISCARD);
            shader::VSConstants c;
            c.MVP = glm::transpose(MVP);
            *CBConstants = c;
        }
        {
            Diligent::MapHelper<shader::msdf::PSConstants> CBConstants(mDeviceContext,
                                                                       mGlyphShaders.PSConstants,
                                                                       Diligent::MAP_WRITE,
                                                                       Diligent::MAP_FLAG_DISCARD);
            shader::msdf::PSConstants c;
            c.color = this->fillStyle.color;
            c.distanceRange = (float)fnt->distanceRange;
            c.isLinearGradient = false;
            if(this->fillStyle.type == Style::Type::LinearGradient) {
                c.isLinearGradient = true;
                c.gradient = shader::GradientConstants(this->fillStyle, MVP, *this);
            }
            *CBConstants = c;
        }

        Diligent::Uint64   offset = 0;
        Diligent::IBuffer* pBuffs[] = { character.vertexBuffer };
        mDeviceContext->SetVertexBuffers(0, 1, pBuffs, &offset,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
        mDeviceContext->SetIndexBuffer(mGlyphShaders.quadIndexBuffer, 0,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        mDeviceContext->SetPipelineState(fnt->PSO);

        mDeviceContext->CommitShaderResources(fnt->SRB,
                                       Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        Diligent::DrawIndexedAttribs DrawAttrs;
        DrawAttrs.IndexType = Diligent::VT_UINT32;
        DrawAttrs.NumIndices = _countof(render::GlyphQuadIndices);
        DrawAttrs.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;

        mDeviceContext->DrawIndexed(DrawAttrs);

        pos.x += (float)character.advance * scale;
    }
}

float tAtLengthClosed(float length, std::vector<float> lengths, float fullLength, bool closed) {
    if(closed) {
        float cyclesLength = 0;
        int cycles = (int)floorf(length / fullLength);
        cyclesLength = cycles * fullLength;
        length -= cyclesLength;
        if(length < 0.0f)
            length = fullLength - length;
    }
    return factory::tAtLength(length, lengths);
}

void DiligentContext::textFillOnPath(std::wstring str, float x, float y) {
    this->assertDrawingIsBegan();
    assert(this->font != nullptr);
    
    std::vector<glm::vec2> polyline = this->toOnePolyline(mPolylines);
    
    float scale = fontSize / (float)font->size;
    float length = x;
    glm::mat4 transform = this->viewProj * math::toMatrix3D(this->matrix);
    
    std::vector<float> polylineLengths = factory::measurePolyline(polyline);
    float polylineLength = 0;
    for(float len : polylineLengths)
        polylineLength += len;
    
    bool closed = mIsPolylineClosed;
    
    DiligentFont* fnt = static_cast<DiligentFont*>(this->font);
    fnt->recreatePipelineState(mColorBufferFormat,
                               mDepthBufferFormat,
                               mNumSamples);
    
    for (int i = 0; i < str.size(); i++)
    {
        int symbol = str[i];
        if (symbol == '\n')
        {
            continue;
        }
        
        render::CharacterQuad& character = fnt->chars[symbol];

        if (symbol == ' ')
        {
            length += (float)character.advance * scale;
            continue;
        }
        
        if (character.vertexBuffer == nullptr)
            continue;
        
        float t = tAtLengthClosed(length, polylineLengths, polylineLength, closed);
        glm::vec2 pos = factory::getPointAtT(polyline, t);
        
        float t2 = tAtLengthClosed(length + (float)character.advance, polylineLengths,
                                   polylineLength, closed);
        glm::vec2 pos2 = factory::getPointAtT(polyline, t2);
        
        if(!closed) {
            if(length < 0) {
                glm::vec2 origin = polyline.at(0);
                glm::vec2 dir = glm::normalize(polyline.at(1) - polyline.at(0));
                pos = origin + dir * length;
            }
            if(length >= polylineLength) {
                glm::vec2 origin = polyline.back();
                glm::vec2 dir = glm::normalize(polyline.back() - polyline.at(polyline.size() - 2));
                float lengthFromOrigin = length - polylineLength;
                pos = origin + dir * lengthFromOrigin;
                pos2 = origin + dir * (lengthFromOrigin + (float)character.advance);
            }
        }
        
        glm::vec2 relativePos = pos2 - pos;
        float angle = atan2f(relativePos.y, relativePos.x);
        
        float upper = -(float)fnt->baseline * scale + y;
        
        glm::mat4 MVP = transform * glm::scale(glm::mat4(1.0f), glm::vec3(scale)) *
            glm::translate(glm::mat4(1.0f), glm::vec3(pos, 0.0f)) *
            glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0, 0, 1)) *
            glm::translate(glm::mat4(1.0f), glm::vec3(glm::vec2(0.0f, upper), 0.0f));
        {
            Diligent::MapHelper<shader::VSConstants> CBConstants(mDeviceContext,
                                                                 mGlyphShaders.VSConstants,
                                                                 Diligent::MAP_WRITE,
                                                                 Diligent::MAP_FLAG_DISCARD);
            shader::VSConstants c;
            c.MVP = glm::transpose(MVP);
            *CBConstants = c;
        }
        {
            Diligent::MapHelper<shader::msdf::PSConstants> CBConstants(mDeviceContext,
                                                                       mGlyphShaders.PSConstants,
                                                                       Diligent::MAP_WRITE,
                                                                       Diligent::MAP_FLAG_DISCARD);
            shader::msdf::PSConstants c;
            c.color = this->fillStyle.color;
            c.distanceRange = (float)fnt->distanceRange;
            c.isLinearGradient = false;
            if(this->fillStyle.type == Style::Type::LinearGradient) {
                c.isLinearGradient = true;
                c.gradient = shader::GradientConstants(this->fillStyle, MVP, *this);
            }
            *CBConstants = c;
        }

        Diligent::Uint64   offset = 0;
        Diligent::IBuffer* pBuffs[] = { character.vertexBuffer };
        mDeviceContext->SetVertexBuffers(0, 1, pBuffs, &offset,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
            Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
        mDeviceContext->SetIndexBuffer(mGlyphShaders.quadIndexBuffer, 0,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        mDeviceContext->SetPipelineState(fnt->PSO);

        mDeviceContext->CommitShaderResources(fnt->SRB,
                                       Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        Diligent::DrawIndexedAttribs DrawAttrs;
        DrawAttrs.IndexType = Diligent::VT_UINT32;
        DrawAttrs.NumIndices = _countof(render::GlyphQuadIndices);
        DrawAttrs.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;

        mDeviceContext->DrawIndexed(DrawAttrs);

        length += (float)character.advance * scale;
    }
}

float DiligentContext::measureTextWidth(std::wstring str) {
    assert(this->font != nullptr);
    
    float scale = fontSize / (float)font->size;
    float width = 0.0f;
    
    DiligentFont* fnt = static_cast<DiligentFont*>(this->font);
    for (int i = 0; i < str.size(); i++)
    {
        int symbol = str[i];
        if (symbol == '\n')
            return width;
        
        render::CharacterQuad& character = fnt->chars[symbol];

        width += (float)character.advance * scale;
    }
    return width;
}
float DiligentContext::measureTextHeight() {
    assert(this->font != nullptr);
    
    float scale = fontSize / (float)font->size;
    return this->font->lineHeight * scale;
}

void DiligentContext::loadFontFromMemory(std::string& json,
                        std::string fontName,
                        void* imageData,
                        int width,
                        int height,
                        int numChannels)
{
    DiligentFont* font = new DiligentFont(mRenderDevice,
                                          mColorBufferFormat,
                                          mDepthBufferFormat,
                                          this->mNumSamples,
                                          json,
                                          imageData,
                                          width,
                                          height,
                                          numChannels,
                                          *this);
    this->fonts[fontName] = static_cast<Font*>(font);
}

DiligentFont::DiligentFont(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice,
                           Diligent::TEXTURE_FORMAT colorBufferFormat,
                           Diligent::TEXTURE_FORMAT depthBufferFormat,
                           int numSamples,
                           std::string& json,
                           void* imageData,
                           int width,
                           int height,
                           int numChannels,
                           DiligentContext& context):
    mRenderDevice(renderDevice),
    mContext(context)
{
    createTexture(colorBufferFormat, imageData, width, height, numChannels);
    recreatePipelineState(colorBufferFormat, depthBufferFormat, numSamples);
    this->parseJson(json);
}

void* convertRGBToRGBA(char* imageData,
                       int width,
                       int height)
{
    char* newImage = new char[width * height * 4];
    for(size_t i = 0; i < width * height; i++) {
        size_t oldIndex = i * 3LL;
        size_t newIndex = i * 4LL;
        newImage[newIndex] = imageData[oldIndex]; // Red
        newImage[newIndex + 1LL] = imageData[oldIndex + 1LL]; // Green
        newImage[newIndex + 2LL] = imageData[oldIndex + 2LL]; // Blue
        newImage[newIndex + 3LL] = 255; // Alpha
    }
    return newImage;
}

void DiligentFont::createTexture(Diligent::TEXTURE_FORMAT colorBufferFormat,
                                 void* imageData,
                                 int width,
                                 int height,
                                 int numChannels)
{
    assert(imageData != nullptr);
    
    int RGBAStride = width * numChannels;
    if(numChannels == 3) {
        RGBAStride = width * 4;
        imageData = convertRGBToRGBA((char*)imageData, width, height);
    }
    // RGBAStride = (RGBAStride + 3) & (-4);

    Diligent::TextureSubResData SubRes;
    SubRes.Stride = RGBAStride;
    SubRes.pData = imageData;

    Diligent::TextureData TexData;
    TexData.NumSubresources = 1;
    TexData.pSubResources = &SubRes;

    Diligent::TextureDesc TexDesc;
    TexDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
    TexDesc.Width = width;
    TexDesc.Height = height;
    TexDesc.MipLevels = 1;
    TexDesc.Format = colorBufferFormat;
    TexDesc.BindFlags = Diligent::BIND_SHADER_RESOURCE;
    mRenderDevice->CreateTexture(TexDesc, &TexData, &mTexture);
    textureSRV = mTexture->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    
    if(numChannels == 3) {
        delete[] (char*)imageData;
    }
}

void DiligentFont::recreatePipelineState(Diligent::TEXTURE_FORMAT colorBufferFormat,
                                         Diligent::TEXTURE_FORMAT depthBufferFormat,
                                         int numSamples) {
    if(mColorBufferFormat == colorBufferFormat &&
       mDepthBufferFormat == depthBufferFormat &&
       mNumSamples == numSamples) {
        return;
    }
    
    mColorBufferFormat = colorBufferFormat;
    mDepthBufferFormat = depthBufferFormat;
    mNumSamples = numSamples;
    
    Diligent::GraphicsPipelineStateCreateInfo PSOCreateInfo;
    PSOCreateInfo.PSODesc.Name = "blazevg font PSO";
    PSOCreateInfo.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;
    PSOCreateInfo.GraphicsPipeline.SmplDesc.Count = numSamples;
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = colorBufferFormat;
    PSOCreateInfo.GraphicsPipeline.DSVFormat = depthBufferFormat;
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = Diligent::CULL_MODE_NONE;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.StencilEnable = Diligent::True;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = Diligent::False;

    Diligent::BlendStateDesc BlendState;
    BlendState.RenderTargets[0].BlendEnable = Diligent::True;
    BlendState.RenderTargets[0].SrcBlend = Diligent::BLEND_FACTOR_SRC_ALPHA;
    BlendState.RenderTargets[0].DestBlend = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
    PSOCreateInfo.GraphicsPipeline.BlendDesc = BlendState;

    PSOCreateInfo.pVS = mContext.mGlyphShaders.VS;
    PSOCreateInfo.pPS = mContext.mGlyphShaders.PS;

    Diligent::LayoutElement LayoutElems[] =
    {
        // Attribute 0 - vertex position
        Diligent::LayoutElement{0, 0, 2, Diligent::VT_FLOAT32, Diligent::False},
        // Attribute 1 - texture coordinate
        Diligent::LayoutElement{1, 0, 2, Diligent::VT_FLOAT32, Diligent::False}
    };
    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElems);
    
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    
    Diligent::ShaderResourceVariableDesc variables[] =
    {
        { Diligent::SHADER_TYPE_PIXEL, "g_Texture", Diligent::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE }
    };
    PSOCreateInfo.PSODesc.ResourceLayout.Variables = variables;
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(variables);

    Diligent::ImmutableSamplerDesc samplers[] =
    {
        { Diligent::SHADER_TYPE_PIXEL, "g_Texture", Diligent::Sam_LinearClamp }
    };
    PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers = samplers;
    PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(samplers);

    PSO.Release();
    SRB.Release();
    
    mRenderDevice->CreateGraphicsPipelineState(PSOCreateInfo, &PSO);

    PSO->GetStaticVariableByName(Diligent::SHADER_TYPE_VERTEX, "Constants")->
        Set(mContext.mGlyphShaders.VSConstants);
    PSO->GetStaticVariableByName(Diligent::SHADER_TYPE_PIXEL, "Constants")->
        Set(mContext.mGlyphShaders.PSConstants);

    PSO->CreateShaderResourceBinding(&SRB, true);
    SRB->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "g_Texture")->
        Set(textureSRV);
}

void DiligentFont::loadCharacter(Character& character) {
    this->chars[character.unicode] = render::CharacterQuad(mRenderDevice, character, this->size);
}

void DiligentContext::
setupPipelineStates(Diligent::TEXTURE_FORMAT colorBufferFormat,
                    Diligent::TEXTURE_FORMAT depthBufferFormat,
                    int numSamples) {
    if(mColorBufferFormat == colorBufferFormat &&
       mDepthBufferFormat == depthBufferFormat &&
       mNumSamples == numSamples) {
         return;
    }
    
    mColorBufferFormat = colorBufferFormat;
    mDepthBufferFormat = depthBufferFormat;
    mNumSamples = numSamples;
    
    mSolidColorPSO.recreate(mRenderDevice, mColorBufferFormat, mDepthBufferFormat,
                            this->blendingMode, numSamples);
    mGradientPSO.recreate(mRenderDevice, mColorBufferFormat, mDepthBufferFormat,
                          this->blendingMode, numSamples);
}

void DiligentContext::specifyTextureViews(Diligent::ITextureView* RTV,
                         Diligent::ITextureView* DSV) {
    mDSV = DSV;
}

void DiligentContext::beginClip() {
    if(mDSV == nullptr) {
        std::cerr << "blazevg: Error: Depth-stencil view is not specified. Please specify with specifyTextureViews()" << std::endl;
        exit(-1);
    }
    mIsClipping = true;
    mDeviceContext->ClearDepthStencil(mDSV, Diligent::CLEAR_DEPTH_FLAG, 0.0f, 0,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void DiligentContext::endClip() {
    mIsClipping = false;
}

void DiligentContext::clearClip() {
    mDeviceContext->ClearDepthStencil(mDSV, Diligent::CLEAR_DEPTH_FLAG, 1.0f, 0,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

} // namespace bvg
