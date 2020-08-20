#pragma once

#include "DXUtil.h"
#include "Renderer.h"
#include <string>
#include <vector>
#include <map>
#include <wrl.h>

#define DESCRIPTORS_HEAP_SIZE 50

class AssetsManager
{

public:

	AssetsManager(Microsoft::WRL::ComPtr<ID3D12Device> device) 
	{
		m_device = device;

		m_SRV_DescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		m_sampler_DescriptorSize  = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

		// Create descriptor heap for shader resources
		D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
		descriptorHeapDesc = {};
		descriptorHeapDesc.NumDescriptors = DESCRIPTORS_HEAP_SIZE;
		descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		DXUtil::ThrowIfFailed(m_device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_texturesDescriptorHeap)),
			"Cannot create shader resources decriptor heap");

		// Create descriptor heap for samplers
		descriptorHeapDesc = {};
		descriptorHeapDesc.NumDescriptors = 1;
		descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		DXUtil::ThrowIfFailed(m_device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_samplersDescriptorHeap)),
			"Cannot create sampler descriptor heap");
	}

	void AddGPUBuffer(Microsoft::WRL::ComPtr<ID3D12Resource> buffer)
	{
		m_buffersGPU.push_back(buffer);
	}

	void AddTexture(unsigned int textureId, Microsoft::WRL::ComPtr<ID3D12Resource> texture)
	{
		m_textures[textureId] = texture;
		
		// Create the view for the texture
		CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(m_texturesDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
		hDescriptor.Offset(textureId, m_SRV_DescriptorSize);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = texture->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = texture->GetDesc().MipLevels;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		m_device->CreateShaderResourceView(texture.Get(), &srvDesc, hDescriptor);
	}

	void AddSampler(unsigned int samplerId, D3D12_SAMPLER_DESC samplerDesc)
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(m_samplersDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
		hDescriptor.Offset(samplerId, m_sampler_DescriptorSize);
		m_device->CreateSampler(&samplerDesc, m_samplersDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	}

public:
	Microsoft::WRL::ComPtr<ID3D12Device> m_device;

	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_buffersGPU;
	std::map<unsigned int, Microsoft::WRL::ComPtr<ID3D12Resource>> m_textures;

	UINT m_SRV_DescriptorSize = 0;
	UINT m_sampler_DescriptorSize = 0;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_samplersDescriptorHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_texturesDescriptorHeap;
};

struct BufferView 
{
	unsigned int bufferId = -1;
	unsigned int byteOffset = 0;
	unsigned int byteLength = 0;
	unsigned int count = 0;
};

struct SubMesh
{
	BufferView verticesBufferView;
	BufferView indicesBufferView;
	BufferView normalsBufferView;
	BufferView texCoord0BufferView;
	BufferView texCoord1BufferView;
	unsigned int materialId;
	unsigned int renderMode;
};


class Mesh
{
public:

	Mesh(std::shared_ptr<AssetsManager> assetsManager)
	{
		m_assetsManager = assetsManager;
	}

	void SetId(unsigned int id)
	{
		m_id = id;
	}

	void AddSubMesh(const SubMesh& subMesh)
	{
		m_subMeshes.push_back(subMesh);
	}

	void Draw(ID3D12GraphicsCommandList* commandList)
	{

		for(SubMesh subMesh : m_subMeshes) 
		{
			D3D12_VERTEX_BUFFER_VIEW vbView;
			vbView.BufferLocation = m_assetsManager->m_buffersGPU[subMesh.verticesBufferView.bufferId]->GetGPUVirtualAddress() + subMesh.verticesBufferView.byteOffset;
			vbView.StrideInBytes = sizeof(DirectX::XMFLOAT3);
			vbView.SizeInBytes = subMesh.verticesBufferView.byteLength;

			D3D12_VERTEX_BUFFER_VIEW tbView;
			tbView.BufferLocation = m_assetsManager->m_buffersGPU[subMesh.texCoord0BufferView.bufferId]->GetGPUVirtualAddress() + subMesh.texCoord0BufferView.byteOffset;
			tbView.StrideInBytes = sizeof(DirectX::XMFLOAT2);
			tbView.SizeInBytes = subMesh.texCoord0BufferView.byteLength;;

			D3D12_VERTEX_BUFFER_VIEW vertexBuffers[2] = { vbView, tbView };
			commandList->IASetVertexBuffers(0, 2, vertexBuffers);
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			D3D12_INDEX_BUFFER_VIEW ibView;
			ibView.BufferLocation = m_assetsManager->m_buffersGPU[subMesh.indicesBufferView.bufferId]->GetGPUVirtualAddress() + subMesh.indicesBufferView.byteOffset;
			ibView.Format = DXGI_FORMAT_R16_UINT;
			ibView.SizeInBytes = subMesh.indicesBufferView.byteLength;

			D3D12_INDEX_BUFFER_VIEW indexBuffers[1] = { ibView };
			commandList->IASetIndexBuffer(indexBuffers);

			commandList->DrawIndexedInstanced(subMesh.indicesBufferView.count, 1, 0, 0, 0);
		}
	}

public:
	unsigned int m_id;
	std::vector<SubMesh> m_subMeshes;
	std::shared_ptr<AssetsManager> m_assetsManager;
};


