
#include "Engine/pch.h"
#include "Model.h"
#include "Engine/Core/Application.h"
#include "Engine/Resource/ResourceManager.h"

#if defined(_DEBUG)
#pragma comment(lib, "assimp-vc143-mtd.lib")
#else
#pragma comment(lib, "assimp-vc143-mt.lib")
#endif

namespace Arche
{
	static XMMATRIX AiToXM(const aiMatrix4x4& m) {
		return XMMatrixSet(
			m.a1, m.b1, m.c1, m.d1,
			m.a2, m.b2, m.c2, m.d2,
			m.a3, m.b3, m.c3, m.d3,
			m.a4, m.b4, m.c4, m.d4
		);
	}

	Model::Model() {}
	Model::~Model() { Reset(); }

	void Model::Reset() {
		for (auto& m : m_meshes) { if (m.pMesh) delete m.pMesh; }
		m_meshes.clear();
		m_materials.clear();
		m_nodes.clear();
		m_animes.clear();
		m_currentTransforms.clear();
		m_defaultTransforms.clear();
		m_playNo = ANIME_NONE;
		m_nextPlayNo = ANIME_NONE;
	}

	bool Model::LoadSync(const std::string& filename, float scale, Flip flip)
	{
		if (!LoadCPU(filename, scale, flip)) return false;
		UploadGPU();
		return true;
	}

	bool Model::LoadCPU(const std::string& filename, float scale, Flip flip)
	{
		Reset();
		Assimp::Importer importer;
		importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
		unsigned int flags = aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_FlipWindingOrder | aiProcess_LimitBoneWeights | aiProcess_OptimizeGraph;

		const aiScene* pScene = importer.ReadFile(filename, flags);
		if (!pScene) return false;

		if (scale < 0.001f || scale > 1000.0f) m_loadScale = 1.0f;
		else m_loadScale = scale;

		m_loadFlip = flip;

		std::string dir = filename;
		size_t slash = dir.find_last_of("/\\");
		dir = (slash != std::string::npos) ? dir.substr(0, slash + 1) : "";

		MakeBoneNodes(pScene);
		MakeMesh(pScene, scale, flip);
		MakeMaterial(pScene, dir);

		if (pScene->HasAnimations()) {
			for (unsigned int i = 0; i < pScene->mNumAnimations; ++i)
			{
				aiAnimation* pAnim = pScene->mAnimations[i];
				Animation newAnim;
				newAnim.name = pAnim->mName.C_Str();
				if (newAnim.name.empty()) newAnim.name = "default";
				newAnim.totalTime = (float)pAnim->mDuration;

				for (unsigned int c = 0; c < pAnim->mNumChannels; ++c) {
					aiNodeAnim* channel = pAnim->mChannels[c];
					std::string nodeName = channel->mNodeName.C_Str();

					// 1. 完全一致
					auto it = std::find_if(m_nodes.begin(), m_nodes.end(), [&](const Node& n) { return n.name == nodeName; });
					// 2. ショートネーム一致
					if (it == m_nodes.end()) {
						std::string shortName = nodeName;
						size_t colonPos = shortName.find_last_of(':');
						if (colonPos != std::string::npos) shortName = shortName.substr(colonPos + 1);
						it = std::find_if(m_nodes.begin(), m_nodes.end(), [&](const Node& n) {
							std::string nShort = n.name;
							size_t nColon = nShort.find_last_of(':');
							if (nColon != std::string::npos) nShort = nShort.substr(nColon + 1);
							return nShort == shortName;
							});
					}
					if (it == m_nodes.end()) continue;

					Animation::Channel ch;
					ch.nodeIndex = (int)std::distance(m_nodes.begin(), it);

					for (unsigned int k = 0; k < channel->mNumPositionKeys; ++k)
						ch.positionKeys.push_back({ (float)channel->mPositionKeys[k].mTime, {channel->mPositionKeys[k].mValue.x, channel->mPositionKeys[k].mValue.y, channel->mPositionKeys[k].mValue.z} });
					if (channel->mNumRotationKeys > 0) {
						for (unsigned int k = 0; k < channel->mNumRotationKeys; ++k) {
							float x = channel->mRotationKeys[k].mValue.x; float y = channel->mRotationKeys[k].mValue.y; float z = channel->mRotationKeys[k].mValue.z; float w = channel->mRotationKeys[k].mValue.w;
							DirectX::XMVECTOR Q = DirectX::XMVectorSet(x, y, z, w); Q = DirectX::XMQuaternionNormalize(Q);
							DirectX::XMFLOAT4 normQ; DirectX::XMStoreFloat4(&normQ, Q);
							ch.rotationKeys.push_back({ (float)channel->mRotationKeys[k].mTime, normQ });
						}
					}
					for (unsigned int k = 0; k < channel->mNumScalingKeys; ++k) ch.scalingKeys.push_back({ (float)channel->mScalingKeys[k].mTime, {channel->mScalingKeys[k].mValue.x, channel->mScalingKeys[k].mValue.y, channel->mScalingKeys[k].mValue.z} });
					newAnim.channels.push_back(ch);
				}
				m_animes.push_back(newAnim);
			}
		}
		return true;
	}

