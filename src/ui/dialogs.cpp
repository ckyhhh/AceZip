// ============================================================================
// dialogs.cpp - 对话框实现
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#include "dialogs.h"
#include "resource.h"
#include "../utils/string_utils.h"
#include "../utils/file_utils.h"
#include "../utils/path_utils.h"
#include "../utils/logger.h"

#include <shellapi.h>
#include <shlobj.h>
#include <commdlg.h>
#include <commctrl.h>
#include <algorithm>
#include <chrono>

namespace bandzip {
namespace ui {
namespace dialogs {

// ---------------------------------------------------------------------------
// 文件打开对话框
// ---------------------------------------------------------------------------
tstring OpenFileDialog(HWND parent, const tstring& filter) {
    tchar buf[MAX_PATH] = {0};
    OPENFILENAME ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = parent;
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileName(&ofn)) {
        return tstring(buf);
    }
    return tstring();
}

std::vector<tstring> OpenMultiFileDialog(HWND parent, const tstring& filter) {
    std::vector<tstring> result;
    tchar buf[32768] = {0};
    OPENFILENAME ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = parent;
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = buf;
    ofn.nMaxFile = 32768;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT |
                OFN_PATHMUSTEXIST;

    if (GetOpenFileName(&ofn)) {
        tstring s(buf);
        size_t pos = 0;
        size_t next = s.find(_T('\0'), pos);

        if (next == pos) {
            // 只有一个文件
            result.push_back(s);
        } else {
            // 多个文件，第一个是目录
            tstring dir = s.substr(pos, next - pos);
            pos = next + 1;
            while (pos < s.size()) {
                next = s.find(_T('\0'), pos);
                if (next == tstring::npos) break;
                tstring name = s.substr(pos, next - pos);
                if (name.empty()) break;
                result.push_back(util::join_path(dir, name));
                pos = next + 1;
            }
        }
    }
    return result;
}

tstring SaveFileDialog(HWND parent, const tstring& filter,
                        const tstring& default_ext) {
    tchar buf[MAX_PATH] = {0};
    OPENFILENAME ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = parent;
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = default_ext.c_str();
    ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (GetSaveFileName(&ofn)) {
        return tstring(buf);
    }
    return tstring();
}

// ---------------------------------------------------------------------------
// 浏览文件夹
// ---------------------------------------------------------------------------
tstring BrowseFolderDialog(HWND parent, const tstring& title) {
    tstring result;
    BROWSEINFO bi = {};
    bi.hwndOwner = parent;
    bi.lpszTitle = title.c_str();
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;

    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
    if (pidl) {
        tchar path[MAX_PATH];
        if (SHGetPathFromIDList(pidl, path)) {
            result = path;
        }
        CoTaskMemFree(pidl);
    }
    return result;
}

// ---------------------------------------------------------------------------
// 消息框
// ---------------------------------------------------------------------------
int MessageBox(HWND parent, const tstring& text, const tstring& title, UINT flags) {
    return ::MessageBox(parent, text.c_str(), title.c_str(), flags);
}

void ShowError(HWND parent, const tstring& title, const tstring& msg) {
    ::MessageBox(parent, msg.c_str(), title.c_str(), MB_OK | MB_ICONERROR);
}

void ShowInfo(HWND parent, const tstring& title, const tstring& msg) {
    ::MessageBox(parent, msg.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
}

void ShowWarning(HWND parent, const tstring& title, const tstring& msg) {
    ::MessageBox(parent, msg.c_str(), title.c_str(), MB_OK | MB_ICONWARNING);
}

// ---------------------------------------------------------------------------
// 输入框
// ---------------------------------------------------------------------------
tstring InputBox(HWND parent, const tstring& title, const tstring& prompt,
                  const tstring& default_text) {
    // 简化实现：使用一个简单的对话框
    // 实际生产中应该用 Duilib 创建一个漂亮的输入框
    class InputDlg : public DuiLib::CWindowWnd,
                      public DuiLib::INotifyUI {
    public:
        InputDlg(HWND parent, const tstring& title,
                  const tstring& prompt, const tstring& def)
            : parent_(parent), title_(title), prompt_(prompt), default_(def) {}

        LPCTSTR GetWindowClassName() const override {
            return _T("BandzipCloneInputDlg");
        }

        void Notify(DuiLib::TNotifyUI& msg) override {
            if (msg.sType == _T("click")) {
                DuiLib::CDuiString name = msg.pSender->GetName();
                if (name == _T("btn_ok")) {
                    if (edit_) result_ = edit_->GetText();
                    Close();
                } else if (name == _T("btn_cancel")) {
                    Close();
                }
            }
        }

        LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override {
            if (uMsg == WM_CREATE) {
                paint_manager_.Init(m_hWnd);

                DuiLib::CVerticalLayoutUI* root = new DuiLib::CVerticalLayoutUI;
                root->SetInset(DuiLib::CDuiRect(20, 20, 20, 20));
                root->SetFixedWidth(400);
                root->SetFixedHeight(150);

                DuiLib::CLabelUI* lbl = new DuiLib::CLabelUI;
                lbl->SetText(prompt_.c_str());
                lbl->SetFixedHeight(30);
                root->Add(lbl);

                edit_ = new DuiLib::CEditUI;
                edit_->SetName(_T("edit_input"));
                edit_->SetText(default_.c_str());
                edit_->SetFixedHeight(25);
                root->Add(edit_);

                DuiLib::CHorizontalLayoutUI* btns = new DuiLib::CHorizontalLayoutUI;
                btns->SetFixedHeight(35);

                DuiLib::CButtonUI* ok = new DuiLib::CButtonUI;
                ok->SetName(_T("btn_ok"));
                ok->SetText(_T("确定"));
                ok->SetFixedWidth(80);
                btns->Add(ok);

                DuiLib::CButtonUI* cancel = new DuiLib::CButtonUI;
                cancel->SetName(_T("btn_cancel"));
                cancel->SetText(_T("取消"));
                cancel->SetFixedWidth(80);
                btns->Add(cancel);

                root->Add(btns);

                paint_manager_.AttachDialog(root);
                paint_manager_.AddNotifier(this);

                SetWindowText(m_hWnd, title_.c_str());
                return 0;
            }
            LRESULT lRes = 0;
            if (paint_manager_.MessageHandler(uMsg, wParam, lParam, lRes)) {
                return lRes;
            }
            return __super::HandleMessage(uMsg, wParam, lParam);
        }

        void OnFinalMessage(HWND) override { delete this; }

        tstring GetResult() const { return result_; }

    private:
        HWND parent_;
        tstring title_;
        tstring prompt_;
        tstring default_;
        tstring result_;
        DuiLib::CPaintManagerUI paint_manager_;
        DuiLib::CEditUI* edit_ = nullptr;
    };

    InputDlg dlg(parent, title, prompt, default_text);
    dlg.Create(parent, title.c_str(), WS_POPUP | WS_CAPTION | WS_SYSMENU,
                WS_EX_TOOLWINDOW);
    dlg.CenterWindow();
    dlg.ShowModal();
    return dlg.GetResult();
}

// ---------------------------------------------------------------------------
// 密码对话框
// ---------------------------------------------------------------------------
class PasswordDialog : public DuiLib::CWindowWnd,
                        public DuiLib::INotifyUI {
public:
    PasswordDialog(tstring& password) : password_(password) {}

    LPCTSTR GetWindowClassName() const override {
        return _T("BandzipClonePasswordDlg");
    }

    void Notify(DuiLib::TNotifyUI& msg) override {
        if (msg.sType == _T("click")) {
            DuiLib::CDuiString name = msg.pSender->GetName();
            if (name == _T("btn_ok")) {
                if (edit_pwd_) password_ = edit_pwd_->GetText();
                result_ = true;
                Close();
            } else if (name == _T("btn_cancel")) {
                result_ = false;
                Close();
            }
        }
    }

    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override {
        if (uMsg == WM_CREATE) {
            paint_manager_.Init(m_hWnd);

            DuiLib::CVerticalLayoutUI* root = new DuiLib::CVerticalLayoutUI;
            root->SetInset(DuiLib::CDuiRect(20, 20, 20, 20));
            root->SetFixedWidth(380);
            root->SetFixedHeight(180);

            DuiLib::CLabelUI* lbl = new DuiLib::CLabelUI;
            lbl->SetText(_T("请输入密码："));
            lbl->SetFixedHeight(25);
            root->Add(lbl);

            edit_pwd_ = new DuiLib::CEditUI;
            edit_pwd_->SetName(_T("edit_password"));
            edit_pwd_->SetPasswordMode(true);
            edit_pwd_->SetFixedHeight(28);
            root->Add(edit_pwd_);

            DuiLib::CLabelUI* lbl2 = new DuiLib::CLabelUI;
            lbl2->SetText(_T("确认密码："));
            lbl2->SetFixedHeight(25);
            root->Add(lbl2);

            edit_pwd2_ = new DuiLib::CEditUI;
            edit_pwd2_->SetName(_T("edit_password2"));
            edit_pwd2_->SetPasswordMode(true);
            edit_pwd2_->SetFixedHeight(28);
            root->Add(edit_pwd2_);

            DuiLib::CHorizontalLayoutUI* btns = new DuiLib::CHorizontalLayoutUI;
            btns->SetFixedHeight(35);

            DuiLib::CButtonUI* ok = new DuiLib::CButtonUI;
            ok->SetName(_T("btn_ok"));
            ok->SetText(_T("确定"));
            ok->SetFixedWidth(80);
            btns->Add(ok);

            DuiLib::CButtonUI* cancel = new DuiLib::CButtonUI;
            cancel->SetName(_T("btn_cancel"));
            cancel->SetText(_T("取消"));
            cancel->SetFixedWidth(80);
            btns->Add(cancel);

            root->Add(btns);

            paint_manager_.AttachDialog(root);
            paint_manager_.AddNotifier(this);

            SetWindowText(m_hWnd, _T("输入密码"));
            return 0;
        }
        LRESULT lRes = 0;
        if (paint_manager_.MessageHandler(uMsg, wParam, lParam, lRes)) {
            return lRes;
        }
        return __super::HandleMessage(uMsg, wParam, lParam);
    }

    void OnFinalMessage(HWND) override { delete this; }

    bool GetResult() const { return result_; }

private:
    tstring& password_;
    bool result_ = false;
    DuiLib::CPaintManagerUI paint_manager_;
    DuiLib::CEditUI* edit_pwd_ = nullptr;
    DuiLib::CEditUI* edit_pwd2_ = nullptr;
};

bool ShowPasswordDialog(HWND parent, tstring& password) {
    PasswordDialog dlg(password);
    dlg.Create(parent, _T("Password"), WS_POPUP | WS_CAPTION | WS_SYSMENU,
                WS_EX_TOOLWINDOW);
    dlg.CenterWindow();
    dlg.ShowModal();
    return dlg.GetResult();
}

// ---------------------------------------------------------------------------
// 解压对话框
// ---------------------------------------------------------------------------
class ExtractDialog : public DuiLib::CWindowWnd,
                       public DuiLib::INotifyUI {
public:
    ExtractDialog(const tstring& archive_path, tstring& output_dir,
                   ExtractOptions& opts)
        : archive_path_(archive_path), output_dir_(output_dir), opts_(opts) {}

    LPCTSTR GetWindowClassName() const override {
        return _T("BandzipCloneExtractDlg");
    }

    void Notify(DuiLib::TNotifyUI& msg) override {
        if (msg.sType == _T("click")) {
            DuiLib::CDuiString name = msg.pSender->GetName();
            if (name == _T("btn_browse")) {
                tstring dir = BrowseFolderDialog(m_hWnd, _T("选择解压目录"));
                if (!dir.empty() && edit_dir_) {
                    edit_dir_->SetText(dir.c_str());
                }
            } else if (name == _T("btn_ok")) {
                if (edit_dir_) output_dir_ = edit_dir_->GetText();
                if (chk_keep_paths_ && chk_keep_paths_->GetCheck()) {
                    opts_.keep_paths = true;
                } else {
                    opts_.keep_paths = false;
                }
                if (chk_overwrite_ && chk_overwrite_->GetCheck()) {
                    opts_.overwrite = OverwriteMode::Overwrite;
                } else {
                    opts_.overwrite = OverwriteMode::Ask;
                }
                if (chk_skip_existing_ && chk_skip_existing_->GetCheck()) {
                    opts_.skip_existing = true;
                } else {
                    opts_.skip_existing = false;
                }
                if (chk_open_folder_ && chk_open_folder_->GetCheck()) {
                    opts_.open_folder_after = true;
                } else {
                    opts_.open_folder_after = false;
                }
                if (chk_delete_archive_ && chk_delete_archive_->GetCheck()) {
                    opts_.delete_archive_after = true;
                } else {
                    opts_.delete_archive_after = false;
                }
                result_ = true;
                Close();
            } else if (name == _T("btn_cancel")) {
                result_ = false;
                Close();
            }
        }
    }

    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override {
        if (uMsg == WM_CREATE) {
            paint_manager_.Init(m_hWnd);

            DuiLib::CVerticalLayoutUI* root = new DuiLib::CVerticalLayoutUI;
            root->SetInset(DuiLib::CDuiRect(20, 20, 20, 20));
            root->SetFixedWidth(500);
            root->SetFixedHeight(400);

            // 标题
            DuiLib::CLabelUI* lbl_title = new DuiLib::CLabelUI;
            lbl_title->SetText(_T("解压到："));
            lbl_title->SetFixedHeight(25);
            root->Add(lbl_title);

            // 目标目录
            DuiLib::CHorizontalLayoutUI* dir_row = new DuiLib::CHorizontalLayoutUI;
            dir_row->SetFixedHeight(30);

            edit_dir_ = new DuiLib::CEditUI;
            edit_dir_->SetName(_T("edit_dir"));
            edit_dir_->SetText(output_dir_.c_str());
            dir_row->Add(edit_dir_);

            DuiLib::CButtonUI* btn_browse = new DuiLib::CButtonUI;
            btn_browse->SetName(_T("btn_browse"));
            btn_browse->SetText(_T("浏览..."));
            btn_browse->SetFixedWidth(80);
            dir_row->Add(btn_browse);

            root->Add(dir_row);

            // 选项
            chk_keep_paths_ = new DuiLib::CCheckBoxUI;
            chk_keep_paths_->SetName(_T("chk_keep_paths"));
            chk_keep_paths_->SetText(_T("保留目录结构"));
            chk_keep_paths_->SetCheck(opts_.keep_paths);
            chk_keep_paths_->SetFixedHeight(25);
            root->Add(chk_keep_paths_);

            chk_overwrite_ = new DuiLib::CCheckBoxUI;
            chk_overwrite_->SetName(_T("chk_overwrite"));
            chk_overwrite_->SetText(_T("覆盖已存在的文件"));
            chk_overwrite_->SetCheck(opts_.overwrite == OverwriteMode::Overwrite);
            chk_overwrite_->SetFixedHeight(25);
            root->Add(chk_overwrite_);

            chk_skip_existing_ = new DuiLib::CCheckBoxUI;
            chk_skip_existing_->SetName(_T("chk_skip_existing"));
            chk_skip_existing_->SetText(_T("跳过已存在的文件"));
            chk_skip_existing_->SetCheck(opts_.skip_existing);
            chk_skip_existing_->SetFixedHeight(25);
            root->Add(chk_skip_existing_);

            chk_open_folder_ = new DuiLib::CCheckBoxUI;
            chk_open_folder_->SetName(_T("chk_open_folder"));
            chk_open_folder_->SetText(_T("完成后打开文件夹"));
            chk_open_folder_->SetCheck(opts_.open_folder_after);
            chk_open_folder_->SetFixedHeight(25);
            root->Add(chk_open_folder_);

            chk_delete_archive_ = new DuiLib::CCheckBoxUI;
            chk_delete_archive_->SetName(_T("chk_delete_archive"));
            chk_delete_archive_->SetText(_T("完成后删除归档"));
            chk_delete_archive_->SetCheck(opts_.delete_archive_after);
            chk_delete_archive_->SetFixedHeight(25);
            root->Add(chk_delete_archive_);

            // 按钮
            DuiLib::CHorizontalLayoutUI* btns = new DuiLib::CHorizontalLayoutUI;
            btns->SetFixedHeight(35);

            DuiLib::CButtonUI* ok = new DuiLib::CButtonUI;
            ok->SetName(_T("btn_ok"));
            ok->SetText(_T("确定"));
            ok->SetFixedWidth(80);
            btns->Add(ok);

            DuiLib::CButtonUI* cancel = new DuiLib::CButtonUI;
            cancel->SetName(_T("btn_cancel"));
            cancel->SetText(_T("取消"));
            cancel->SetFixedWidth(80);
            btns->Add(cancel);

            root->Add(btns);

            paint_manager_.AttachDialog(root);
            paint_manager_.AddNotifier(this);

            SetWindowText(m_hWnd, _T("解压"));
            return 0;
        }
        LRESULT lRes = 0;
        if (paint_manager_.MessageHandler(uMsg, wParam, lParam, lRes)) {
            return lRes;
        }
        return __super::HandleMessage(uMsg, wParam, lParam);
    }

    void OnFinalMessage(HWND) override { delete this; }

    bool GetResult() const { return result_; }

private:
    tstring archive_path_;
    tstring& output_dir_;
    ExtractOptions& opts_;
    bool result_ = false;
    DuiLib::CPaintManagerUI paint_manager_;
    DuiLib::CEditUI* edit_dir_ = nullptr;
    DuiLib::CCheckBoxUI* chk_keep_paths_ = nullptr;
    DuiLib::CCheckBoxUI* chk_overwrite_ = nullptr;
    DuiLib::CCheckBoxUI* chk_skip_existing_ = nullptr;
    DuiLib::CCheckBoxUI* chk_open_folder_ = nullptr;
    DuiLib::CCheckBoxUI* chk_delete_archive_ = nullptr;
};

bool ShowExtractDialog(HWND parent, const tstring& archive_path,
                        tstring& output_dir, ExtractOptions& opts) {
    if (output_dir.empty()) {
        output_dir = util::get_dirname(archive_path);
    }
    ExtractDialog dlg(archive_path, output_dir, opts);
    dlg.Create(parent, _T("Extract"), WS_POPUP | WS_CAPTION | WS_SYSMENU,
                WS_EX_TOOLWINDOW);
    dlg.CenterWindow();
    dlg.ShowModal();
    return dlg.GetResult();
}

// ---------------------------------------------------------------------------
// 压缩对话框
// ---------------------------------------------------------------------------
class CompressDialog : public DuiLib::CWindowWnd,
                        public DuiLib::INotifyUI {
public:
    CompressDialog(const std::vector<tstring>& files, CreateOptions& opts)
        : files_(files), opts_(opts) {}

    LPCTSTR GetWindowClassName() const override {
        return _T("BandzipCloneCompressDlg");
    }

    void Notify(DuiLib::TNotifyUI& msg) override {
        if (msg.sType == _T("click")) {
            DuiLib::CDuiString name = msg.pSender->GetName();
            if (name == _T("btn_browse")) {
                tstring default_name = _T("archive.zip");
                if (!files_.empty()) {
                    tstring base = util::get_basename(files_[0]);
                    tstring ext = util::get_extension_lower(base);
                    if (!ext.empty()) {
                        base = base.substr(0, base.length() - ext.length());
                    }
                    default_name = base + _T(".zip");
                }
                tstring path = SaveFileDialog(m_hWnd,
                    _T("ZIP (*.zip)|*.zip|7Z (*.7z)|*.7z|TAR (*.tar)|*.tar|GZ (*.gz)|*.gz|XZ (*.xz)|*.xz|ZSTD (*.zst)|*.zst|LZ4 (*.lz4)|*.lz4|"),
                    _T("zip"));
                if (!path.empty() && edit_path_) {
                    edit_path_->SetText(path.c_str());
                    // 根据扩展名更新格式
                    tstring ext = util::get_extension_lower(path);
                    if (ext == _T(".zip")) opts_.format = ArchiveFormat::Zip;
                    else if (ext == _T(".7z")) opts_.format = ArchiveFormat::SevenZip;
                    else if (ext == _T(".tar")) opts_.format = ArchiveFormat::Tar;
                    else if (ext == _T(".gz")) opts_.format = ArchiveFormat::Gzip;
                    else if (ext == _T(".xz")) opts_.format = ArchiveFormat::Xz;
                    else if (ext == _T(".zst")) opts_.format = ArchiveFormat::Zstd;
                    else if (ext == _T(".lz4")) opts_.format = ArchiveFormat::Lz4;
                }
            } else if (name == _T("btn_ok")) {
                if (edit_path_) opts_.archive_path = edit_path_->GetText();
                if (combo_level_) {
                    int idx = combo_level_->GetCurSel();
                    if (idx >= 0) opts_.level = idx;
                }
                if (combo_method_) {
                    int idx = combo_method_->GetCurSel();
                    if (idx >= 0) opts_.method = static_cast<CompressionMethod>(idx);
                }
                if (chk_solid_ && chk_solid_->GetCheck()) {
                    opts_.solid = true;
                }
                if (chk_encrypt_ && chk_encrypt_->GetCheck()) {
                    opts_.encrypt = true;
                    if (edit_pwd_) opts_.password = edit_pwd_->GetText();
                }
                if (chk_volume_ && chk_volume_->GetCheck()) {
                    opts_.volume_size = 100 * 1024 * 1024;  // 默认 100MB
                    if (edit_volume_) {
                        tstring v = edit_volume_->GetText();
                        // 解析大小
                        opts_.volume_size = _ttoi(v.c_str()) * 1024 * 1024;
                    }
                }
                result_ = true;
                Close();
            } else if (name == _T("btn_cancel")) {
                result_ = false;
                Close();
            }
        }
    }

    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override {
        if (uMsg == WM_CREATE) {
            paint_manager_.Init(m_hWnd);

            DuiLib::CVerticalLayoutUI* root = new DuiLib::CVerticalLayoutUI;
            root->SetInset(DuiLib::CDuiRect(20, 20, 20, 20));
            root->SetFixedWidth(520);
            root->SetFixedHeight(500);

            // 归档路径
            DuiLib::CLabelUI* lbl_path = new DuiLib::CLabelUI;
            lbl_path->SetText(_T("归档文件："));
            lbl_path->SetFixedHeight(25);
            root->Add(lbl_path);

            DuiLib::CHorizontalLayoutUI* path_row = new DuiLib::CHorizontalLayoutUI;
            path_row->SetFixedHeight(30);

            edit_path_ = new DuiLib::CEditUI;
            edit_path_->SetName(_T("edit_path"));
            edit_path_->SetText(opts_.archive_path.c_str());
            path_row->Add(edit_path_);

            DuiLib::CButtonUI* btn_browse = new DuiLib::CButtonUI;
            btn_browse->SetName(_T("btn_browse"));
            btn_browse->SetText(_T("浏览..."));
            btn_browse->SetFixedWidth(80);
            path_row->Add(btn_browse);

            root->Add(path_row);

            // 压缩级别
            DuiLib::CLabelUI* lbl_level = new DuiLib::CLabelUI;
            lbl_level->SetText(_T("压缩级别："));
            lbl_level->SetFixedHeight(25);
            root->Add(lbl_level);

            combo_level_ = new DuiLib::CComboUI;
            combo_level_->SetName(_T("combo_level"));
            combo_level_->Add(_T("0 - 仅存储"));
            combo_level_->Add(_T("1 - 最快"));
            combo_level_->Add(_T("2"));
            combo_level_->Add(_T("3"));
            combo_level_->Add(_T("4"));
            combo_level_->Add(_T("5 - 标准"));
            combo_level_->Add(_T("6"));
            combo_level_->Add(_T("7"));
            combo_level_->Add(_T("8"));
            combo_level_->Add(_T("9 - 最大"));
            combo_level_->SelectItem(opts_.level);
            combo_level_->SetFixedHeight(25);
            root->Add(combo_level_);

            // 压缩方法
            DuiLib::CLabelUI* lbl_method = new DuiLib::CLabelUI;
            lbl_method->SetText(_T("压缩方法："));
            lbl_method->SetFixedHeight(25);
            root->Add(lbl_method);

            combo_method_ = new DuiLib::CComboUI;
            combo_method_->SetName(_T("combo_method"));
            combo_method_->Add(_T("Copy"));
            combo_method_->Add(_T("Deflate"));
            combo_method_->Add(_T("Deflate64"));
            combo_method_->Add(_T("Bzip2"));
            combo_method_->Add(_T("LZMA"));
            combo_method_->Add(_T("LZMA2"));
            combo_method_->Add(_T("PPMd"));
            combo_method_->Add(_T("Zstd"));
            combo_method_->Add(_T("LZ4"));
            combo_method_->SelectItem(static_cast<int>(opts_.method));
            combo_method_->SetFixedHeight(25);
            root->Add(combo_method_);

            // 固实压缩
            chk_solid_ = new DuiLib::CCheckBoxUI;
            chk_solid_->SetName(_T("chk_solid"));
            chk_solid_->SetText(_T("固实压缩（仅 7z）"));
            chk_solid_->SetCheck(opts_.solid);
            chk_solid_->SetFixedHeight(25);
            root->Add(chk_solid_);

            // 加密
            chk_encrypt_ = new DuiLib::CCheckBoxUI;
            chk_encrypt_->SetName(_T("chk_encrypt"));
            chk_encrypt_->SetText(_T("加密文件"));
            chk_encrypt_->SetCheck(opts_.encrypt);
            chk_encrypt_->SetFixedHeight(25);
            root->Add(chk_encrypt_);

            edit_pwd_ = new DuiLib::CEditUI;
            edit_pwd_->SetName(_T("edit_pwd"));
            edit_pwd_->SetPasswordMode(true);
            edit_pwd_->SetFixedHeight(25);
            edit_pwd_->SetText(opts_.password.c_str());
            root->Add(edit_pwd_);

            // 分卷
            chk_volume_ = new DuiLib::CCheckBoxUI;
            chk_volume_->SetName(_T("chk_volume"));
            chk_volume_->SetText(_T("分卷压缩"));
            chk_volume_->SetCheck(false);
            chk_volume_->SetFixedHeight(25);
            root->Add(chk_volume_);

            DuiLib::CHorizontalLayoutUI* vol_row = new DuiLib::CHorizontalLayoutUI;
            vol_row->SetFixedHeight(25);

            edit_volume_ = new DuiLib::CEditUI;
            edit_volume_->SetName(_T("edit_volume"));
            edit_volume_->SetText(_T("100"));
            edit_volume_->SetFixedWidth(100);
            vol_row->Add(edit_volume_);

            DuiLib::CLabelUI* lbl_mb = new DuiLib::CLabelUI;
            lbl_mb->SetText(_T(" MB"));
            vol_row->Add(lbl_mb);

            root->Add(vol_row);

            // 按钮
            DuiLib::CHorizontalLayoutUI* btns = new DuiLib::CHorizontalLayoutUI;
            btns->SetFixedHeight(35);

            DuiLib::CButtonUI* ok = new DuiLib::CButtonUI;
            ok->SetName(_T("btn_ok"));
            ok->SetText(_T("确定"));
            ok->SetFixedWidth(80);
            btns->Add(ok);

            DuiLib::CButtonUI* cancel = new DuiLib::CButtonUI;
            cancel->SetName(_T("btn_cancel"));
            cancel->SetText(_T("取消"));
            cancel->SetFixedWidth(80);
            btns->Add(cancel);

            root->Add(btns);

            paint_manager_.AttachDialog(root);
            paint_manager_.AddNotifier(this);

            SetWindowText(m_hWnd, _T("创建压缩包"));
            return 0;
        }
        LRESULT lRes = 0;
        if (paint_manager_.MessageHandler(uMsg, wParam, lParam, lRes)) {
            return lRes;
        }
        return __super::HandleMessage(uMsg, wParam, lParam);
    }

    void OnFinalMessage(HWND) override { delete this; }

    bool GetResult() const { return result_; }

private:
    std::vector<tstring> files_;
    CreateOptions& opts_;
    bool result_ = false;
    DuiLib::CPaintManagerUI paint_manager_;
    DuiLib::CEditUI* edit_path_ = nullptr;
    DuiLib::CComboUI* combo_level_ = nullptr;
    DuiLib::CComboUI* combo_method_ = nullptr;
    DuiLib::CCheckBoxUI* chk_solid_ = nullptr;
    DuiLib::CCheckBoxUI* chk_encrypt_ = nullptr;
    DuiLib::CEditUI* edit_pwd_ = nullptr;
    DuiLib::CCheckBoxUI* chk_volume_ = nullptr;
    DuiLib::CEditUI* edit_volume_ = nullptr;
};

bool ShowCompressDialog(HWND parent, const std::vector<tstring>& files,
                        CreateOptions& opts) {
    CompressDialog dlg(files, opts);
    dlg.Create(parent, _T("Compress"), WS_POPUP | WS_CAPTION | WS_SYSMENU,
                WS_EX_TOOLWINDOW);
    dlg.CenterWindow();
    dlg.ShowModal();
    return dlg.GetResult();
}

// ---------------------------------------------------------------------------
// 选项对话框
// ---------------------------------------------------------------------------
class OptionsDialog : public DuiLib::CWindowWnd,
                       public DuiLib::INotifyUI {
public:
    LPCTSTR GetWindowClassName() const override {
        return _T("BandzipCloneOptionsDlg");
    }

    void Notify(DuiLib::TNotifyUI& msg) override {
        if (msg.sType == _T("click")) {
            DuiLib::CDuiString name = msg.pSender->GetName();
            if (name == _T("btn_ok")) {
                result_ = true;
                Close();
            } else if (name == _T("btn_cancel")) {
                result_ = false;
                Close();
            }
        }
    }

    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override {
        if (uMsg == WM_CREATE) {
            paint_manager_.Init(m_hWnd);

            DuiLib::CVerticalLayoutUI* root = new DuiLib::CVerticalLayoutUI;
            root->SetInset(DuiLib::CDuiRect(20, 20, 20, 20));
            root->SetFixedWidth(500);
            root->SetFixedHeight(400);

            DuiLib::CLabelUI* lbl = new DuiLib::CLabelUI;
            lbl->SetText(_T("选项设置"));
            lbl->SetFixedHeight(30);
            root->Add(lbl);

            // 默认格式
            DuiLib::CLabelUI* lbl_fmt = new DuiLib::CLabelUI;
            lbl_fmt->SetText(_T("默认压缩格式："));
            lbl_fmt->SetFixedHeight(25);
            root->Add(lbl_fmt);

            DuiLib::CComboUI* combo_fmt = new DuiLib::CComboUI;
            combo_fmt->Add(_T("ZIP"));
            combo_fmt->Add(_T("7Z"));
            combo_fmt->Add(_T("TAR"));
            combo_fmt->SelectItem(0);
            combo_fmt->SetFixedHeight(25);
            root->Add(combo_fmt);

            // 默认级别
            DuiLib::CLabelUI* lbl_lvl = new DuiLib::CLabelUI;
            lbl_lvl->SetText(_T("默认压缩级别："));
            lbl_lvl->SetFixedHeight(25);
            root->Add(lbl_lvl);

            DuiLib::CComboUI* combo_lvl = new DuiLib::CComboUI;
            for (int i = 0; i <= 9; ++i) {
                combo_lvl->Add(util::format(_T("%d"), i).c_str());
            }
            combo_lvl->SelectItem(5);
            combo_lvl->SetFixedHeight(25);
            root->Add(combo_lvl);

            // Shell 集成
            DuiLib::CCheckBoxUI* chk_shell = new DuiLib::CCheckBoxUI;
            chk_shell->SetText(_T("集成到右键菜单"));
            chk_shell->SetCheck(true);
            chk_shell->SetFixedHeight(25);
            root->Add(chk_shell);

            // 文件关联
            DuiLib::CCheckBoxUI* chk_assoc = new DuiLib::CCheckBoxUI;
            chk_assoc->SetText(_T("关联压缩文件格式"));
            chk_assoc->SetCheck(true);
            chk_assoc->SetFixedHeight(25);
            root->Add(chk_assoc);

            // 按钮
            DuiLib::CHorizontalLayoutUI* btns = new DuiLib::CHorizontalLayoutUI;
            btns->SetFixedHeight(35);

            DuiLib::CButtonUI* ok = new DuiLib::CButtonUI;
            ok->SetName(_T("btn_ok"));
            ok->SetText(_T("确定"));
            ok->SetFixedWidth(80);
            btns->Add(ok);

            DuiLib::CButtonUI* cancel = new DuiLib::CButtonUI;
            cancel->SetName(_T("btn_cancel"));
            cancel->SetText(_T("取消"));
            cancel->SetFixedWidth(80);
            btns->Add(cancel);

            root->Add(btns);

            paint_manager_.AttachDialog(root);
            paint_manager_.AddNotifier(this);

            SetWindowText(m_hWnd, _T("选项"));
            return 0;
        }
        LRESULT lRes = 0;
        if (paint_manager_.MessageHandler(uMsg, wParam, lParam, lRes)) {
            return lRes;
        }
        return __super::HandleMessage(uMsg, wParam, lParam);
    }

    void OnFinalMessage(HWND) override { delete this; }

    bool GetResult() const { return result_; }

private:
    DuiLib::CPaintManagerUI paint_manager_;
    bool result_ = false;
};

bool ShowOptionsDialog(HWND parent) {
    OptionsDialog dlg;
    dlg.Create(parent, _T("Options"), WS_POPUP | WS_CAPTION | WS_SYSMENU,
                WS_EX_TOOLWINDOW);
    dlg.CenterWindow();
    dlg.ShowModal();
    return dlg.GetResult();
}

// ---------------------------------------------------------------------------
// 关于对话框
// ---------------------------------------------------------------------------
class AboutDialog : public DuiLib::CWindowWnd,
                     public DuiLib::INotifyUI {
public:
    LPCTSTR GetWindowClassName() const override {
        return _T("BandzipCloneAboutDlg");
    }

    void Notify(DuiLib::TNotifyUI& msg) override {
        if (msg.sType == _T("click")) {
            DuiLib::CDuiString name = msg.pSender->GetName();
            if (name == _T("btn_ok")) {
                Close();
            } else if (name == _T("btn_homepage")) {
                util::shell_open(_T("https://github.com/bandzipclone/bandzipclone"));
            }
        }
    }

    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override {
        if (uMsg == WM_CREATE) {
            paint_manager_.Init(m_hWnd);

            DuiLib::CVerticalLayoutUI* root = new DuiLib::CVerticalLayoutUI;
            root->SetInset(DuiLib::CDuiRect(30, 30, 30, 30));
            root->SetFixedWidth(400);
            root->SetFixedHeight(300);

            DuiLib::CLabelUI* lbl_name = new DuiLib::CLabelUI;
            lbl_name->SetText(_T("BandzipClone"));
            lbl_name->SetFixedHeight(40);
            root->Add(lbl_name);

            DuiLib::CLabelUI* lbl_ver = new DuiLib::CLabelUI;
            lbl_ver->SetText(_T("版本 0.9.0"));
            lbl_ver->SetFixedHeight(25);
            root->Add(lbl_ver);

            DuiLib::CLabelUI* lbl_desc = new DuiLib::CLabelUI;
            lbl_desc->SetText(_T("类 Bandizip 的开源解压缩软件"));
            lbl_desc->SetFixedHeight(25);
            root->Add(lbl_desc);

            DuiLib::CLabelUI* lbl_license = new DuiLib::CLabelUI;
            lbl_license->SetText(_T("许可证：AGPLv3"));
            lbl_license->SetFixedHeight(25);
            root->Add(lbl_license);

            DuiLib::CButtonUI* btn_home = new DuiLib::CButtonUI;
            btn_home->SetName(_T("btn_homepage"));
            btn_home->SetText(_T("访问主页"));
            btn_home->SetFixedWidth(120);
            btn_home->SetFixedHeight(30);
            root->Add(btn_home);

            DuiLib::CButtonUI* ok = new DuiLib::CButtonUI;
            ok->SetName(_T("btn_ok"));
            ok->SetText(_T("确定"));
            ok->SetFixedWidth(120);
            ok->SetFixedHeight(30);
            root->Add(ok);

            paint_manager_.AttachDialog(root);
            paint_manager_.AddNotifier(this);

            SetWindowText(m_hWnd, _T("关于"));
            return 0;
        }
        LRESULT lRes = 0;
        if (paint_manager_.MessageHandler(uMsg, wParam, lParam, lRes)) {
            return lRes;
        }
        return __super::HandleMessage(uMsg, wParam, lParam);
    }

    void OnFinalMessage(HWND) override { delete this; }
};

void ShowAboutDialog(HWND parent) {
    AboutDialog dlg;
    dlg.Create(parent, _T("About"), WS_POPUP | WS_CAPTION | WS_SYSMENU,
                WS_EX_TOOLWINDOW);
    dlg.CenterWindow();
    dlg.ShowModal();
}

// ---------------------------------------------------------------------------
// 更新检查对话框
// ---------------------------------------------------------------------------
void ShowUpdateDialog(HWND parent) {
    ShowInfo(parent, _T("检查更新"),
        _T("当前版本：0.9.0\n")
        _T("请访问 https://github.com/bandzipclone/bandzipclone/releases 检查最新版本。"));
}

// ---------------------------------------------------------------------------
// 文件属性
// ---------------------------------------------------------------------------
void ShowEntryProperties(HWND parent, const ArchiveEntry& entry) {
    tstring info;
    info += _T("名称：") + entry.name + _T("\n");
    info += _T("大小：") + util::format_size(entry.size) + _T("\n");
    info += _T("压缩后：") + util::format_size(entry.compressed_size) + _T("\n");
    info += _T("修改时间：") + util::format_time(entry.modified) + _T("\n");
    info += _T("创建时间：") + util::format_time(entry.created) + _T("\n");
    info += _T("CRC32：") + util::format(_T("%08X"), entry.crc32) + _T("\n");
    info += _T("类型：") + (entry.is_directory ? _T("文件夹") : _T("文件")) + _T("\n");
    info += _T("加密：") + (entry.is_encrypted ? _T("是") : _T("否")) + _T("\n");

    ShowInfo(parent, _T("属性"), info);
}

// ---------------------------------------------------------------------------
// 归档属性
// ---------------------------------------------------------------------------
void ShowArchiveProperties(HWND parent, std::unique_ptr<IArchive>& archive) {
    tstring info;
    info += _T("格式：") + util::format(_T("%d"), static_cast<int>(archive->format())) + _T("\n");
    info += _T("可读：") + (archive->can_read() ? _T("是") : _T("否")) + _T("\n");
    info += _T("可写：") + (archive->can_write() ? _T("是") : _T("否")) + _T("\n");
    info += _T("加密：") + (archive->can_encrypt() ? _T("是") : _T("否")) + _T("\n");
    info += _T("固实：") + (archive->can_solid() ? _T("是") : _T("否")) + _T("\n");
    info += _T("分卷：") + (archive->can_volume() ? _T("是") : _T("否")) + _T("\n");

    ShowInfo(parent, _T("归档属性"), info);
}

} // namespace dialogs

// ---------------------------------------------------------------------------
// ProgressDialog 实现
// ---------------------------------------------------------------------------
ProgressDialog::ProgressDialog() {
    start_time_ = GetTickCount64();
    last_time_ = start_time_;
}

ProgressDialog::~ProgressDialog() = default;

LPCTSTR ProgressDialog::GetWindowClassName() const {
    return _T("BandzipCloneProgressDlg");
}

void ProgressDialog::Notify(DuiLib::TNotifyUI& msg) {
    if (msg.sType == _T("click")) {
        DuiLib::CDuiString name = msg.pSender->GetName();
        if (name == _T("btn_cancel")) {
            cancelled_ = true;
            if (cancel_cb_) cancel_cb_();
            btn_cancel_->SetEnabled(false);
            btn_cancel_->SetText(_T("正在取消..."));
        }
    }
}

LRESULT ProgressDialog::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_CREATE) {
        paint_manager_.Init(m_hWnd);

        DuiLib::CVerticalLayoutUI* root = new DuiLib::CVerticalLayoutUI;
        root->SetInset(DuiLib::CDuiRect(20, 20, 20, 20));
        root->SetFixedWidth(500);
        root->SetFixedHeight(180);

        lbl_file_ = new DuiLib::CLabelUI;
        lbl_file_->SetText(_T("正在处理..."));
        lbl_file_->SetFixedHeight(25);
        root->Add(lbl_file_);

        progress_ = new DuiLib::CProgressUI;
        progress_->SetName(_T("progress"));
        progress_->SetFixedHeight(25);
        progress_->SetMinValue(0);
        progress_->SetMaxValue(100);
        root->Add(progress_);

        DuiLib::CHorizontalLayoutUI* info_row = new DuiLib::CHorizontalLayoutUI;
        info_row->SetFixedHeight(25);

        lbl_percent_ = new DuiLib::CLabelUI;
        lbl_percent_->SetText(_T("0%"));
        lbl_percent_->SetFixedWidth(80);
        info_row->Add(lbl_percent_);

        lbl_speed_ = new DuiLib::CLabelUI;
        lbl_speed_->SetText(_T("速度: -"));
        lbl_speed_->SetFixedWidth(150);
        info_row->Add(lbl_speed_);

        lbl_time_left_ = new DuiLib::CLabelUI;
        lbl_time_left_->SetText(_T("剩余: -"));
        info_row->Add(lbl_time_left_);

        root->Add(info_row);

        btn_cancel_ = new DuiLib::CButtonUI;
        btn_cancel_->SetName(_T("btn_cancel"));
        btn_cancel_->SetText(_T("取消"));
        btn_cancel_->SetFixedWidth(100);
        btn_cancel_->SetFixedHeight(30);
        root->Add(btn_cancel_);

        paint_manager_.AttachDialog(root);
        paint_manager_.AddNotifier(this);

        SetWindowText(m_hWnd, _T("进度"));
        return 0;
    }
    LRESULT lRes = 0;
    if (paint_manager_.MessageHandler(uMsg, wParam, lParam, lRes)) {
        return lRes;
    }
    return __super::HandleMessage(uMsg, wParam, lParam);
}

