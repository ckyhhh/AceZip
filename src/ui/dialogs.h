// ============================================================================
// dialogs.h - 各种对话框
//
// 包含：
// - 密码对话框
// - 解压对话框
// - 压缩对话框
// - 进度对话框
// - 关于对话框
// - 选项对话框
// - 文件属性对话框
// - 归档属性对话框
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#pragma once

#include "../core/archive.h"
#include <UIlib.h>
#include <string>
#include <vector>
#include <functional>

namespace bandzip {
namespace ui {

// ---------------------------------------------------------------------------
// 通用对话框函数（简化接口）
// ---------------------------------------------------------------------------
namespace dialogs {

// 文件打开对话框
tstring OpenFileDialog(HWND parent, const tstring& filter);
std::vector<tstring> OpenMultiFileDialog(HWND parent, const tstring& filter);
tstring SaveFileDialog(HWND parent, const tstring& filter,
                       const tstring& default_ext);

// 浏览文件夹
tstring BrowseFolderDialog(HWND parent, const tstring& title);

// 消息框
int MessageBox(HWND parent, const tstring& text, const tstring& title,
                UINT flags = MB_OK);
void ShowError(HWND parent, const tstring& title, const tstring& msg);
void ShowInfo(HWND parent, const tstring& title, const tstring& msg);
void ShowWarning(HWND parent, const tstring& title, const tstring& msg);

// 输入框
tstring InputBox(HWND parent, const tstring& title, const tstring& prompt,
                 const tstring& default_text = tstring());

// 密码对话框
bool ShowPasswordDialog(HWND parent, tstring& password);

// 解压对话框
bool ShowExtractDialog(HWND parent, const tstring& archive_path,
                       tstring& output_dir, ExtractOptions& opts);

// 压缩对话框
bool ShowCompressDialog(HWND parent, const std::vector<tstring>& files,
                        CreateOptions& opts);

// 选项对话框
bool ShowOptionsDialog(HWND parent);

// 关于对话框
void ShowAboutDialog(HWND parent);

// 更新检查对话框
void ShowUpdateDialog(HWND parent);

// 文件属性
void ShowEntryProperties(HWND parent, const ArchiveEntry& entry);

// 归档属性
void ShowArchiveProperties(HWND parent, std::unique_ptr<IArchive>& archive);

} // namespace dialogs

// ---------------------------------------------------------------------------
// 进度对话框
// ---------------------------------------------------------------------------
class ProgressDialog : public DuiLib::CWindowWnd,
                        public DuiLib::INotifyUI {
public:
    ProgressDialog();
    ~ProgressDialog() override;

    LPCTSTR GetWindowClassName() const override;
    void Notify(DuiLib::TNotifyUI& msg) override;
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override;
    void OnFinalMessage(HWND hWnd) override;

    bool Create(HWND parent, const tstring& title);
    void Update(const ProgressInfo& info);
    void Close();
    bool IsCancelled() const { return cancelled_; }

    void SetCancelCallback(std::function<void()> cb) {
        cancel_cb_ = std::move(cb);
    }

private:
    DuiLib::CPaintManagerUI paint_manager_;
    DuiLib::CLabelUI* lbl_file_ = nullptr;
    DuiLib::CProgressUI* progress_ = nullptr;
    DuiLib::CLabelUI* lbl_percent_ = nullptr;
    DuiLib::CLabelUI* lbl_speed_ = nullptr;
    DuiLib::CLabelUI* lbl_time_left_ = nullptr;
    DuiLib::CButtonUI* btn_cancel_ = nullptr;

    std::atomic<bool> cancelled_{false};
    std::function<void()> cancel_cb_;

    u64 start_time_ = 0;
    u64 last_bytes_ = 0;
    u64 last_time_ = 0;
    double speed_ = 0;
};

} // namespace ui
} // namespace bandzip