	void Model::UploadGPU()
	{
		// メッシュバッファの作成
		for (auto& mesh : m_meshes)
		{
			if (mesh.pMesh == nullptr && !mesh.vertices.empty())
			{
				MeshBuffer::Description desc = {};
				desc.pVtx = mesh.vertices.data(); desc.vtxSize = sizeof(MeshBuffer::Vertex); desc.vtxCount = (UINT)mesh.vertices.size();
				desc.pIdx = mesh.indices.data(); desc.idxSize = 4; desc.idxCount = (UINT)mesh.indices.size();
				desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST; desc.isWrite = false;
				mesh.pMesh = new MeshBuffer();
				mesh.pMesh->Create(desc);
			}
		}

		// マテリアルテクスチャの解決 (メインスレッドで行う)
		for (auto& mat : m_materials)
		{
			if (!mat.texturePath.empty() && mat.pTexture == nullptr)
			{
				// ここでリソースマネージャーから取得（既にロードされていることを期待）
				mat.pTexture = ResourceManager::Instance().GetTexture(mat.texturePath);
			}
		}
	}

	void Model::MakeBoneNodes(const aiScene* pScene) {
		// (前回の修正版と同じ)
		m_nodes.clear();
		std::function<int(aiNode*, int)> Rec = [&](aiNode* node, int parent) -> int {
			Node n; n.name = node->mName.C_Str(); n.parent = parent;
			n.localMat = AiToXM(node->mTransformation);
			n.mat = XMMatrixIdentity();
			m_nodes.push_back(n);
			int idx = (int)m_nodes.size() - 1;
			for (unsigned int i = 0; i < node->mNumChildren; ++i) {
				int child = Rec(node->mChildren[i], idx);
				m_nodes[idx].children.push_back(child);
			}
			return idx;
			};
		if (pScene->mRootNode) Rec(pScene->mRootNode, -1);

		m_currentTransforms.resize(m_nodes.size());
		m_defaultTransforms.resize(m_nodes.size());

		for (size_t i = 0; i < m_nodes.size(); ++i) {
			XMVECTOR S, R, T;
			if (XMMatrixDecompose(&S, &R, &T, m_nodes[i].localMat)) {
				XMStoreFloat3(&m_defaultTransforms[i].scale, S);
				XMStoreFloat4(&m_defaultTransforms[i].quaternion, R);
				XMStoreFloat3(&m_defaultTransforms[i].translate, T);
			}
			else {
				m_defaultTransforms[i].scale = { 1, 1, 1 };
				m_defaultTransforms[i].quaternion = { 0, 0, 0, 1 };
				m_defaultTransforms[i].translate = { 0, 0, 0 };
			}
			m_currentTransforms[i] = m_defaultTransforms[i];
		}
		UpdateNodeTransforms();
	}

