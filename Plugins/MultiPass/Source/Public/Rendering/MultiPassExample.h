#pragma once
#include "RenderGraph.h"

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MultiPassExample.generated.h"

class FRHICommandListImmediate;
struct IPooledRenderTarget;

USTRUCT(BlueprintType, meta = (ScriptName = "SimpleRenderingExample"))
struct FSimpleShaderParameter
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, meta = (WorldContext = "WorldContextObject"))
	FLinearColor Color1;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, meta = (WorldContext = "WorldContextObject"))
	FLinearColor Color2;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, meta = (WorldContext = "WorldContextObject"))
	FLinearColor Color3;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, meta = (WorldContext = "WorldContextObject"))
	FLinearColor Color4;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, meta = (WorldContext = "WorldContextObject"))
	int32 ColorIndex;
};

UCLASS(MinimalAPI, meta = (ScriptName = "SimpleRenderingExample"))
class USimpleRenderingExampleBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, Category = "SimpleRenderingExample", meta = (WorldContext = "WorldContextObject"))
	static void UseMultiPass(const UObject* WorldContextObject, UTextureRenderTarget2D* OutputRenderTarget);
};

namespace MultiPassExample
{
	BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FSimpleUniformStructParameters, )
		SHADER_PARAMETER(FVector4f, Color1)
		SHADER_PARAMETER(FVector4f, Color2)
		SHADER_PARAMETER(FVector4f, Color3)
		SHADER_PARAMETER(FVector4f, Color4)
		SHADER_PARAMETER(uint32, ColorIndex)
	END_GLOBAL_SHADER_PARAMETER_STRUCT()

	struct FTextureVertex
	{
		FVector4f Position;
		FVector2f UV;
	};

	class FRectangleVertexBuffer : public FVertexBuffer
	{
	public:
		void InitRHI() override
		{
			TResourceArray<FTextureVertex, VERTEXBUFFER_ALIGNMENT> Vertices;
			Vertices.SetNumUninitialized(6);

			Vertices[0].Position = FVector4f(1, 1, 0, 1);
			Vertices[0].UV = FVector2f(1, 1);

			Vertices[1].Position = FVector4f(-1, 1, 0, 1);
			Vertices[1].UV = FVector2f(0, 1);

			Vertices[2].Position = FVector4f(1, -1, 0, 1);
			Vertices[2].UV = FVector2f(1, 0);

			Vertices[3].Position = FVector4f(-1, -1, 0, 1);
			Vertices[3].UV = FVector2f(0, 0);

			Vertices[4].Position = FVector4f(-1, 1, 0, 1);
			Vertices[4].UV = FVector2f(-1, 1);

			Vertices[5].Position = FVector4f(1, -1, 0, 1);
			Vertices[5].UV = FVector2f(1, -1);

			FRHIResourceCreateInfo CreateInfo(TEXT("FRectangleVertexBuffer"), &Vertices);
			VertexBufferRHI = RHICreateVertexBuffer(Vertices.GetResourceDataSize(), BUF_Static, CreateInfo);
		}
	};

	class FRectangleIndexBuffer : public FIndexBuffer
	{
	public:
		void InitRHI() override
		{
			const uint16 Indices[] = {0, 1, 2, 2, 1, 3, 0, 4, 5};
			const uint32 NumIndices = UE_ARRAY_COUNT(Indices);

			TResourceArray<uint16, INDEXBUFFER_ALIGNMENT> IndexBuffer;
			IndexBuffer.AddUninitialized(NumIndices);
			FMemory::Memcpy(IndexBuffer.GetData(), Indices, NumIndices * sizeof(uint16));

			FRHIResourceCreateInfo CreateInfo(TEXT("FRectangleIndexBuffer"), &IndexBuffer);
			IndexBufferRHI = RHICreateIndexBuffer(sizeof(uint16), IndexBuffer.GetResourceDataSize(), BUF_Static, CreateInfo);
		}
	};

	class FTextureVertexDeclaration : public FRenderResource
	{
	public:
		FVertexDeclarationRHIRef VertexDeclarationRHI;

		virtual void InitRHI() override
		{
			FVertexDeclarationElementList Elements;
			uint32 Stride = sizeof(FTextureVertex);
			Elements.Add(FVertexElement(0, STRUCT_OFFSET(FTextureVertex, Position), VET_Float2, 0, Stride));
			Elements.Add(FVertexElement(0, STRUCT_OFFSET(FTextureVertex, UV), VET_Float2, 1, Stride));
			VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
		}

		virtual void ReleaseRHI() override { VertexDeclarationRHI.SafeRelease(); }
	};

	extern TGlobalResource<FTextureVertexDeclaration> GTextureVertexDeclaration;
	extern TGlobalResource<FRectangleVertexBuffer>    GRectangleVertexBuffer;
	extern TGlobalResource<FRectangleIndexBuffer>     GRectangleIndexBuffer;

	void RDGMultiPass(FRHICommandListImmediate &RHIImmCmdList, FTexture2DRHIRef OutRenderTargetRHI);

} 
