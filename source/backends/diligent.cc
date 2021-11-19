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
    Diligent::GraphicsPipelineStateCreateInfo PSOCreateInfo;
    PSOCreateInfo.PSODesc.Name = "blazevg PSO";
    PSOCreateInfo.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = mColorBufferFormat;
    PSOCreateInfo.GraphicsPipeline.DSVFormat = mDepthBufferFormat;
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = Diligent::CULL_MODE_NONE;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = Diligent::False;

    Diligent::BlendStateDesc BlendState;
    BlendState.RenderTargets[0].BlendEnable = Diligent::True;
    BlendState.RenderTargets[0].SrcBlend = Diligent::BLEND_FACTOR_SRC_ALPHA;
    BlendState.RenderTargets[0].DestBlend = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
    PSOCreateInfo.GraphicsPipeline.BlendDesc = BlendState;

    Diligent::ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.UseCombinedTextureSamplers = Diligent::True;

    // Create a vertex shader
    Diligent::RefCntAutoPtr<Diligent::IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "blazevg vertex shader";
        ShaderCI.Source = shader::VSSource;
        mRenderDevice->CreateShader(ShaderCI, &pVS);

        Diligent::BufferDesc CBDesc;
        CBDesc.Name = "VS constants CB";
        CBDesc.Size = sizeof(shader::VSConstants);
        CBDesc.Usage = Diligent::USAGE_DYNAMIC;
        CBDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
        CBDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
        mRenderDevice->CreateBuffer(CBDesc, nullptr, &mVSConstants);
    }

    // Create a pixel shader
    Diligent::RefCntAutoPtr<Diligent::IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint = "main";
        ShaderCI.Desc.Name = "blazevg pixel shader";
        ShaderCI.Source = shader::PSSource;
        mRenderDevice->CreateShader(ShaderCI, &pPS);

        Diligent::BufferDesc CBDesc;
        CBDesc.Name = "PS constants CB";
        CBDesc.Size = sizeof(shader::PSConstants);
        CBDesc.Usage = Diligent::USAGE_DYNAMIC;
        CBDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER;
        CBDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
        mRenderDevice->CreateBuffer(CBDesc, nullptr, &mPSConstants);
    }

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    Diligent::LayoutElement LayoutElems[] =
    {
        // Attribute 0 - vertex position 2D
        Diligent::LayoutElement{0, 0, 2, Diligent::VT_FLOAT32, Diligent::False}
    };
    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElems);
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = Diligent::SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    mRenderDevice->CreateGraphicsPipelineState(PSOCreateInfo, &mPSO);

    mPSO->GetStaticVariableByName(Diligent::SHADER_TYPE_VERTEX, "Constants")->Set(mVSConstants);
    mPSO->GetStaticVariableByName(Diligent::SHADER_TYPE_PIXEL, "Constants")->Set(mPSConstants);

    mPSO->CreateShaderResourceBinding(&mSRB, true);
}

namespace render {

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
                                                             context.mVSConstants,
                                                             Diligent::MAP_WRITE,
                                                             Diligent::MAP_FLAG_DISCARD);
        shader::VSConstants c;
        c.MVP = glm::transpose(context.MVP);
        *CBConstants = c;
    }
    {
        Diligent::MapHelper<shader::PSConstants> CBConstants(deviceCtx,
                                                             context.mPSConstants,
                                                             Diligent::MAP_WRITE,
                                                             Diligent::MAP_FLAG_DISCARD);
        shader::PSConstants c;
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

    deviceCtx->SetPipelineState(context.mPSO);

    deviceCtx->CommitShaderResources(context.mSRB, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

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
    shape.draw(*this, this->fillStyle);
}

} // namespace bvg