	void Model::MakeMesh(const aiScene* pScene, float scale, Flip flip) {
		m_meshes.resize(pScene->mNumMeshes);
		for (unsigned int i = 0; i < pScene->mNumMeshes; ++i) {
			const aiMesh* pAiMesh = pScene->mMeshes[i];
			Mesh& mesh = m_meshes[i];
			mesh.vertices.resize(pAiMesh->mNumVertices);
			for (unsigned int v = 0; v < pAiMesh->mNumVertices; ++v) {
				auto& vert = mesh.vertices[v];
				vert.pos = { pAiMesh->mVertices[v].x, pAiMesh->mVertices[v].y, pAiMesh->mVertices[v].z };
				if (pAiMesh->HasNormals()) vert.normal = { pAiMesh->mNormals[v].x, pAiMesh->mNormals[v].y, pAiMesh->mNormals[v].z };
				if (pAiMesh->HasTextureCoords(0)) vert.uv = { pAiMesh->mTextureCoords[0][v].x, pAiMesh->mTextureCoords[0][v].y };
				vert.color = { 1,1,1,1 };
				vert.weight = { 0,0,0,0 }; vert.index[0] = vert.index[1] = vert.index[2] = vert.index[3] = 0;
			}
			for (unsigned int f = 0; f < pAiMesh->mNumFaces; ++f) {
				const aiFace& face = pAiMesh->mFaces[f];
				if (face.mNumIndices == 3) { mesh.indices.push_back(face.mIndices[0]); mesh.indices.push_back(face.mIndices[1]); mesh.indices.push_back(face.mIndices[2]); }
			}
			mesh.materialID = pAiMesh->mMaterialIndex;
			MakeWeight(pScene, i);

			// ★削除: pMeshのnewはUploadGPUで行うためここではしない
		}
	}

	void Model::MakeMaterial(const aiScene* pScene, const std::string& directory) {
		m_materials.resize(pScene->mNumMaterials);
		for (unsigned int i = 0; i < pScene->mNumMaterials; ++i) {
			const aiMaterial* pMat = pScene->mMaterials[i];
			auto& mat = m_materials[i];
			aiColor3D c(1, 1, 1);
			if (pMat->Get(AI_MATKEY_COLOR_DIFFUSE, c) == AI_SUCCESS) mat.diffuse = { c.r, c.g, c.b, 1 };

			aiString path;
			if (pMat->Get(AI_MATKEY_TEXTURE_DIFFUSE(0), path) == AI_SUCCESS) {
				std::string file = path.C_Str();
				// パスだけ保持し、実際のロードはUploadGPU時に行う、またはResourceManagerにリクエストする
				mat.texturePath = directory + file;

				// 自動的に非同期ロードリクエストを投げても良いが、今回はUploadGPUで解決する
				ResourceManager::Instance().LoadTextureAsync(mat.texturePath);
			}
		}
	}

	void Model::MakeWeight(const aiScene* pScene, int meshIdx)
	{
		const aiMesh* pAiMesh = pScene->mMeshes[meshIdx];
		Mesh& mesh = m_meshes[meshIdx];

		if (!pAiMesh->HasBones()) return;

		struct WInfo { UINT id; float w; };
		std::vector<std::vector<WInfo>> vWeights(mesh.vertices.size());

		mesh.bones.clear();

		for (unsigned int b = 0; b < pAiMesh->mNumBones; ++b)
		{
			aiBone* pBone = pAiMesh->mBones[b];
			std::string name = pBone->mName.C_Str();

			if (name.find("$AssimpFbx$") != std::string::npos)
			{
				continue;
			}

			auto it = std::find_if(m_nodes.begin(), m_nodes.end(),
				[&](const Node& n) { return n.name == name; });

			if (it == m_nodes.end())
			{
				Logger::LogWarning("Bone not found in nodes: " + name);
				continue;
			}

			// 有効なボーンとして登録
			Bone newBone;
			newBone.index = (int)std::distance(m_nodes.begin(), it);
			newBone.invOffset = AiToXM(pBone->mOffsetMatrix);
			mesh.bones.push_back(newBone);

			// 現在の登録数（インデックス）を取得
			UINT currentBoneIndex = (UINT)mesh.bones.size() - 1;

			// ウェイト情報を登録
			for (unsigned int w = 0; w < pBone->mNumWeights; ++w)
			{
				//「b」(Assimp側のインデックス) ではなく
				vWeights[pBone->mWeights[w].mVertexId].push_back({ currentBoneIndex, pBone->mWeights[w].mWeight });
			}
		}
		for (size_t i = 0; i < mesh.vertices.size(); ++i) {
			auto& ws = vWeights[i];
			std::sort(ws.begin(), ws.end(), [](auto& a, auto& b) { return a.w > b.w; });
			float total = 0.0f; for (int k = 0; k < 4 && k < ws.size(); ++k) total += ws[k].w;
			if (total > 0) {
				if (ws.size() > 0) { mesh.vertices[i].weight.x = ws[0].w / total; mesh.vertices[i].index[0] = ws[0].id; }
				if (ws.size() > 1) { mesh.vertices[i].weight.y = ws[1].w / total; mesh.vertices[i].index[1] = ws[1].id; }
				if (ws.size() > 2) { mesh.vertices[i].weight.z = ws[2].w / total; mesh.vertices[i].index[2] = ws[2].id; }
				if (ws.size() > 3) { mesh.vertices[i].weight.w = ws[3].w / total; mesh.vertices[i].index[3] = ws[3].id; }
			}
		}
	}

