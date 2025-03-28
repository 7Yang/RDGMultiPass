#include "Rendering/MultiPassExample.h"
#include "Engine/TextureRenderTarget2D.h"

#include "GlobalShader.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "RHIStaticStates.h"
#include "ShaderParameterUtils.h"
#include "PipelineStateCache.h"
#include "PixelShaderUtils.h"

namespace MultiPassExample
{
    struct FGPUOutput
    {
        float Data1[4];
        float Data2[4];
    };

	TGlobalResource<FTextureVertexDeclaration> GTextureVertexDeclaration;
	TGlobalResource<FRectangleVertexBuffer>    GRectangleVertexBuffer;
	TGlobalResource<FRectangleIndexBuffer>     GRectangleIndexBuffer;

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	class FSimpleRDGComputeShader : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FSimpleRDGComputeShader);
		SHADER_USE_PARAMETER_STRUCT(FSimpleRDGComputeShader, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		    SHADER_PARAMETER_STRUCT_REF(FSimpleUniformStructParameters, UniformStructParameters)
            SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FGPUOutput>, GPUOutput)
		    SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutTexture)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters &Parameters)
		{
            return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}
	};

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	class FSimpleRDGGlobalShader : public FGlobalShader
	{
	public:
		SHADER_USE_PARAMETER_STRUCT(FSimpleRDGGlobalShader, FGlobalShader);

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		    SHADER_PARAMETER_STRUCT_REF(FSimpleUniformStructParameters, SimpleUniform)
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, Texture)
		    SHADER_PARAMETER_SAMPLER(SamplerState,  TextureSampler)
		    RENDER_TARGET_BINDING_SLOTS()
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters &Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES3_1);
		}
	};

	class FSimpleRDGVertexShader : public FSimpleRDGGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FSimpleRDGVertexShader);

		FSimpleRDGVertexShader() {}

		FSimpleRDGVertexShader(const ShaderMetaType::CompiledShaderInitializerType &Initializer) : FSimpleRDGGlobalShader(Initializer) {}
	};

	class FSimpleRDGPixelShader : public FSimpleRDGGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FSimpleRDGPixelShader);

		FSimpleRDGPixelShader() {}

		FSimpleRDGPixelShader(const ShaderMetaType::CompiledShaderInitializerType &Initializer) : FSimpleRDGGlobalShader(Initializer) {}
	};

	IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FSimpleUniformStructParameters, "UniformStructParameters");
	IMPLEMENT_GLOBAL_SHADER(FSimpleRDGComputeShader, "/MultiPass/Private/SimpleComputeShader.usf", "MainCS", SF_Compute);
	IMPLEMENT_GLOBAL_SHADER(FSimpleRDGVertexShader,  "/MultiPass/Private/SimplePixelShader.usf",   "MainVS", SF_Vertex);
	IMPLEMENT_GLOBAL_SHADER(FSimpleRDGPixelShader,   "/MultiPass/Private/SimplePixelShader.usf",   "MainPS", SF_Pixel);

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	void RDGMultiPass(FRHICommandListImmediate& RHIImmCmdList, FTexture2DRHIRef OutRenderTargetRHI)
	{
		check(IsInRenderingThread());
		FRDGBuilder GraphBuilder(RHIImmCmdList);

		// Pass1: 将计算着色器输出到纹理
		const FRDGTextureDesc& ComputeRenderTargetDesc = FRDGTextureDesc::Create2D(OutRenderTargetRHI->GetSizeXY(), OutRenderTargetRHI->GetFormat(), FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_UAV);
		FRDGTextureRef RDGComputeRenderTarget  = GraphBuilder.CreateTexture(ComputeRenderTargetDesc, TEXT("ComputeRDGRenderTarget"));
		FRDGTextureUAVRef OutComputeTextureRef = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(RDGComputeRenderTarget));

        int32 ArrayLength = (OutRenderTargetRHI->GetSizeX() / 32) * (OutRenderTargetRHI->GetSizeY() / 32);
        FRDGBufferRef GPUOutputBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FGPUOutput), ArrayLength), TEXT("GPUOutput"));

		FSimpleUniformStructParameters StructParameters;
        StructParameters.Color1 = FLinearColor(1.f, 0.f, 0.f, 1.f);
        StructParameters.Color2 = FLinearColor(0.f, 1.f, 0.f, 1.f);
        StructParameters.Color3 = FLinearColor(1.f, 0.f, 1.f, 1.f);
        StructParameters.Color4 = FLinearColor(1.f, 0.f, 0.f, 1.f);
        StructParameters.ColorIndex = 1;

		FSimpleRDGComputeShader::FParameters* ComputeShaderParameters = GraphBuilder.AllocParameters<FSimpleRDGComputeShader::FParameters>();
		ComputeShaderParameters->UniformStructParameters = TUniformBufferRef<FSimpleUniformStructParameters>::CreateUniformBufferImmediate(StructParameters, UniformBuffer_SingleFrame);
        ComputeShaderParameters->GPUOutput               = GraphBuilder.CreateUAV(GPUOutputBuffer);
		ComputeShaderParameters->OutTexture              = OutComputeTextureRef;

        FRDGBufferRef GPUDuplicatedBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FGPUOutput), ArrayLength), TEXT("DuplicatedBuffer"));
		GraphBuilder.AddPass(RDG_EVENT_NAME("RDGComputeShader"), ComputeShaderParameters, ERDGPassFlags::Compute, [ComputeShaderParameters, OutRenderTargetRHI](FRHICommandList& RHICmdList)
		{
			TShaderMapRef<FSimpleRDGComputeShader> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			FIntVector ThreadGroupCount(OutRenderTargetRHI->GetSizeX() / 32, OutRenderTargetRHI->GetSizeY() / 32, 1);
			FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *ComputeShaderParameters, ThreadGroupCount);
        });

		TRefCountPtr<FRDGPooledBuffer> PooledBuffer;
		GraphBuilder.QueueBufferExtraction(GPUOutputBuffer, &PooledBuffer, ERHIAccess::CPURead);

		TRefCountPtr<IPooledRenderTarget> ComputePooledRenderTarget;
		GraphBuilder.QueueTextureExtraction(RDGComputeRenderTarget, &ComputePooledRenderTarget);

		// Pass2: 基础绘制，并使用计算着色器输出纹理为图片
		const FRDGTextureDesc& RenderTargetDesc = FRDGTextureDesc::Create2D(OutRenderTargetRHI->GetSizeXY(), OutRenderTargetRHI->GetFormat(), FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_UAV);
		FRDGTextureRef RDGRenderTarget = GraphBuilder.CreateTexture(RenderTargetDesc, TEXT("RDGRenderTarget"));

		FSimpleRDGPixelShader::FParameters* Parameters = GraphBuilder.AllocParameters<FSimpleRDGPixelShader::FParameters>();
		Parameters->Texture            = RDGComputeRenderTarget;
		Parameters->TextureSampler     = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		Parameters->SimpleUniform      = TUniformBufferRef<FSimpleUniformStructParameters>::CreateUniformBufferImmediate(StructParameters, UniformBuffer_SingleFrame);
		Parameters->RenderTargets[0]   = FRenderTargetBinding(RDGRenderTarget, ERenderTargetLoadAction::ENoAction);

		GraphBuilder.AddPass(RDG_EVENT_NAME("RDGSimpleDraw"), Parameters, ERDGPassFlags::Raster, [Parameters, ComputeShaderParameters](FRHICommandList& RHICmdList)
		{
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			FRHITexture2D* RT = Parameters->RenderTargets[0].GetTexture()->GetRHI()->GetTexture2D();
			RHICmdList.SetViewport(0, 0, 0.0f, RT->GetSizeX(), RT->GetSizeY(), 1.0f);

			TShaderMapRef<FSimpleRDGVertexShader> VertexShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			TShaderMapRef<FSimpleRDGPixelShader>  PixelShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GTextureVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI  = PixelShader.GetPixelShader();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BlendState        = TStaticBlendState<>::GetRHI();
			GraphicsPSOInit.RasterizerState   = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.PrimitiveType     = PT_TriangleList;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
			RHICmdList.SetStencilRef(0);

			// Draw
			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *Parameters);
			RHICmdList.SetStreamSource(0, GRectangleVertexBuffer.VertexBufferRHI, 0); 
			RHICmdList.DrawIndexedPrimitive(GRectangleIndexBuffer.IndexBufferRHI, 0, 0, 4, 0, 2, 1);
		});

		TRefCountPtr<IPooledRenderTarget> PooledRenderTarget;
		GraphBuilder.QueueTextureExtraction(RDGRenderTarget, &PooledRenderTarget);

		GraphBuilder.Execute();

        //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // 必须调用GraphBuilder.QueueBufferExtraction()函数且在GraphBuilder.Execute()之后才能访问PooledBuffer!
        FGPUOutput Data;
        void* GPUOutputPtr = static_cast<FGPUOutput*>(RHILockBuffer(PooledBuffer->GetRHI(), 0, PooledBuffer->GetSize(), EResourceLockMode::RLM_ReadOnly));
        FMemory::Memcpy(&Data, GPUOutputPtr, sizeof(FGPUOutput));
        RHIUnlockBuffer(PooledBuffer->GetRHI());

		RHIImmCmdList.CopyTexture(PooledRenderTarget->GetRHI(), OutRenderTargetRHI, FRHICopyTextureInfo());
	}
} 

void USimpleRenderingExampleBlueprintLibrary::UseMultiPass(const UObject* WorldContextObject, UTextureRenderTarget2D* OutputRenderTarget)
{
	check(IsInGameThread());

	FTexture2DRHIRef RenderTargetRHI = OutputRenderTarget->GetResource()->GetTexture2DRHI();
	ENQUEUE_RENDER_COMMAND(CaptureCommand)([RenderTargetRHI](FRHICommandListImmediate& RHICmdList)
	{
		MultiPassExample::RDGMultiPass(RHICmdList, RenderTargetRHI);
	});
}
