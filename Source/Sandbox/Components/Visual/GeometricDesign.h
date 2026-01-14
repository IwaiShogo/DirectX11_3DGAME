#pragma once
// 必須インクルード（マクロ使用のため）
#include "Engine/Scene/Components/Components.h"

namespace Arche {

	enum class GeoShape
	{
		Cube,
		Sphere,
		Cylinder,
		Capsule,
		Pyramid,
		Cone,
		Torus,
		Diamond,
	};

	/**
	 * @struct	GeometricDesign
	 * @brief
	 * プリミティブレンダラーを用いて描画するための定義データ
	 * モデルファイルをロードせず、プログラムで形状を制御します。
	 */
	struct GeometricDesign
	{
		GeoShape shapeType = GeoShape::Cube;
		XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };	// RGBA
		XMFLOAT3 size = { 1.0f, 1.0f, 1.0f };			// 形状固有のサイズ倍率
		bool isWireframe = false;						// ワイヤーフレーム表示

		// アニメーション用: 明滅や変形をプログラム制御するためのパラメータ
		bool ignoreTimeScale = false;	// ヒットストップ中もアニメーションするか
		float pulseSpeed = 0.0f;		// 0より大きいとサイズが明滅する
		float timer = 0.0f;				// アニメーション用内部タイマー

		// 被弾時などの点滅演出
		float flashTimer = 0.0f;

		GeometricDesign(GeoShape s = GeoShape::Cube, XMFLOAT4 c = { 1, 1, 1, 1 })
			: shapeType(s), color(c) {}
	};

	ARCHE_COMPONENT(GeometricDesign,
		REFLECT_VAR(shapeType)
		REFLECT_VAR(color)
		REFLECT_VAR(size)
		REFLECT_VAR(isWireframe)
		REFLECT_VAR(pulseSpeed)
		REFLECT_VAR(flashTimer)
	)
}

#include "Editor/Core/InspectorGui.h"
ARCHE_REGISTER_ENUM_NAMES(Arche::GeoShape, "Cube", "Sphere", "Cylinder", "Capsule", "Pyramid", "Cone", "Torus", "Diamond")