	// --- アニメーション再生 ---
	Model::AnimeNo Model::AddAnimation(const std::string& filename)
	{
		// 1. 既に自分が持っているかチェック
		auto it = std::find_if(m_animes.begin(), m_animes.end(), [&](const Animation& a) { return a.name == filename; });
		if (it != m_animes.end()) return (AnimeNo)std::distance(m_animes.begin(), it);

		// 2. ResourceManagerにキャッシュがあるか確認する
		// これにより、非同期ロード済みのデータがあれば即座にメモリコピーだけで完了する
		if (auto cachedModel = ResourceManager::Instance().GetModel(filename))
		{
			// キャッシュされているモデル自体が自分自身でないことを確認
			if (cachedModel.get() != this)
			{
				return ImportAnimation(cachedModel, filename);
			}
		}

		// 3. キャッシュになければディスクから同期読み込み (カクつきの原因だが、安全策として残す)
		Assimp::Importer importer;
		const aiScene* pScene = importer.ReadFile(filename, 0);
		if (!pScene || !pScene->HasAnimations()) return ANIME_NONE;

		// 1つ目のアニメーションを読み込む
		aiAnimation* pAnim = pScene->mAnimations[0];
		Animation newAnim;
		newAnim.name = filename; // 識別名としてファイル名を使う
		newAnim.totalTime = (float)pAnim->mDuration;
		// TPS処理など（省略、Loadと同じロジック）

		for (unsigned int i = 0; i < pAnim->mNumChannels; ++i) {
			// (Loadと同じ処理でチャンネル作成)
			aiNodeAnim* channel = pAnim->mChannels[i];
			std::string nodeName = channel->mNodeName.C_Str();

			// 1. 完全一致で探す
			auto nodeIt = std::find_if(m_nodes.begin(), m_nodes.end(), [&](const Node& n) { return n.name == nodeName; });
			
			// 2. 見つからない場合、「:」以前の名前（ショートネーム）で再検索
			if (nodeIt == m_nodes.end())
			{
				// "mixamorig:Hips" -> "Hips" を取り出す
				std::string shortName = nodeName;
				size_t colonPos = shortName.find_last_of(':');
				if (colonPos != std::string::npos) {
					shortName = shortName.substr(colonPos + 1);
				}

				nodeIt = std::find_if(m_nodes.begin(), m_nodes.end(),
					[&](const Node& n) {
						std::string nShort = n.name;
						size_t nColon = nShort.find_last_of(':');
						if (nColon != std::string::npos) nShort = nShort.substr(nColon + 1);

						// 末尾一致で比較（"LeftUpLeg" と "mixamorig:LeftUpLeg" を一致させる）
						return nShort == shortName;
					});
			}

			// それでも見つからないならスキップ
			if (nodeIt == m_nodes.end()) {
				// 念のためログを出す（どのボーンが無視されたか知るため）
				// Logger::LogWarning("[AddAnim] Node not found for channel: " + nodeName);
				continue;
			}

			Animation::Channel ch;
			ch.nodeIndex = (int)std::distance(m_nodes.begin(), nodeIt);

			for (unsigned int k = 0; k < channel->mNumPositionKeys; ++k)
				ch.positionKeys.push_back({ (float)channel->mPositionKeys[k].mTime, {channel->mPositionKeys[k].mValue.x, channel->mPositionKeys[k].mValue.y, channel->mPositionKeys[k].mValue.z} });
			if (channel->mNumRotationKeys > 0 && channel->mRotationKeys != nullptr)
			{
				for (unsigned int k = 0; k < channel->mNumRotationKeys; ++k)
				{
					// データアクセス
					float x = channel->mRotationKeys[k].mValue.x;
					float y = channel->mRotationKeys[k].mValue.y;
					float z = channel->mRotationKeys[k].mValue.z;
					float w = channel->mRotationKeys[k].mValue.w;

					DirectX::XMVECTOR Q = DirectX::XMVectorSet(x, y, z, w);

					// ゼロチェック & 正規化
					DirectX::XMVECTOR Len = DirectX::XMQuaternionLength(Q);
					float len;
					DirectX::XMStoreFloat(&len, Len);

					if (len < 0.0001f) {
						Q = DirectX::XMQuaternionIdentity();
					}
					else {
						Q = DirectX::XMQuaternionNormalize(Q);
					}

					DirectX::XMFLOAT4 normQ;
					DirectX::XMStoreFloat4(&normQ, Q);

					ch.rotationKeys.push_back({ (float)channel->mRotationKeys[k].mTime, normQ });
				}
			}
			for (unsigned int k = 0; k < channel->mNumScalingKeys; ++k)
			{
				ch.scalingKeys.push_back({
					(float)channel->mScalingKeys[k].mTime,
					{
						channel->mScalingKeys[k].mValue.x,
						channel->mScalingKeys[k].mValue.y,
						channel->mScalingKeys[k].mValue.z
					}
					});
			}

			newAnim.channels.push_back(ch);
		}

		m_animes.push_back(newAnim);
		return (AnimeNo)m_animes.size() - 1;
	}

