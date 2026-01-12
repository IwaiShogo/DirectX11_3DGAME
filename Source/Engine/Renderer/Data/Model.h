/*****************************************************************//**
 * @file	Model.h
 * @brief	3Dモデルのデータを保持するクラス
 *
 * @details
 *
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *
 * @date	2025/11/26	初回作成日
 * 			作業内容：	- 追加：
 *
 * @update	2025/xx/xx	最終更新日
 * 			作業内容：	- XX：
 *
 * @note	（省略可）
 *********************************************************************/

#ifndef ___MODEL_H___
#define ___MODEL_H___

#include "Engine/pch.h"
#include "Engine/Renderer/RHI/Texture.h"
#include "Engine/Renderer/RHI/MeshBuffer.h"

struct aiScene;
struct aiNode;

namespace Arche
{
	class Model
	{
	public:
		using AnimeNo = int;
		static const AnimeNo ANIME_NONE = -1;

		enum Flip { None, XFlip, ZFlip, ZFlipUseAnime };

		struct Bone {
			int index;
			XMMATRIX invOffset;
		};

		struct Mesh {
			std::vector<MeshBuffer::Vertex> vertices;
			std::vector<uint32_t> indices;
			unsigned int materialID;
			std::vector<Bone> bones;
			MeshBuffer* pMesh = nullptr;
		};

		struct Material {
			XMFLOAT4 diffuse;
			XMFLOAT4 ambient;
			XMFLOAT4 specular;
			std::shared_ptr<Texture> pTexture = nullptr;
			std::string texturePath; // 非同期ロード用にパスを保持
		};

		struct Node {
			std::string name;
			int parent;
			std::vector<int> children;
			XMMATRIX mat;
			XMMATRIX localMat;
		};

		struct TransformData {
			XMFLOAT3 translate;
			XMFLOAT4 quaternion;
			XMFLOAT3 scale;
		};

		struct Animation {
			std::string name;
			float totalTime = 0.0f;
			float nowTime = 0.0f;
			float speed = 1.0f;
			bool isLoop = false;

			struct Channel {
				int nodeIndex;
				std::vector<std::pair<float, XMFLOAT3>> positionKeys;
				std::vector<std::pair<float, XMFLOAT4>> rotationKeys;
				std::vector<std::pair<float, XMFLOAT3>> scalingKeys;
			};
			std::vector<Channel> channels;
		};

	public:
		Model();
		~Model();

		// ★API変更: ロードを2段階に分離
		bool LoadCPU(const std::string& filename, float scale = 1.0f, Flip flip = None);
		void UploadGPU();

		// 従来互換用（同期ロード）
		bool LoadSync(const std::string& filename, float scale = 1.0f, Flip flip = None);

		void Reset();

		const std::vector<Mesh>& GetMeshes() const { return m_meshes; }
		const std::vector<Node>& GetNodes() const { return m_nodes; }
		const Material* GetMaterial(size_t index) const { return &m_materials[index]; }

		void Step(float deltaTime);
		AnimeNo AddAnimation(const std::string& filename);
		AnimeNo ImportAnimation(std::shared_ptr<Model> sourceModel, const std::string& name);
		void Play(AnimeNo no, bool loop = true, float speed = 1.0f, float transitionDuration = 0.0f);
		void Play(const std::string& animName, bool loop = true, float speed = 1.0f, float transitionDuration = 0.0f);
		AnimeNo GetCurrentAnimation() const { return m_playNo; }
		float GetCurrentAnimationTime() const;
		float GetCurrentAnimationLength() const;

	private:
		void MakeMesh(const aiScene* pScene, float scale, Flip flip);
		void MakeMaterial(const aiScene* pScene, const std::string& directory);
		void MakeBoneNodes(const aiScene* pScene);
		void MakeWeight(const aiScene* pScene, int meshIdx);

		void UpdateNodeTransforms();
		void EvaluateAnimation(AnimeNo no, float time, std::vector<TransformData>& outTransforms);
		void BlendTransforms(const std::vector<TransformData>& src, const std::vector<TransformData>& dst, float t, std::vector<TransformData>& outResult);

		XMFLOAT3 Lerp3(const XMFLOAT3& a, const XMFLOAT3& b, float t);
		XMFLOAT4 Slerp4(const XMFLOAT4& a, const XMFLOAT4& b, float t);

	private:
		std::vector<Mesh> m_meshes;
		std::vector<Material> m_materials;
		std::vector<Node> m_nodes;
		std::vector<Animation> m_animes;

		AnimeNo m_playNo = ANIME_NONE;
		AnimeNo m_nextPlayNo = ANIME_NONE;
		float m_transitionTime = 0.0f;
		float m_transitionDuration = 0.0f;

		std::vector<TransformData> m_currentTransforms;
		std::vector<TransformData> m_defaultTransforms;
		std::vector<TransformData> m_blendSrcTransforms;
		std::vector<TransformData> m_blendDstTransforms;

		float m_loadScale = 1.0f;
		Flip m_loadFlip = None;
	};
}
#endif