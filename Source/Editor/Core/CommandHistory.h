/*****************************************************************//**
 * @file	CommandHistory.h
 * @brief	Undo / Redoのためのコマンド履歴管理
 * 
 * @details	
 * 
 * ------------------------------------------------------------
 * @author	Iwai Shogo
 * ------------------------------------------------------------
 *********************************************************************/

#ifndef ___COMMAND_HISTORY_H___
#define ___COMMAND_HISTORY_H___

// ===== インクルード =====
#include "Engine/pch.h"
#include "Engine/Core/Base/Logger.h"

namespace Arche
{
	// コマンドインターフェース
	class ICommand
	{
	public:
		virtual ~ICommand() = default;
		virtual void Execute() = 0;
		virtual void Undo() = 0;
		// メモリ管理用: trueならUndoスタックから消えた時にdeleteされる
		virtual bool IsMerged() const { return false; }
	};

	class CommandHistory
	{
	public:
		static void Execute(std::shared_ptr<ICommand> cmd)
		{
			cmd->Execute();
			m_undoStack.push_back(cmd);
			m_redoStack.clear();	// 新しい操作をしたらRedoスタックはクリア

			// 履歴制限
			if (m_undoStack.size() > 50) m_undoStack.pop_front();
			SceneManager::Instance().SetDirty(true);
		}

		static void Undo()
		{
			if (m_undoStack.empty()) return;
			auto cmd = m_undoStack.back();
			m_undoStack.pop_back();
			cmd->Undo();
			m_redoStack.push_back(cmd);
			Logger::Log("Undo performed.");
		}

		static void Redo()
		{
			if (m_redoStack.empty()) return;
			auto cmd = m_redoStack.back();
			m_redoStack.pop_back();
			cmd->Execute();
			m_undoStack.push_back(cmd);
			Logger::Log("Redo performed.");
		}

		// 履歴の全消去（モード切替時などに使用）
		static void Clear()
		{
			m_undoStack.clear();
			m_redoStack.clear();
			Logger::Log("Command History Cleared.");
		}

	private:
		static inline std::deque<std::shared_ptr<ICommand>> m_undoStack;
		static inline std::deque<std::shared_ptr<ICommand>> m_redoStack;
	};

}	// namespace Arche

#endif // !___COMMAND_HISTORY_H___