	Model::AnimeNo Model::ImportAnimation(std::shared_ptr<Model> sourceModel, const std::string& name)
	{
		if (!sourceModel || sourceModel->m_animes.empty()) return ANIME_NONE;

		// ソースモデルの最初のアニメーションを取得（通常FBXには1つだけ）
		const Animation& srcAnim = sourceModel->m_animes[0];

		// 自分のリストに追加
		Animation newAnim = srcAnim;
		newAnim.name = name; // 名前を上書き

		// チャンネルのノードインデックスを、自分のノード構造に合わせてリマップする
		// (リソースモデルとこのモデルでノード番号が違う可能性があるため)
		for (auto& ch : newAnim.channels)
		{
			// ソースモデルでのノード名を取得
			if (ch.nodeIndex < 0 || ch.nodeIndex >= sourceModel->m_nodes.size()) continue;
			std::string nodeName = sourceModel->m_nodes[ch.nodeIndex].name;

			// 自分の中で同じ名前のノードを探す
			auto it = std::find_if(m_nodes.begin(), m_nodes.end(), [&](const Node& n) { return n.name == nodeName; });

			// 見つからなければショートネームで再検索 (mixamorig:Hips -> Hips)
			if (it == m_nodes.end())
			{
				std::string shortName = nodeName;
				size_t colon = shortName.find_last_of(':');
				if (colon != std::string::npos) shortName = shortName.substr(colon + 1);

				it = std::find_if(m_nodes.begin(), m_nodes.end(), [&](const Node& n) {
					std::string myShort = n.name;
					size_t myColon = myShort.find_last_of(':');
					if (myColon != std::string::npos) myShort = myShort.substr(myColon + 1);
					return myShort == shortName;
					});
			}

			if (it != m_nodes.end())
			{
				ch.nodeIndex = (int)std::distance(m_nodes.begin(), it);
			}
			else
			{
				// 対応するボーンがない場合、このチャンネルは無効化
				ch.nodeIndex = -1;
			}
		}

		m_animes.push_back(newAnim);
		return (AnimeNo)m_animes.size() - 1;
	}

