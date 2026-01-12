/*****************************************************************//**
 * @file	ThumbnailGenerator.cpp
 * @brief	アセットのサムネイルを動的に生成するクラス
 *
 * @details
 *
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___THUMBNAIL_GENERATOR_H___
#define ___THUMBNAIL_GENERATOR_H___

#include "Engine/pch.h"
#include "Engine/Core/Graphics/Graphics.h"
#include "Engine/Resource/ResourceManager.h"
#include "Engine/Scene/Components/Components.h"
#include "Engine/Renderer/Renderers/ModelRenderer.h"
#include "Engine/Renderer/Renderers/SpriteRenderer.h"
#include "Engine/Scene/Serializer/SceneSerializer.h"

namespace Arche
{
	class ThumbnailGenerator
	{
	public:
		static void Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
		{
			s_device = device;
			s_context = context;

			// 128x128 のサムネイル用レンダーターゲット作成
			CreateRenderTarget(128, 128);

			// 保存先ディレクトリ作成
			if (!std::filesystem::exists("Resources/Editor/Temp/Thumbnails"))
			{
				std::filesystem::create_directories("Resources/Editor/Temp/Thumbnails");
			}
		}

		static void Shutdown()
		{
			s_rtv.Reset();
			s_srv.Reset();
			s_dsv.Reset();
			s_tex.Reset();
			s_aabbCache.clear();
		}

		// 指定パスのアセットのサムネイルを生成し、保存先のパスを返す
		static std::string Generate(const std::filesystem::path& assetPath)
		{
			// ファイル名ハッシュなどをキーにするのが安全だが、今回はファイル名を使用
			std::string filename = assetPath.filename().stem().string();
			// 拡張子判別で衝突しないように元拡張子も付与 (例: MyModel_fbx.png)
			std::string ext = assetPath.extension().string();
			std::string cacheName = filename + "_" + ext.substr(1) + ".png";
			std::string cachePath = "Resources/Editor/Temp/Thumbnails/" + cacheName;

			// 既に生成済みならそのパスを返す
			if (std::filesystem::exists(cachePath))
			{
				// 本来はファイルの更新日時を比較して再生成判定すべきですが今回は省略
				return cachePath;
			}

			// 生成処理
			bool success = false;
			std::string lowerExt = ext;
			std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::tolower);

			if (lowerExt == ".fbx" || lowerExt == ".obj" || lowerExt == ".gltf" || lowerExt == ".glb")
			{
				success = RenderModelThumbnail(assetPath, cachePath);
			}
			else if (lowerExt == ".json")
			{
				success = RenderPrefabThumbnail(assetPath, cachePath);
			}

			if (success) return cachePath;
			return "";
		}

	private:
		// AABB定義
		struct AABBMinMax {
			XMFLOAT3 min = { FLT_MAX, FLT_MAX, FLT_MAX };
			XMFLOAT3 max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
			bool isValid = false;

			void Merge(const XMFLOAT3& p) {
				min.x = std::min(min.x, p.x); min.y = std::min(min.y, p.y); min.z = std::min(min.z, p.z);
				max.x = std::max(max.x, p.x); max.y = std::max(max.y, p.y); max.z = std::max(max.z, p.z);
				isValid = true;
			}
			void Merge(const AABBMinMax& other) {
				if (!other.isValid) return;
				Merge(other.min);
				Merge(other.max);
			}
			XMFLOAT3 GetCenter() const {
				return { (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f };
			}
			float GetRadius() const {
				XMFLOAT3 c = GetCenter();
				float dx = max.x - c.x; float dy = max.y - c.y; float dz = max.z - c.z;
				return std::sqrt(dx * dx + dy * dy + dz * dz);
			}
		};

		static void CreateRenderTarget(UINT width, UINT height)
		{
			s_width = width;
			s_height = height;

			// Texture
			D3D11_TEXTURE2D_DESC td = {};
			td.Width = width; td.Height = height;
			td.MipLevels = 1; td.ArraySize = 1;
			td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			td.SampleDesc.Count = 1;
			td.Usage = D3D11_USAGE_DEFAULT;
			td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			s_device->CreateTexture2D(&td, nullptr, &s_tex);

			// RTV
			s_device->CreateRenderTargetView(s_tex.Get(), nullptr, &s_rtv);

			// SRV
			s_device->CreateShaderResourceView(s_tex.Get(), nullptr, &s_srv);

			// DSV
			D3D11_TEXTURE2D_DESC dsd = {};
			dsd.Width = width; dsd.Height = height;
			dsd.MipLevels = 1; dsd.ArraySize = 1;
			dsd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
			dsd.SampleDesc.Count = 1;
			dsd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
			ComPtr<ID3D11Texture2D> depthTex;
			s_device->CreateTexture2D(&dsd, nullptr, &depthTex);
			s_device->CreateDepthStencilView(depthTex.Get(), nullptr, &s_dsv);
		}

		// レンダリング共通処理（準備～描画～保存）
		static bool RenderAndSave(const std::string& outPath, const XMFLOAT3& center, float radius, std::function<void()> drawCallback)
		{
			// 1. バックアップ
			ComPtr<ID3D11RenderTargetView> oldRTV;
			ComPtr<ID3D11DepthStencilView> oldDSV;
			s_context->OMGetRenderTargets(1, &oldRTV, &oldDSV);
			D3D11_VIEWPORT oldVP;
			UINT numVP = 1;
			s_context->RSGetViewports(&numVP, &oldVP);

			// 2. ターゲット切り替え & クリア
			// 背景色：透明またはダークグレー
			float clearColor[] = { 1.0f, 1.0f };
			s_context->OMSetRenderTargets(1, s_rtv.GetAddressOf(), s_dsv.Get());
			s_context->ClearRenderTargetView(s_rtv.Get(), clearColor);
			s_context->ClearDepthStencilView(s_dsv.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

			D3D11_VIEWPORT vp = { 0, 0, (float)s_width, (float)s_height, 0.0f, 1.0f };
			s_context->RSSetViewports(1, &vp);

			// 3. カメラ設定
			// オブジェクト全体が収まるように距離を計算
			float fov = XM_PIDIV4; // 45度
			float padding = 1.3f;
			float distance = (radius / sinf(fov * 0.5f)) * padding;
			if (distance < 0.1f) distance = 2.0f; // 安全策

			// 斜め上から見る
			XMVECTOR target = XMLoadFloat3(&center);
			XMVECTOR dir = XMVector3Normalize(XMVectorSet(1.0f, 1.0f, -1.0f, 0.0f));
			XMVECTOR eye = target + dir * distance;
			XMVECTOR up = XMVectorSet(0, 1, 0, 0);

			XMMATRIX view = XMMatrixLookAtLH(eye, target, up);
			XMMATRIX proj = XMMatrixPerspectiveFovLH(fov, 1.0f, 0.1f, 1000.0f);

			// 4. 描画開始
			XMFLOAT3 lightDir = { 0.5f, -1.0f, 0.5f };
			ModelRenderer::Begin(view, proj, lightDir, { 1, 1, 1 });

			// 5. 実際の描画（コールバック）
			drawCallback();

			// 6. 画像保存
			bool result = false;
			DirectX::ScratchImage image;
			HRESULT hr = DirectX::CaptureTexture(s_device, s_context, s_tex.Get(), image);
			if (SUCCEEDED(hr))
			{
				std::wstring wOutPath(outPath.begin(), outPath.end());
				hr = DirectX::SaveToWICFile(*image.GetImage(0, 0, 0), DirectX::WIC_FLAGS_NONE, GetWICCodec(WIC_CODEC_PNG), wOutPath.c_str());
				result = SUCCEEDED(hr);
			}

			// 7. 復元
			s_context->OMSetRenderTargets(1, oldRTV.GetAddressOf(), oldDSV.Get());
			s_context->RSSetViewports(numVP, &oldVP);

			return result;
		}

		// ------------------------------------------------------------
		// モデル (.fbx, .obj) 用
		// ------------------------------------------------------------
		static bool RenderModelThumbnail(const std::filesystem::path& path, const std::string& outPath)
		{
			// モデルロード（GPU用）
			auto model = ResourceManager::Instance().GetModel(path.string());
			if (!model) return false;

			// AABB計算（CPU用データがないのでAssimpで再読み込みして計算）
			AABBMinMax aabb = GetModelAABB(path.string());
			if (!aabb.isValid) {
				// 読み込み失敗時はデフォルトサイズ
				aabb.min = { -0.5f, -0.5f, -0.5f };
				aabb.max = { 0.5f, 0.5f, 0.5f };
			}

			return RenderAndSave(outPath, aabb.GetCenter(), aabb.GetRadius(), [&]()
				{
					ModelRenderer::Draw(model, XMMatrixIdentity());
				});
		}

		// ------------------------------------------------------------
		// プレファブ (.json) 用
		// ------------------------------------------------------------
		static bool RenderPrefabThumbnail(const std::filesystem::path& path, const std::string& outPath)
		{
			World tempWorld;
			Entity root = SceneSerializer::LoadPrefab(tempWorld, path.string());
			if (root == NullEntity) return false;

			Registry& reg = tempWorld.getRegistry();

			// ワールド行列計算
			UpdateTransforms(reg, root, XMMatrixIdentity());

			// 全体のAABB計算
			AABBMinMax totalBounds;
			CalculateHierarchyAABB(reg, root, totalBounds);

			// 有効なメッシュがない（2Dスプライトのみ、あるいは空）場合
			if (!totalBounds.isValid)
			{
				// Spriteがあるか確認して、あればデフォルトサイズで描画を試みる
				bool hasSprite = false;
				reg.view<SpriteComponent>().each([&](auto entity, auto& sprite) { hasSprite = true; });

				if (hasSprite) {
					// 2D用に少し引く
					totalBounds.min = { -1, -1, 0 }; totalBounds.max = { 1, 1, 0 };
				}
				else {
					totalBounds.min = { -0.5f, -0.5f, -0.5f }; totalBounds.max = { 0.5f, 0.5f, 0.5f };
				}
			}

			return RenderAndSave(outPath, totalBounds.GetCenter(), totalBounds.GetRadius(), [&]() {
				DrawHierarchy(reg, root);
				});
		}

		// 階層のトランスフォーム更新（再帰）
		static void UpdateTransforms(Registry& reg, Entity entity, CXMMATRIX parentMat)
		{
			XMMATRIX worldMat = parentMat;

			if (reg.has<Transform>(entity))
			{
				auto& t = reg.get<Transform>(entity);
				// ローカル行列作成
				XMMATRIX local = XMMatrixScaling(t.scale.x, t.scale.y, t.scale.z) *
					XMMatrixRotationRollPitchYaw(t.rotation.x, t.rotation.y, t.rotation.z) *
					XMMatrixTranslation(t.position.x, t.position.y, t.position.z);

				worldMat = local * parentMat;

				// 計算結果を格納（描画で使用）
				XMStoreFloat4x4(&t.worldMatrix, worldMat);
			}

			if (reg.has<Relationship>(entity))
			{
				for (auto child : reg.get<Relationship>(entity).children)
				{
					UpdateTransforms(reg, child, worldMat);
				}
			}
		}

		// 階層全体のAABB計算（再帰）
		static void CalculateHierarchyAABB(Registry& reg, Entity entity, AABBMinMax& outBounds)
		{
			// MeshComponentを持っていればAABBを加算
			if (reg.has<MeshComponent>(entity) && reg.has<Transform>(entity))
			{
				auto& meshComp = reg.get<MeshComponent>(entity);
				auto& transform = reg.get<Transform>(entity);

				// モデルのローカルAABBを取得
				AABBMinMax modelBounds = GetModelAABB(meshComp.modelKey);
				if (modelBounds.isValid)
				{
					// ローカルAABBの8頂点をワールド変換して、全体のMinMaxを更新
					XMMATRIX world = transform.GetWorldMatrix(); // UpdateTransformsで計算済み

					// 8頂点
					XMFLOAT3 corners[8];
					corners[0] = { modelBounds.min.x, modelBounds.min.y, modelBounds.min.z };
					corners[1] = { modelBounds.min.x, modelBounds.min.y, modelBounds.max.z };
					corners[2] = { modelBounds.min.x, modelBounds.max.y, modelBounds.min.z };
					corners[3] = { modelBounds.min.x, modelBounds.max.y, modelBounds.max.z };
					corners[4] = { modelBounds.max.x, modelBounds.min.y, modelBounds.min.z };
					corners[5] = { modelBounds.max.x, modelBounds.min.y, modelBounds.max.z };
					corners[6] = { modelBounds.max.x, modelBounds.max.y, modelBounds.min.z };
					corners[7] = { modelBounds.max.x, modelBounds.max.y, modelBounds.max.z };

					for (int i = 0; i < 8; ++i)
					{
						XMVECTOR v = XMLoadFloat3(&corners[i]);
						v = XMVector3Transform(v, world);
						XMFLOAT3 p; XMStoreFloat3(&p, v);
						outBounds.Merge(p);
					}
				}
			}

			// 子要素へ
			if (reg.has<Relationship>(entity))
			{
				for (auto child : reg.get<Relationship>(entity).children)
				{
					CalculateHierarchyAABB(reg, child, outBounds);
				}
			}
		}

		// 階層描画（再帰）
		static void DrawHierarchy(Registry& reg, Entity entity)
		{
			if (reg.has<MeshComponent>(entity) && reg.has<Transform>(entity))
			{
				auto& meshComp = reg.get<MeshComponent>(entity);
				auto& transform = reg.get<Transform>(entity);

				auto model = ResourceManager::Instance().GetModel(meshComp.modelKey);
				if (model)
				{
					ModelRenderer::Draw(model, transform.GetWorldMatrix());
				}
			}

			if (reg.has<Relationship>(entity))
			{
				for (auto child : reg.get<Relationship>(entity).children)
				{
					DrawHierarchy(reg, child);
				}
			}
		}

		// Assimpを使ってモデルファイルのAABBを計算（キャッシュ付き）
		static AABBMinMax GetModelAABB(const std::string& keyName)
		{
			// キャッシュにあれば返す
			if (s_aabbCache.find(keyName) != s_aabbCache.end())
			{
				return s_aabbCache[keyName];
			}

			std::string path = keyName;
			if (!std::filesystem::exists(path))
			{
				// よくあるフォルダを探す
				std::string tryPath = "Resources/Game/Models/" + keyName;
				if (std::filesystem::exists(tryPath)) path = tryPath;
			}

			AABBMinMax bounds;
			if (!std::filesystem::exists(path)) return bounds;

			Assimp::Importer importer;
			// 頂点情報だけ必要なので最小限のフラグで
			const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate);

			if (scene)
			{
				ProcessNodeAABB(scene->mRootNode, scene, bounds);
			}

			s_aabbCache[keyName] = bounds;
			return bounds;
		}

		static void ProcessNodeAABB(aiNode* node, const aiScene* scene, AABBMinMax& bounds)
		{
			for (unsigned int i = 0; i < node->mNumMeshes; i++)
			{
				aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
				for (unsigned int v = 0; v < mesh->mNumVertices; v++)
				{
					// Assimpのベクトルを変換
					XMFLOAT3 p;
					p.x = mesh->mVertices[v].x;
					p.y = mesh->mVertices[v].y;
					p.z = mesh->mVertices[v].z;
					bounds.Merge(p);
				}
			}
			for (unsigned int i = 0; i < node->mNumChildren; i++)
			{
				ProcessNodeAABB(node->mChildren[i], scene, bounds);
			}
		}

		static REFGUID GetWICCodec(WICCodecs codec)
		{
			switch (codec) {
			case WIC_CODEC_PNG: return GUID_ContainerFormatPng;
			default: return GUID_ContainerFormatPng;
			}
		}

		static inline ID3D11Device* s_device = nullptr;
		static inline ID3D11DeviceContext* s_context = nullptr;
		static inline UINT s_width = 128;
		static inline UINT s_height = 128;

		static inline ComPtr<ID3D11Texture2D> s_tex;
		static inline ComPtr<ID3D11RenderTargetView> s_rtv;
		static inline ComPtr<ID3D11ShaderResourceView> s_srv;
		static inline ComPtr<ID3D11DepthStencilView> s_dsv;

		// AABBキャッシュ
		static inline std::unordered_map<std::string, AABBMinMax> s_aabbCache;
	};
}

#endif // !___THUMBNAIL_GENERATOR_H___