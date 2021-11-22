#include <backends/diligent.hh>
#include <Graphics/GraphicsTools/interface/CommonlyUsedStates.h>
#include <Graphics/GraphicsTools/interface/MapHelper.hpp>

namespace bvg {

DiligentContext::DiligentContext(float width, float height,
                Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice,
                Diligent::RefCntAutoPtr<Diligent::IDeviceContext> deviceContext,
                Diligent::TEXTURE_FORMAT colorBufferFormat,
                Diligent::TEXTURE_FORMAT depthBufferFormat):
    Context(width, height),
    mRenderDevice(renderDevice),
    mDeviceContext(deviceContext),
    mColorBufferFormat(colorBufferFormat),
    mDepthBufferFormat(depthBufferFormat)
{
    initPipelineState();
}

DiligentContext::DiligentContext()
{
}

void DiligentContext::initPipelineState() {
    mSolidColorPSO = render::SolidColorPipelineState(mRenderDevice,
                                                     mColorBufferFormat,
                                                     mDepthBufferFormat);
    
    mLinearGradientPSO = render::LinearGradientPipelineState(mRenderDevice,
                                                     mColorBufferFormat,
                                                     mDepthBufferFormat);
}

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

SolidColorPipelineState::
SolidColorPipelineState(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice,
                        Diligent::TEXTURE_FORMAT colorBufferFormat,
                        Diligent::TEXTURE_FORMAT depthBufferFormat) {
    createShaders(renderDevice);
    recreate(renderDevice, colorBufferFormat, depthBufferFormat);
    this->isInitialized = true;
}

void SolidColorPipelineState::
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

void SolidColorPipelineState::
recreate(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice,
         Diligent::TEXTURE_FORMAT colorBufferFormat,
         Diligent::TEXTURE_FORMAT depthBufferFormat)
{
    Diligent::GraphicsPipelineStateCreateInfo PSOCreateInfo;
    PSOCreateInfo.PSODesc.Name = "blazevg solid color PSO";
    PSOCreateInfo.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;
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

    renderDevice->CreateGraphicsPipelineState(PSOCreateInfo, &PSO);

    PSO->GetStaticVariableByName(Diligent::SHADER_TYPE_VERTEX, "Constants")->Set(VSConstants);
    PSO->GetStaticVariableByName(Diligent::SHADER_TYPE_PIXEL, "Constants")->Set(PSConstants);

    PSO->CreateShaderResourceBinding(&SRB, true);
}

SolidColorPipelineState::SolidColorPipelineState()
{
}

LinearGradientPipelineState::
LinearGradientPipelineState(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice,
                        Diligent::TEXTURE_FORMAT colorBufferFormat,
                        Diligent::TEXTURE_FORMAT depthBufferFormat) {
    createShaders(renderDevice);
    recreate(renderDevice, colorBufferFormat, depthBufferFormat);
    this->isInitialized = true;
}

void LinearGradientPipelineState::
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
        ShaderCI.Source = shader::lingrad::PSSource;
        renderDevice->CreateShader(ShaderCI, &PS);

        Diligent::BufferDesc CBDesc;
        CBDesc.Name = "blazevg linear gradient PS constants CB";
        CBDesc.Size = sizeof(shader::lingrad::PSConstants);
        CBDesc.Usage = Diligent::USAGE_DYNAMIC;
        CBDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
        CBDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
        renderDevice->CreateBuffer(CBDesc, nullptr, &PSConstants);
    }
}

void LinearGradientPipelineState::
recreate(Diligent::RefCntAutoPtr<Diligent::IRenderDevice> renderDevice,
         Diligent::TEXTURE_FORMAT colorBufferFormat,
         Diligent::TEXTURE_FORMAT depthBufferFormat)
{
    Diligent::GraphicsPipelineStateCreateInfo PSOCreateInfo;
    PSOCreateInfo.PSODesc.Name = "blazevg linear gradient PSO";
    PSOCreateInfo.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;
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

    renderDevice->CreateGraphicsPipelineState(PSOCreateInfo, &PSO);

    PSO->GetStaticVariableByName(Diligent::SHADER_TYPE_VERTEX, "Constants")->Set(VSConstants);
    PSO->GetStaticVariableByName(Diligent::SHADER_TYPE_PIXEL, "Constants")->Set(PSConstants);

    PSO->CreateShaderResourceBinding(&SRB, true);
}

LinearGradientPipelineState::LinearGradientPipelineState()
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
            deviceCtx->SetPipelineState(context.mSolidColorPSO.PSO);
            deviceCtx->CommitShaderResources(context.mSolidColorPSO.SRB, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }
            break;
        case Style::Type::LinearGradient:
        {
            {
                Diligent::MapHelper<shader::VSConstants> CBConstants(deviceCtx,
                                                                     context.mLinearGradientPSO
                                                                            .VSConstants,
                                                                     Diligent::MAP_WRITE,
                                                                     Diligent::MAP_FLAG_DISCARD);
                shader::VSConstants c;
                c.MVP = glm::transpose(MVP);
                *CBConstants = c;
            }
            {
                Diligent::MapHelper<shader::lingrad::PSConstants> CBConstants(deviceCtx,
                                                                     context.mLinearGradientPSO
                                                                               .PSConstants,
                                                                     Diligent::MAP_WRITE,
                                                                     Diligent::MAP_FLAG_DISCARD);
                shader::lingrad::PSConstants c;
                c.startColor = style.gradientStartColor;
                c.endColor = style.gradientEndColor;
                c.startPos = glm::vec2(MVP * glm::vec4(style.gradientStartX,
                                                               style.gradientStartY,
                                                               0.0f, 1.0f));
                c.endPos = glm::vec2(MVP * glm::vec4(style.gradientEndX,
                                                             style.gradientEndY,
                                                             0.0f, 1.0f));
                // Convert range (-1.0, 1.0) to (0.0, 1.0)
                // and invert Y coordinate
                c.startPos = (c.startPos + 1.0f) / 2.0f;
                c.endPos = (c.endPos + 1.0f) / 2.0f;
                c.startPos.y = 1.0f - c.startPos.y;
                c.endPos.y = 1.0f - c.endPos.y;
                c.resolution = glm::vec2(context.width, context.height) * context.contentScale;
                *CBConstants = c;
            }
            deviceCtx->SetPipelineState(context.mLinearGradientPSO.PSO);
            deviceCtx->CommitShaderResources(context.mLinearGradientPSO.SRB, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }
            break;
        default:
            return;
    }

    Diligent::DrawIndexedAttribs DrawAttrs;
    DrawAttrs.IndexType = Diligent::VT_UINT32;
    DrawAttrs.NumIndices = this->numIndices;
    DrawAttrs.Flags = Diligent::DRAW_FLAG_VERIFY_ALL;

    deviceCtx->DrawIndexed(DrawAttrs);
}

} // namespace render

void DiligentContext::convexFill() {
    factory::ShapeMesh mesh = internalConvexFill();
    render::Shape shape = render::Shape(mRenderDevice, mesh);
    shape.draw(*this, this->fillStyle);
}

void DiligentContext::stroke() {
    factory::ShapeMesh mesh = internalStroke();
    render::Shape shape = render::Shape(mRenderDevice, mesh);
    shape.draw(*this, this->strokeStyle);
}

} // namespace bvg