	void Model::Play(AnimeNo no, bool loop, float speed, float transitionDuration)
	{
		if (no < 0 || no >= (int)m_animes.size()) return;

		// 同じアニメならパラメータ更新のみ
		if (m_playNo == no && m_nextPlayNo == ANIME_NONE) {
			m_animes[no].isLoop = loop;
			m_animes[no].speed = speed;
			return;
		}

		// 遷移時間が指定されている場合、クロスフェード開始
		if (transitionDuration > 0.0f)
		{
			// 既に遷移中なら、ターゲットを更新してブレンド継続（簡易実装）
			m_nextPlayNo = no;
			m_animes[m_nextPlayNo].nowTime = 0.0f;
			m_animes[m_nextPlayNo].isLoop = loop;
			m_animes[m_nextPlayNo].speed = speed;

			m_transitionDuration = transitionDuration;
			m_transitionTime = 0.0f;
		}
		else
		{
			// 即時切り替え
			m_playNo = no;
			m_nextPlayNo = ANIME_NONE;
			m_animes[no].nowTime = 0.0f;
			m_animes[no].isLoop = loop;
			m_animes[no].speed = speed;
		}
	}

	void Model::Play(const std::string& animName, bool loop, float speed, float transitionDuration)
	{
		// 1. 既に読み込み済みのアニメーションから検索
		auto it = std::find_if(m_animes.begin(), m_animes.end(), [&](const Animation& a) { return a.name == animName; });
		if (it != m_animes.end()) {
			Play((AnimeNo)std::distance(m_animes.begin(), it), loop, speed, transitionDuration);
			return;
		}

		// 2. 見つからない場合、外部ファイルとしてロードを試みる
		namespace fs = std::filesystem;

		// 検索パスの候補
		std::vector<std::string> searchPaths = {
			animName,									// そのまま
			"Resources/Game/Animations/" + animName,	// アニメーションフォルダ
			"Resources/Game/Models/" + animName			// モデルフォルダ
		};

		for (const auto& path : searchPaths)
		{
			if (fs::exists(path))
			{
				// ファイルが見つかったら追加ロード
				AnimeNo newNo = AddAnimation(path);
				if (newNo != ANIME_NONE)
				{
					m_animes[newNo].name = animName;

					// 再生
					Play(newNo, loop, speed, transitionDuration);
					return;
				}
			}
		}
	}

	void Model::Step(float deltaTime)
	{
		// 1. 現在のアニメーション進行
		if (m_playNo != ANIME_NONE) {
			Animation& anim = m_animes[m_playNo];
			anim.nowTime += anim.speed * deltaTime * 24.0f;
			if (anim.nowTime >= anim.totalTime) {
				if (anim.isLoop) anim.nowTime = fmod(anim.nowTime, anim.totalTime);
				else anim.nowTime = anim.totalTime;
			}
		}

		// 2. 遷移処理 (クロスフェード)
		if (m_nextPlayNo != ANIME_NONE)
		{
			// 次のアニメーションも進行
			Animation& nextAnim = m_animes[m_nextPlayNo];
			nextAnim.nowTime += nextAnim.speed * deltaTime * 24.0f;
			if (nextAnim.nowTime >= nextAnim.totalTime) {
				if (nextAnim.isLoop) nextAnim.nowTime = fmod(nextAnim.nowTime, nextAnim.totalTime);
				else nextAnim.nowTime = nextAnim.totalTime;
			}

			// 遷移タイマー更新
			m_transitionTime += deltaTime;
			float t = (m_transitionDuration > 0.0f) ? (m_transitionTime / m_transitionDuration) : 1.0f;

			if (t >= 1.0f)
			{
				// 遷移完了: NextをCurrentにする
				m_playNo = m_nextPlayNo;
				m_nextPlayNo = ANIME_NONE;

				// 最終姿勢を適用
				EvaluateAnimation(m_playNo, m_animes[m_playNo].nowTime, m_currentTransforms);
			}
			else
			{
				// ブレンド計算
				// バッファ確保
				if (m_blendSrcTransforms.size() != m_nodes.size()) m_blendSrcTransforms.resize(m_nodes.size());
				if (m_blendDstTransforms.size() != m_nodes.size()) m_blendDstTransforms.resize(m_nodes.size());

				// 両方の姿勢を計算
				EvaluateAnimation(m_playNo, m_animes[m_playNo].nowTime, m_blendSrcTransforms);
				EvaluateAnimation(m_nextPlayNo, m_animes[m_nextPlayNo].nowTime, m_blendDstTransforms);

				// ブレンドして m_currentTransforms に格納
				BlendTransforms(m_blendSrcTransforms, m_blendDstTransforms, t, m_currentTransforms);
			}
		}
		else if (m_playNo != ANIME_NONE)
		{
			// 通常再生
			EvaluateAnimation(m_playNo, m_animes[m_playNo].nowTime, m_currentTransforms);
		}

		UpdateNodeTransforms();
	}