void ProgressDialog::OnFinalMessage(HWND) {
    delete this;
}

bool ProgressDialog::Create(HWND parent, const tstring& title) {
    __super::Create(parent, title.c_str(),
                     WS_POPUP | WS_CAPTION,
                     WS_EX_TOOLWINDOW);
    CenterWindow();
    ShowWindow(*this, SW_SHOW);
    return true;
}

void ProgressDialog::Update(const ProgressInfo& info) {
    if (lbl_file_) {
        lbl_file_->SetText(info.current_file.c_str());
    }
    if (progress_) {
        progress_->SetValue(info.percent);
    }
    if (lbl_percent_) {
        tstring p = util::format(_T("%d%%"), info.percent);
        lbl_percent_->SetText(p.c_str());
    }

    // 计算速度
    u64 now = GetTickCount64();
    u64 elapsed = now - last_time_;
    if (elapsed >= 500) {  // 每 500ms 更新一次
        u64 bytes_diff = info.bytes_processed - last_bytes_;
        double current_speed = static_cast<double>(bytes_diff) /
                                (elapsed / 1000.0);
        // 平滑
        speed_ = speed_ * 0.7 + current_speed * 0.3;

        if (lbl_speed_) {
            tstring s = _T("速度: ") + util::format_size(static_cast<u64>(speed_)) + _T("/s");
            lbl_speed_->SetText(s.c_str());
        }

        if (lbl_time_left_ && info.bytes_total > info.bytes_processed) {
            u64 remaining = info.bytes_total - info.bytes_processed;
            double seconds = remaining / (speed_ > 0 ? speed_ : 1);
            int mins = static_cast<int>(seconds / 60);
            int secs = static_cast<int>(seconds) % 60;
            tstring t = util::format(_T("剩余: %d:%02d"), mins, secs);
            lbl_time_left_->SetText(t.c_str());
        }

        last_bytes_ = info.bytes_processed;
        last_time_ = now;
    }
}

void ProgressDialog::Close() {
    CloseWindow(*this);
}

} // namespace ui
} // namespace bandzip
