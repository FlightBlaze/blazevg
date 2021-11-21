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
        ShaderCI.Desc.Name = "blazevg pixel shader";
        ShaderCI.Source = shader::solidcol::PSSource;
        renderDevice->CreateShader(ShaderCI, &PS);

        Diligent::BufferDesc CBDesc;
        CBDesc.Name = "blazevg PS constants CB";
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

void Shape::draw(DiligentContext& context, Style style) {
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> deviceCtx = context.mDeviceContext;
    {
        Diligent::MapHelper<shader::VSConstants> CBConstants(deviceCtx,
                                                             context.mSolidColorPSO.VSConstants,
                                                             Diligent::MAP_WRITE,
                                                             Diligent::MAP_FLAG_DISCARD);
        shader::VSConstants c;
        c.MVP = glm::transpose(context.MVP);
        *CBConstants = c;
    }
    {
        Diligent::MapHelper<shader::solidcol::PSConstants> CBConstants(deviceCtx,
                                                             context.mSolidColorPSO.PSConstants,
                                                             Diligent::MAP_WRITE,
                                                             Diligent::MAP_FLAG_DISCARD);
        shader::solidcol::PSConstants c;
        c.color = style.color;
        *CBConstants = c;
    }

    Diligent::Uint64   offset = 0;
    Diligent::IBuffer* pBuffs[] = { this->vertexBuffer };
    deviceCtx->SetVertexBuffers(0, 1, pBuffs, &offset,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
        Diligent::SET_VERTEX_BUFFERS_FLAG_RESET);
    deviceCtx->SetIndexBuffer(this->indexBuffer, 0,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    deviceCtx->SetPipelineState(context.mSolidColorPSO.PSO);

    deviceCtx->CommitShaderResources(context.mSolidColorPSO.SRB, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

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