	void Model::EvaluateAnimation(AnimeNo no, float time, std::vector<TransformData>& outTransforms)
	{
		// デフォルト姿勢で初期化
		if (outTransforms.size() != m_nodes.size()) outTransforms = m_defaultTransforms;
		else std::copy(m_defaultTransforms.begin(), m_defaultTransforms.end(), outTransforms.begin());

		if (no < 0 || no >= (int)m_animes.size()) return;

		const Animation& anim = m_animes[no];

		for (const auto& ch : anim.channels)
		{
			if (ch.nodeIndex < 0 || ch.nodeIndex >= (int)outTransforms.size()) continue;
			TransformData& trans = outTransforms[ch.nodeIndex];

			// Position
			if (!ch.positionKeys.empty()) {
				// (ループ処理込みの補間ロジック)
				auto it = std::upper_bound(ch.positionKeys.begin(), ch.positionKeys.end(), time,
					[](float val, const std::pair<float, XMFLOAT3>& key) { return val < key.first; });
				size_t nextIdx = 0, prevIdx = 0;
				if (it == ch.positionKeys.end()) { nextIdx = 0; prevIdx = ch.positionKeys.size() - 1; }
				else if (it == ch.positionKeys.begin()) { prevIdx = 0; nextIdx = 0; }
				else { nextIdx = std::distance(ch.positionKeys.begin(), it); prevIdx = nextIdx - 1; }

				const auto& nextKey = ch.positionKeys[nextIdx];
				const auto& prevKey = ch.positionKeys[prevIdx];
				float dt = nextKey.first - prevKey.first;
				if (dt < 0.0f) dt += anim.totalTime; // Loop wrap
				float t = (dt > 0.0001f) ? (time - prevKey.first) : 0.0f;
				if (t < 0.0f) t += anim.totalTime;
				t = (dt > 0.0001f) ? t / dt : 0.0f;
				t = std::max(0.0f, std::min(t, 1.0f));
				trans.translate = Lerp3(prevKey.second, nextKey.second, t);
			}

			// Rotation
			if (!ch.rotationKeys.empty()) {
				auto it = std::upper_bound(ch.rotationKeys.begin(), ch.rotationKeys.end(), time,
					[](float val, const std::pair<float, XMFLOAT4>& key) { return val < key.first; });
				size_t nextIdx = 0, prevIdx = 0;
				if (it == ch.rotationKeys.end()) { nextIdx = 0; prevIdx = ch.rotationKeys.size() - 1; }
				else if (it == ch.rotationKeys.begin()) { prevIdx = 0; nextIdx = 0; }
				else { nextIdx = std::distance(ch.rotationKeys.begin(), it); prevIdx = nextIdx - 1; }

				const auto& nextKey = ch.rotationKeys[nextIdx];
				const auto& prevKey = ch.rotationKeys[prevIdx];
				float dt = nextKey.first - prevKey.first;
				if (dt < 0.0f) dt += anim.totalTime;
				float t = (dt > 0.0001f) ? (time - prevKey.first) : 0.0f;
				if (t < 0.0f) t += anim.totalTime;
				t = (dt > 0.0001f) ? t / dt : 0.0f;
				t = std::max(0.0f, std::min(t, 1.0f));
				trans.quaternion = Slerp4(prevKey.second, nextKey.second, t);
			}

			// Scale
			if (!ch.scalingKeys.empty()) {
				auto it = std::upper_bound(ch.scalingKeys.begin(), ch.scalingKeys.end(), time,
					[](float val, const std::pair<float, XMFLOAT3>& key) { return val < key.first; });
				size_t nextIdx = std::distance(ch.scalingKeys.begin(), it);
				if (it == ch.scalingKeys.end()) nextIdx = 0; // Loop check simplified
				size_t prevIdx = (nextIdx == 0) ? ch.scalingKeys.size() - 1 : nextIdx - 1;

				const auto& nextKey = ch.scalingKeys[nextIdx];
				const auto& prevKey = ch.scalingKeys[prevIdx];
				float dt = nextKey.first - prevKey.first;
				float t = (dt > 0.0001f) ? (time - prevKey.first) / dt : 0.0f;
				trans.scale = Lerp3(prevKey.second, nextKey.second, t);
			}
		}
	}

	// --- 追加: ブレンド関数 ---
	void Model::BlendTransforms(const std::vector<TransformData>& src, const std::vector<TransformData>& dst, float t, std::vector<TransformData>& outResult)
	{
		size_t count = outResult.size();
		for (size_t i = 0; i < count; ++i)
		{
			outResult[i].translate = Lerp3(src[i].translate, dst[i].translate, t);
			outResult[i].scale = Lerp3(src[i].scale, dst[i].scale, t);
			outResult[i].quaternion = Slerp4(src[i].quaternion, dst[i].quaternion, t);
		}
	}

	// --- 追加: ゲッター ---
	float Model::GetCurrentAnimationTime() const {
		if (m_playNo == ANIME_NONE) return 0.0f;
		return m_animes[m_playNo].nowTime;
	}
	float Model::GetCurrentAnimationLength() const {
		if (m_playNo == ANIME_NONE) return 1.0f;
		return m_animes[m_playNo].totalTime;
	}

	// 省略：Lerp3, Slerp4の実装
	XMFLOAT3 Model::Lerp3(const XMFLOAT3& a, const XMFLOAT3& b, float t) {
		XMVECTOR vA = XMLoadFloat3(&a);
		XMVECTOR vB = XMLoadFloat3(&b);
		XMFLOAT3 r; XMStoreFloat3(&r, XMVectorLerp(vA, vB, t)); return r;
	}
	XMFLOAT4 Model::Slerp4(const XMFLOAT4& a, const XMFLOAT4& b, float t) {
		DirectX::XMVECTOR vA = DirectX::XMLoadFloat4(&a);
		DirectX::XMVECTOR vB = DirectX::XMLoadFloat4(&b);

		// Slerp実行
		DirectX::XMVECTOR vResult = DirectX::XMQuaternionSlerp(vA, vB, t);

		// 正規化 (ここでもゼロチェック)
		DirectX::XMVECTOR Len = DirectX::XMQuaternionLength(vResult);
		float len;
		DirectX::XMStoreFloat(&len, Len);

		if (len < 0.0001f) {
			vResult = DirectX::XMQuaternionIdentity();
		}
		else {
			vResult = DirectX::XMQuaternionNormalize(vResult);
		}

		DirectX::XMFLOAT4 r;
		DirectX::XMStoreFloat4(&r, vResult);
		return r;
	}

	void Model::UpdateNodeTransforms()
	{
		static int frameCount = 0;
		frameCount++;

		std::function<void(int, DirectX::XMMATRIX)> Rec = [&](int idx, DirectX::XMMATRIX parent) {
			auto& t = m_currentTransforms[idx];

			// クォータニオンの安全性チェック
			DirectX::XMVECTOR Q = DirectX::XMLoadFloat4(&t.quaternion);
			DirectX::XMVECTOR Len = DirectX::XMQuaternionLength(Q);
			float len;
			DirectX::XMStoreFloat(&len, Len);
			if (len < 0.0001f) {
				Q = DirectX::XMQuaternionIdentity(); // 単位クォータニオンにリセット
			}
			else {
				Q = DirectX::XMQuaternionNormalize(Q); // 正規化
			}

			// スケールは1.0固定 (Assimp爆発対策)
			DirectX::XMMATRIX S = DirectX::XMMatrixScaling(t.scale.x, t.scale.y, t.scale.z);
			DirectX::XMMATRIX R = DirectX::XMMatrixRotationQuaternion(Q); // 安全なQを使用
			DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(t.translate.x, t.translate.y, t.translate.z);

			// ローカル行列
			DirectX::XMMATRIX localMat = S * R * T;

			// 3. 全体行列
			m_nodes[idx].mat = localMat * parent;

			// 子ノードへ再帰
			for (int child : m_nodes[idx].children) Rec(child, m_nodes[idx].mat);
			};

		// ルート呼び出し
		Rec(0, DirectX::XMMatrixScaling(m_loadScale, m_loadScale, m_loadScale));
	}
}