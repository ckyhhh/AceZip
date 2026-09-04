// ============================================================================
// main_window.cpp - 主窗口实现
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#include "main_window.h"
#include "dialogs.h"
#include "resource.h"
#include "../utils/string_utils.h"
#include "../utils/file_utils.h"
#include "../utils/path_utils.h"
#include "../utils/logger.h"

#include <shellapi.h>
#include <shobjidl.h>
#include <objbase.h>
#include <algorithm>
#include <future>
#include <thread>

namespace bandzip {
namespace ui {

// ---------------------------------------------------------------------------
// 构造/析构
// ---------------------------------------------------------------------------
MainWindow::MainWindow() {
    // 注册拖拽
    OleInitialize(nullptr);
}

MainWindow::~MainWindow() {
    OleUninitialize();
}

// ---------------------------------------------------------------------------
// CWindowWnd 接口
// ---------------------------------------------------------------------------
LPCTSTR MainWindow::GetWindowClassName() const {
    return _T("BandzipCloneMainWindow");
}

UINT MainWindow::GetClassStyle() const {
    return CS_DBLCLKS;
}

void MainWindow::OnFinalMessage(HWND /*hWnd*/) {
    delete this;
}

// ---------------------------------------------------------------------------
// 创建窗口
// ---------------------------------------------------------------------------
bool MainWindow::Create() {
    tstring skin_dir = util::get_module_dir() + _T("\\skins\\default");
    DuiLib::CPaintManagerUI::SetInstance(GetModuleHandle(nullptr));
    DuiLib::CPaintManagerUI::SetResourcePath(skin_dir.c_str());

    DWORD style = UI_WNDSTYLE_FRAME;
    DWORD ex_style = WS_EX_WINDOWEDGE | WS_EX_ACCEPTFILES;

    Create(nullptr, _T("BandzipClone"), style, ex_style, 0, 0, 1024, 700);
    CenterWindow();
    ShowWindow(*this, SW_SHOW);
    UpdateWindow(*this);

    return true;
}

void MainWindow::Show() {
    ShowWindow(*this, SW_SHOW);
    SetForegroundWindow(*this);
}

void MainWindow::Close() {
    CloseWindow(*this);
}

// ---------------------------------------------------------------------------
// 初始化 UI
// ---------------------------------------------------------------------------
bool MainWindow::InitUI() {
    tstring xml_path = _T("main_window.xml");
    if (!paint_manager_.Load(xml_path.c_str())) {
        LOG_ERROR(_T("Failed to load main_window.xml"));
        return false;
    }

    root_ = static_cast<DuiLib::CVerticalLayoutUI*>(
        paint_manager_.FindControl(_T("root")));
    if (!root_) {
        LOG_ERROR(_T("root not found"));
        return false;
    }

    // 工具栏
    toolbar_ = static_cast<DuiLib::CHorizontalLayoutUI*>(
        paint_manager_.FindControl(_T("toolbar")));

    btn_new_ = static_cast<DuiLib::CButtonUI*>(
        paint_manager_.FindControl(_T("btn_new")));
    btn_open_ = static_cast<DuiLib::CButtonUI*>(
        paint_manager_.FindControl(_T("btn_open")));
    btn_extract_ = static_cast<DuiLib::CButtonUI*>(
        paint_manager_.FindControl(_T("btn_extract")));
    btn_add_ = static_cast<DuiLib::CButtonUI*>(
        paint_manager_.FindControl(_T("btn_add")));
    btn_test_ = static_cast<DuiLib::CButtonUI*>(
        paint_manager_.FindControl(_T("btn_test")));
    btn_delete_ = static_cast<DuiLib::CButtonUI*>(
        paint_manager_.FindControl(_T("btn_delete")));
    btn_find_ = static_cast<DuiLib::CButtonUI*>(
        paint_manager_.FindControl(_T("btn_find")));
    btn_back_ = static_cast<DuiLib::CButtonUI*>(
        paint_manager_.FindControl(_T("btn_back")));
    btn_up_ = static_cast<DuiLib::CButtonUI*>(
        paint_manager_.FindControl(_T("btn_up")));

    // 地址栏
    address_bar_ = static_cast<DuiLib::CHorizontalLayoutUI*>(
        paint_manager_.FindControl(_T("address_bar")));
    edit_address_ = static_cast<DuiLib::CEditUI*>(
        paint_manager_.FindControl(_T("edit_address")));

    // 列表
    list_ = static_cast<DuiLib::CListUI*>(
        paint_manager_.FindControl(_T("list")));
    tree_ = static_cast<DuiLib::CTreeViewUI*>(
        paint_manager_.FindControl(_T("tree")));

    // 状态栏
    status_bar_ = static_cast<DuiLib::CStatusBarUI*>(
        paint_manager_.FindControl(_T("status_bar")));
    lbl_status_count_ = static_cast<DuiLib::CLabelUI*>(
        paint_manager_.FindControl(_T("lbl_status_count")));
    lbl_status_size_ = static_cast<DuiLib::CLabelUI*>(
        paint_manager_.FindControl(_T("lbl_status_size")));
    lbl_status_ratio_ = static_cast<DuiLib::CLabelUI*>(
        paint_manager_.FindControl(_T("lbl_status_ratio")));

    if (list_) {
        list_->SetListCallback(this);
    }

    return true;
}

bool MainWindow::InitMenu() {
    // 菜单通过 XML 加载
    return true;
}

bool MainWindow::InitToolbar() {
    return true;
}

bool MainWindow::InitList() {
    if (!list_) return false;

    // 设置列表头
    DuiLib::CListHeaderUI* header = list_->GetHeader();
    if (header) {
        header->SetVisible(true);
    }

    return true;
}

bool MainWindow::InitStatusBar() {
    return true;
}

// ---------------------------------------------------------------------------
// 消息处理
// ---------------------------------------------------------------------------
LRESULT MainWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        if (!InitUI()) return -1;
        InitMenu();
        InitToolbar();
        InitList();
        InitStatusBar();
        break;

    case WM_DESTROY:
        ::PostQuitMessage(0);
        break;

    case WM_NCACTIVATE:
        if (!::IsIconic(*this)) return (wParam == 0) ? TRUE : FALSE;
        break;

    case WM_DROPFILES:
        OnDropFiles(reinterpret_cast<HDROP>(wParam));
        break;

    case WM_KEYDOWN:
        OnKeyDown(wParam, lParam);
        break;

    case WM_CONTEXTMENU: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (pt.x == -1 && pt.y == -1) {
            // 键盘触发
            GetCursorPos(&pt);
        }
        ShowContextMenu(pt);
        break;
    }

    case WM_COPYDATA:
        // 接收来自其他实例的消息
        break;
    }

    LRESULT lRes = 0;
    if (paint_manager_.MessageHandler(uMsg, wParam, lParam, lRes)) {
        return lRes;
    }

    return __super::HandleMessage(uMsg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// 事件通知
// ---------------------------------------------------------------------------
void MainWindow::Notify(DuiLib::TNotifyUI& msg) {
    if (msg.sType == _T("click")) {
        OnClick(msg);
    } else if (msg.sType == _T("menu")) {
        OnMenu(msg);
    } else if (msg.sType == _T("itemselect")) {
        OnItemSelected(msg);
    } else if (msg.sType == _T("itemdblclick")) {
        OnItemDblClick(msg);
    } else if (msg.sType == _T("timer")) {
        OnTimer(msg);
    } else if (msg.sType == _T("return")) {
        if (msg.pSender == edit_address_) {
            // 地址栏回车
            tstring path = edit_address_->GetText();
            if (!path.empty()) {
                OpenArchive(path);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// 点击事件
// ---------------------------------------------------------------------------
void MainWindow::OnClick(DuiLib::TNotifyUI& msg) {
    DuiLib::CDuiString name = msg.pSender->GetName();

    if (name == _T("btn_new"))      OnToolbarNew();
    else if (name == _T("btn_open"))    OnToolbarOpen();
    else if (name == _T("btn_extract")) OnToolbarExtract();
    else if (name == _T("btn_add"))     OnToolbarAdd();
    else if (name == _T("btn_test"))    OnToolbarTest();
    else if (name == _T("btn_delete"))  OnToolbarDelete();
    else if (name == _T("btn_find"))    OnToolbarFind();
    else if (name == _T("btn_back"))    OnToolbarBack();
    else if (name == _T("btn_up"))      OnToolbarUp();
}

// ---------------------------------------------------------------------------
// 菜单事件
// ---------------------------------------------------------------------------
void MainWindow::OnMenu(DuiLib::TNotifyUI& msg) {
    DuiLib::CDuiString name = msg.pSender->GetName();

    if (name == _T("menu_file_new"))           OnMenuFileNew();
    else if (name == _T("menu_file_open"))       OnMenuFileOpen();
    else if (name == _T("menu_file_open_as"))    OnMenuFileOpenAs();
    else if (name == _T("menu_file_save"))       OnMenuFileSave();
    else if (name == _T("menu_file_save_as"))    OnMenuFileSaveAs();
    else if (name == _T("menu_file_properties")) OnMenuFileProperties();
    else if (name == _T("menu_file_exit"))       OnMenuFileExit();
    else if (name == _T("menu_edit_select_all")) OnMenuEditSelectAll();
    else if (name == _T("menu_edit_invert"))     OnMenuEditInvertSelection();
    else if (name == _T("menu_edit_find"))       OnMenuEditFind();
    else if (name == _T("menu_edit_find_next"))  OnMenuEditFindNext();
    else if (name == _T("menu_edit_delete"))      OnMenuEditDelete();
    else if (name == _T("menu_view_details"))    OnMenuViewDetails();
    else if (name == _T("menu_view_list"))       OnMenuViewList();
    else if (name == _T("menu_view_large"))       OnMenuViewLargeIcons();
    else if (name == _T("menu_view_small"))       OnMenuViewSmallIcons();
    else if (name == _T("menu_view_tree"))        OnMenuViewTree();
    else if (name == _T("menu_view_refresh"))     OnMenuViewRefresh();
    else if (name == _T("menu_tools_extract"))    OnMenuToolsExtract();
    else if (name == _T("menu_tools_extract_here")) OnMenuToolsExtractHere();
    else if (name == _T("menu_tools_extract_to")) OnMenuToolsExtractTo();
    else if (name == _T("menu_tools_test"))      OnMenuToolsTest();
    else if (name == _T("menu_tools_add"))        OnMenuToolsAdd();
    else if (name == _T("menu_tools_convert"))    OnMenuToolsConvert();
    else if (name == _T("menu_tools_sfx"))        OnMenuToolsSFX();
    else if (name == _T("menu_tools_options"))    OnMenuToolsOptions();
    else if (name == _T("menu_help_about"))       OnMenuHelpAbout();
    else if (name == _T("menu_help_update"))      OnMenuHelpCheckUpdate();
    else if (name == _T("menu_help_homepage"))    OnMenuHelpHomepage();
    else if (name == _T("menu_help_manual"))      OnMenuHelpManual();
}

// ---------------------------------------------------------------------------
// 列表项选择
// ---------------------------------------------------------------------------
void MainWindow::OnItemSelected(DuiLib::TNotifyUI& /*msg*/) {
    UpdateStatusBar();
}

// ---------------------------------------------------------------------------
// 列表项双击
// ---------------------------------------------------------------------------
void MainWindow::OnItemDblClick(DuiLib::TNotifyUI& msg) {
    if (msg.pSender == list_) {
        int index = list_->GetCurSel();
        if (index >= 0 && index < static_cast<int>(current_entries_.size())) {
            PreviewEntry(static_cast<u32>(index));
        }
    }
}

// ---------------------------------------------------------------------------
// 定时器
// ---------------------------------------------------------------------------
void MainWindow::OnTimer(DuiLib::TNotifyUI& /*msg*/) {
    // 用于更新进度等
}

// ---------------------------------------------------------------------------
// 菜单事件实现
// ---------------------------------------------------------------------------
void MainWindow::OnMenuFileNew() {
    OnToolbarNew();
}

void MainWindow::OnMenuFileOpen() {
    OnToolbarOpen();
}

void MainWindow::OnMenuFileOpenAs() {
    // 选择文件对话框
    tstring path = dialogs::OpenFileDialog(*this,
        _T("所有支持的格式 (*.zip;*.7z;*.rar;*.tar;*.gz;*.bz2;*.xz;*.zst;*.lz4;*.cab)|*.zip;*.7z;*.rar;*.tar;*.gz;*.bz2;*.xz;*.zst;*.lz4;*.cab|")
        _T("ZIP 文件 (*.zip)|*.zip|")
        _T("7Z 文件 (*.7z)|*.7z|")
        _T("RAR 文件 (*.rar)|*.rar|")
        _T("TAR 文件 (*.tar)|*.tar|")
        _T("所有文件 (*.*)|*.*|"));

    if (!path.empty()) {
        OpenArchive(path);
    }
}

void MainWindow::OnMenuFileSave() {
    if (!current_archive_) return;
    // 保存当前归档
}

void MainWindow::OnMenuFileSaveAs() {
    if (!current_archive_) return;
    // 另存为
}

void MainWindow::OnMenuFileProperties() {
    if (!current_archive_) return;
    dialogs::ShowArchiveProperties(*this, current_archive_);
}

void MainWindow::OnMenuFileExit() {
    Close();
}

void MainWindow::OnMenuEditSelectAll() {
    if (list_) {
        list_->SelectAllItems();
    }
}

void MainWindow::OnMenuEditInvertSelection() {
    if (list_) {
        int count = list_->GetCount();
        for (int i = 0; i < count; ++i) {
            if (list_->IsItemSelected(i)) {
                list_->UnSelectItem(i);
            } else {
                list_->SelectItem(i);
            }
        }
    }
}

void MainWindow::OnMenuEditFind() {
    tstring text = dialogs::InputBox(*this, _T("查找"), _T("输入要查找的文件名："));
    if (!text.empty()) {
        find_text_ = text;
        find_pos_ = 0;
        FindNext();
    }
}

void MainWindow::OnMenuEditFindNext() {
    FindNext();
}

void MainWindow::OnMenuEditDelete() {
    DeleteSelected();
}

void MainWindow::OnMenuViewDetails() {
    view_mode_ = ViewMode::Details;
    if (list_) list_->SetVisible(true);
    if (tree_) tree_->SetVisible(false);
}

void MainWindow::OnMenuViewList() {
    view_mode_ = ViewMode::List;
    if (list_) list_->SetVisible(true);
    if (tree_) tree_->SetVisible(false);
}

void MainWindow::OnMenuViewLargeIcons() {
    view_mode_ = ViewMode::LargeIcons;
}

void MainWindow::OnMenuViewSmallIcons() {
    view_mode_ = ViewMode::SmallIcons;
}

void MainWindow::OnMenuViewTree() {
    view_mode_ = ViewMode::Tree;
    if (list_) list_->SetVisible(false);
    if (tree_) tree_->SetVisible(true);
}

void MainWindow::OnMenuViewRefresh() {
    RefreshList();
}

void MainWindow::OnMenuToolsExtract() {
    OnToolbarExtract();
}

void MainWindow::OnMenuToolsExtractHere() {
    if (!current_archive_) return;
    tstring dir = util::get_dirname(current_archive_path_);
    ExtractSelectedTo(dir);
}

void MainWindow::OnMenuToolsExtractTo() {
    if (!current_archive_) return;
    tstring dir = dialogs::BrowseFolderDialog(*this, _T("选择解压目标目录"));
    if (!dir.empty()) {
        ExtractSelectedTo(dir);
    }
}

void MainWindow::OnMenuToolsTest() {
    OnToolbarTest();
}

void MainWindow::OnMenuToolsAdd() {
    OnToolbarAdd();
}

void MainWindow::OnMenuToolsConvert() {
    // 格式转换
}

void MainWindow::OnMenuToolsSFX() {
    // 创建自解压
}

void MainWindow::OnMenuToolsOptions() {
    dialogs::ShowOptionsDialog(*this);
}

void MainWindow::OnMenuHelpAbout() {
    dialogs::ShowAboutDialog(*this);
}

void MainWindow::OnMenuHelpCheckUpdate() {
    dialogs::ShowUpdateDialog(*this);
}

void MainWindow::OnMenuHelpHomepage() {
    util::shell_open(_T("https://github.com/bandzipclone/bandzipclone"));
}

void MainWindow::OnMenuHelpManual() {
    util::shell_open(_T("https://github.com/bandzipclone/bandzipclone/wiki"));
}

// ---------------------------------------------------------------------------
// 工具栏事件
// ---------------------------------------------------------------------------
void MainWindow::OnToolbarNew() {
    CreateOptions opts;
    opts.archive_path = _T("new.zip");
    opts.format = ArchiveFormat::Zip;
    if (ShowCompressDialog({}, opts)) {
        CreateArchive(opts);
    }
}

void MainWindow::OnToolbarOpen() {
    tstring path = dialogs::OpenFileDialog(*this,
        _T("所有支持的格式 (*.zip;*.7z;*.rar;*.tar;*.gz;*.bz2;*.xz;*.zst;*.lz4;*.cab)|*.zip;*.7z;*.rar;*.tar;*.gz;*.bz2;*.xz;*.zst;*.lz4;*.cab|")
        _T("所有文件 (*.*)|*.*|"));

    if (!path.empty()) {
        OpenArchive(path);
    }
}

void MainWindow::OnToolbarExtract() {
    if (!current_archive_) return;

    tstring output_dir;
    ExtractOptions opts;
    if (ShowExtractDialog(current_archive_path_, output_dir, opts)) {
        ExtractSelectedTo(output_dir);
    }
}

void MainWindow::OnToolbarAdd() {
    std::vector<tstring> files = dialogs::OpenMultiFileDialog(*this,
        _T("所有文件 (*.*)|*.*|"));
    if (files.empty()) return;

    CreateOptions opts;
    opts.archive_path = current_archive_path_;
    opts.format = current_archive_ ? current_archive_->format() : ArchiveFormat::Zip;
    if (ShowCompressDialog(files, opts)) {
        // 添加文件到当前归档
    }
}

void MainWindow::OnToolbarTest() {
    TestSelected();
}

void MainWindow::OnToolbarDelete() {
    DeleteSelected();
}

void MainWindow::OnToolbarFind() {
    OnMenuEditFind();
}

void MainWindow::OnToolbarBack() {
    // 后退
}

void MainWindow::OnToolbarUp() {
    // 上级目录
}

// ---------------------------------------------------------------------------
// 打开归档
// ---------------------------------------------------------------------------
bool MainWindow::OpenArchive(const tstring& path, const tstring& password) {
    std::error_code ec;
    auto archive = ArchiveManager::instance().open(path, password, &ec);
    if (ec) {
        if (ec == make_error_code(ArchiveError::PasswordRequired) ||
            ec == make_error_code(ArchiveError::WrongPassword)) {
            tstring pwd;
            if (ShowPasswordDialog(pwd)) {
                return OpenArchive(path, pwd);
            }
        }
        dialogs::ShowError(*this, _T("打开失败"), ec.message().c_str());
        return false;
    }

    current_archive_ = std::move(archive);
    current_archive_path_ = path;
    current_password_ = password;

    // 读取条目
    ec = current_archive_->read_entries(current_entries_);
    if (ec) {
        dialogs::ShowError(*this, _T("读取失败"), ec.message().c_str());
        return false;
    }

    // 更新 UI
    if (edit_address_) {
        edit_address_->SetText(path.c_str());
    }
    UpdateTitle();
    RefreshList();
    UpdateStatusBar();

    return true;
}

// ---------------------------------------------------------------------------
// 创建归档
// ---------------------------------------------------------------------------
bool MainWindow::CreateArchive(const CreateOptions& opts) {
    auto archive = create_archive(opts.format);
    if (!archive) {
        dialogs::ShowError(*this, _T("创建失败"), _T("不支持的格式"));
        return false;
    }

    std::error_code ec = archive->create(opts);
    if (ec) {
        dialogs::ShowError(*this, _T("创建失败"), ec.message().c_str());
        return false;
    }

    current_archive_ = std::move(archive);
    current_archive_path_ = opts.archive_path;
    current_password_ = opts.password;
    current_entries_.clear();

    if (edit_address_) {
        edit_address_->SetText(opts.archive_path.c_str());
    }
    UpdateTitle();
    RefreshList();
    UpdateStatusBar();

    return true;
}

// ---------------------------------------------------------------------------
// 刷新列表
// ---------------------------------------------------------------------------
void MainWindow::RefreshList() {
    ClearList();
    if (!current_archive_) return;

    for (const auto& entry : current_entries_) {
        AddEntryToList(entry);
    }
}

// ---------------------------------------------------------------------------
// 清空列表
// ---------------------------------------------------------------------------
void MainWindow::ClearList() {
    if (list_) {
        list_->RemoveAll();
    }
    if (tree_) {
        tree_->RemoveAll();
    }
}

// ---------------------------------------------------------------------------
// 添加条目到列表
// ---------------------------------------------------------------------------
void MainWindow::AddEntryToList(const ArchiveEntry& entry) {
    if (!list_) return;

    DuiLib::CListContainerElementUI* elem = new DuiLib::CListContainerElementUI;
    elem->SetFixedHeight(20);

    // 名称
    DuiLib::CHorizontalLayoutUI* row = new DuiLib::CHorizontalLayoutUI;

    DuiLib::CLabelUI* lbl_name = new DuiLib::CLabelUI;
    lbl_name->SetText(entry.name.c_str());
    lbl_name->SetFixedWidth(300);
    row->Add(lbl_name);

    DuiLib::CLabelUI* lbl_size = new DuiLib::CLabelUI;
    lbl_size->SetText(util::format_size(entry.size).c_str());
    lbl_size->SetFixedWidth(100);
    row->Add(lbl_size);

    DuiLib::CLabelUI* lbl_packed = new DuiLib::CLabelUI;
    lbl_packed->SetText(util::format_size(entry.compressed_size).c_str());
    lbl_packed->SetFixedWidth(100);
    row->Add(lbl_packed);

    DuiLib::CLabelUI* lbl_type = new DuiLib::CLabelUI;
    lbl_type->SetText(entry.is_directory ? _T("文件夹") : _T("文件"));
    lbl_type->SetFixedWidth(80);
    row->Add(lbl_type);

    DuiLib::CLabelUI* lbl_modified = new DuiLib::CLabelUI;
    lbl_modified->SetText(util::format_time(entry.modified).c_str());
    lbl_modified->SetFixedWidth(150);
    row->Add(lbl_modified);

    elem->Add(row);
    list_->Add(elem);
}

// ---------------------------------------------------------------------------
// 更新状态栏
// ---------------------------------------------------------------------------
void MainWindow::UpdateStatusBar() {
    if (!lbl_status_count_) return;

    u64 total_size = 0;
    u64 total_packed = 0;
    int selected = 0;

    if (list_) {
        int count = list_->GetCount();
        for (int i = 0; i < count; ++i) {
            if (list_->IsItemSelected(i)) {
                ++selected;
                if (i < static_cast<int>(current_entries_.size())) {
                    total_size += current_entries_[i].size;
                    total_packed += current_entries_[i].compressed_size;
                }
            }
        }
    }

    tstring count_text = util::format(_T("选中 %d 项 / 共 %d 项"),
                                       selected, static_cast<int>(current_entries_.size()));
    lbl_status_count_->SetText(count_text.c_str());

    if (lbl_status_size_) {
        tstring size_text = util::format(_T("大小: %s"),
                                          util::format_size(total_size).c_str());
        lbl_status_size_->SetText(size_text.c_str());
    }

    if (lbl_status_ratio_) {
        if (total_size > 0) {
            int ratio = static_cast<int>(100 - total_packed * 100 / total_size);
            tstring ratio_text = util::format(_T("压缩率: %d%%"), ratio);
            lbl_status_ratio_->SetText(ratio_text.c_str());
        } else {
            lbl_status_ratio_->SetText(_T("压缩率: -"));
        }
    }
}

// ---------------------------------------------------------------------------
// 更新标题
// ---------------------------------------------------------------------------
void MainWindow::UpdateTitle() {
    tstring title;
    if (current_archive_path_.empty()) {
        title = _T("BandzipClone");
    } else {
        title = util::get_filename(current_archive_path_) + _T(" - BandzipClone");
    }
    SetWindowText(*this, title.c_str());
}

// ---------------------------------------------------------------------------
// 获取选中的条目索引
// ---------------------------------------------------------------------------
std::vector<u32> MainWindow::GetSelectedIndices() const {
    std::vector<u32> indices;
    if (!list_) return indices;

    int count = list_->GetCount();
    for (int i = 0; i < count; ++i) {
        if (list_->IsItemSelected(i)) {
            indices.push_back(static_cast<u32>(i));
        }
    }
    return indices;
}

// ---------------------------------------------------------------------------
// 拖拽
// ---------------------------------------------------------------------------
void MainWindow::OnDropFiles(HDROP hDrop) {
    UINT count = DragQueryFile(hDrop, 0xFFFFFFFF, nullptr, 0);
    for (UINT i = 0; i < count; ++i) {
        tchar path[MAX_PATH];
        DragQueryFile(hDrop, i, path, MAX_PATH);
        tstring p(path);

        // 如果是归档文件，打开它
        ArchiveFormat fmt = util::detect_format(p);
        if (fmt != ArchiveFormat::Unknown) {
            OpenArchive(p);
            break;
        }

        // 否则添加到当前归档
        if (current_archive_) {
            CompressOptions opts;
            opts.level = ArchiveManager::instance().config().default_level;
            current_archive_->add_files({p}, opts);
        }
    }
    DragFinish(hDrop);
    RefreshList();
}

void MainWindow::OnDragEnter() {
    is_dragging_ = true;
}

void MainWindow::OnDragLeave() {
    is_dragging_ = false;
}

void MainWindow::OnDragOver(POINT /*pt*/) {
    // 高亮目标
}

// ---------------------------------------------------------------------------
// 剪贴板
// ---------------------------------------------------------------------------
void MainWindow::OnCopy() {
    auto indices = GetSelectedIndices();
    if (indices.empty()) return;

    // 复制文件名到剪贴板
    tstring text;
    for (u32 idx : indices) {
        if (idx < current_entries_.size()) {
            if (!text.empty()) text += _T("\r\n");
            text += current_entries_[idx].name;
        }
    }

    if (OpenClipboard(*this)) {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(tchar));
        if (hMem) {
            tchar* p = static_cast<tchar*>(GlobalLock(hMem));
            _tcscpy_s(p, text.size() + 1, text.c_str());
            GlobalUnlock(hMem);
#ifdef _UNICODE
            SetClipboardData(CF_UNICODETEXT, hMem);
#else
            SetClipboardData(CF_TEXT, hMem);
#endif
        }
        CloseClipboard();
    }
}

void MainWindow::OnPaste() {
    // 从剪贴板粘贴文件
    if (OpenClipboard(*this)) {
#ifdef _UNICODE
        HANDLE h = GetClipboardData(CF_UNICODETEXT);
#else
        HANDLE h = GetClipboardData(CF_TEXT);
#endif
        if (h) {
            tchar* p = static_cast<tchar*>(GlobalLock(h));
            if (p) {
                tstring text(p);
                GlobalUnlock(h);
                // 处理粘贴的文本
            }
        }
        CloseClipboard();
    }
}

// ---------------------------------------------------------------------------
// 键盘
// ---------------------------------------------------------------------------
void MainWindow::OnKeyDown(WPARAM vk, LPARAM /*lParam*/) {
    switch (vk) {
    case VK_F5:
        RefreshList();
        break;

    case VK_DELETE:
        DeleteSelected();
        break;

    case 'A':
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            OnMenuEditSelectAll();
        }
        break;

    case 'C':
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            OnCopy();
        }
        break;

    case 'V':
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            OnPaste();
        }
        break;

    case 'F':
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            OnMenuEditFind();
        }
        break;

    case VK_F3:
        FindNext();
        break;

    case VK_RETURN:
        if (list_) {
            int index = list_->GetCurSel();
            if (index >= 0) {
                PreviewEntry(static_cast<u32>(index));
            }
        }
        break;
    }
}

// ---------------------------------------------------------------------------
// 右键菜单
// ---------------------------------------------------------------------------
void MainWindow::ShowContextMenu(POINT pt) {
    if (!current_archive_) return;

    HMENU hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, 1, _T("打开(&O)"));
    AppendMenu(hMenu, MF_STRING, 2, _T("解压到指定目录(&E)..."));
    AppendMenu(hMenu, MF_STRING, 3, _T("解压到当前目录(&H)"));
    AppendMenu(hMenu, MF_STRING, 4, _T("解压到 \"文件夹名\"(&X)"));
    AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hMenu, MF_STRING, 5, _T("测试(&T)"));
    AppendMenu(hMenu, MF_STRING, 6, _T("删除(&D)"));
    AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hMenu, MF_STRING, 7, _T("复制文件名(&N)"));
    AppendMenu(hMenu, MF_STRING, 8, _T("属性(&P)"));

    int cmd = TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN |
                                     TPM_RETURNCMD | TPM_NONOTIFY,
                              pt.x, pt.y, 0, *this, nullptr);
    DestroyMenu(hMenu);

    switch (cmd) {
    case 1: {
        int index = list_ ? list_->GetCurSel() : -1;
        if (index >= 0) PreviewEntry(static_cast<u32>(index));
        break;
    }
    case 2: OnMenuToolsExtract(); break;
    case 3: OnMenuToolsExtractHere(); break;
    case 4: {
        tstring dir = util::get_dirname(current_archive_path_);
        tstring base = util::get_basename(current_archive_path_);
        tstring ext = util::get_extension_lower(base);
        if (!ext.empty()) {
            base = base.substr(0, base.length() - ext.length());
        }
        dir = util::join_path(dir, base);
        ExtractSelectedTo(dir);
        break;
    }
    case 5: TestSelected(); break;
    case 6: DeleteSelected(); break;
    case 7: OnCopy(); break;
    case 8: {
        int index = list_ ? list_->GetCurSel() : -1;
        if (index >= 0 && index < static_cast<int>(current_entries_.size())) {
            dialogs::ShowEntryProperties(*this, current_entries_[index]);
        }
        break;
    }
    }
}

// ---------------------------------------------------------------------------
// 进度对话框
// ---------------------------------------------------------------------------
void MainWindow::ShowProgressDialog(const tstring& title) {
    // 在 dialogs.cpp 中实现
}

void MainWindow::UpdateProgressDialog(const ProgressInfo& info) {
    if (progress_dlg_) {
        progress_dlg_->Update(info);
    }
}

void MainWindow::CloseProgressDialog() {
    if (progress_dlg_) {
        progress_dlg_->Close();
        progress_dlg_ = nullptr;
    }
}

// ---------------------------------------------------------------------------
// 密码对话框
// ---------------------------------------------------------------------------
bool MainWindow::ShowPasswordDialog(tstring& password) {
    return dialogs::ShowPasswordDialog(*this, password);
}

// ---------------------------------------------------------------------------
// 解压对话框
// ---------------------------------------------------------------------------
bool MainWindow::ShowExtractDialog(const tstring& archive_path,
                                      tstring& output_dir,
                                      ExtractOptions& opts) {
    return dialogs::ShowExtractDialog(*this, archive_path, output_dir, opts);
}

// ---------------------------------------------------------------------------
// 压缩对话框
// ---------------------------------------------------------------------------
bool MainWindow::ShowCompressDialog(const std::vector<tstring>& files,
                                       CreateOptions& opts) {
    return dialogs::ShowCompressDialog(*this, files, opts);
}

// ---------------------------------------------------------------------------
// 解压选中条目
// ---------------------------------------------------------------------------
void MainWindow::ExtractSelectedTo(const tstring& output_dir) {
    if (!current_archive_) return;

    auto indices = GetSelectedIndices();
    if (indices.empty()) {
        // 没有选中，解压全部
        for (u32 i = 0; i < current_entries_.size(); ++i) {
            indices.push_back(i);
        }
    }

    ExtractOptions opts;
    opts.overwrite = OverwriteMode::Ask;
    opts.keep_paths = true;
    opts.skip_existing = false;

    ShowProgressDialog(_T("正在解压..."));

    task_running_ = true;
    task_cancelled_ = false;

    std::thread([this, indices, output_dir, opts]() {
        std::error_code ec = current_archive_->extract_files(
            indices, output_dir, opts);

        // 在主线程更新 UI
        PostMessage(*this, WM_USER + 100, 0,
                     reinterpret_cast<LPARAM>(new std::error_code(ec)));
    }).detach();
}

// ---------------------------------------------------------------------------
// 预览条目
// ---------------------------------------------------------------------------
void MainWindow::PreviewEntry(u32 index) {
    if (!current_archive_ || index >= current_entries_.size()) return;

    const ArchiveEntry& entry = current_entries_[index];
    if (entry.is_directory) return;

    // 解压到临时目录
    tstring temp_dir = util::create_temp_dir(_T("bz_preview"));
    ExtractOptions opts;
    opts.overwrite = OverwriteMode::Overwrite;
    opts.keep_paths = false;

    std::error_code ec = current_archive_->extract_entry(
        index, util::join_path(temp_dir, entry.name), opts);

    if (ec) {
        dialogs::ShowError(*this, _T("预览失败"), ec.message().c_str());
        util::delete_dir(temp_dir, true);
        return;
    }

    // 用系统默认程序打开
    tstring full_path = util::join_path(temp_dir, entry.name);
    util::shell_open(full_path);

    // 注意：临时文件会在程序退出时清理
}

// ---------------------------------------------------------------------------
// 测试选中条目
// ---------------------------------------------------------------------------
void MainWindow::TestSelected() {
    if (!current_archive_) return;

    auto indices = GetSelectedIndices();
    if (indices.empty()) {
        // 测试全部
        ShowProgressDialog(_T("正在测试..."));
        std::thread([this]() {
            std::error_code ec = current_archive_->test();
            PostMessage(*this, WM_USER + 101, 0,
                         reinterpret_cast<LPARAM>(new std::error_code(ec)));
        }).detach();
        return;
    }

    ShowProgressDialog(_T("正在测试..."));
    std::thread([this, indices]() {
        std::error_code ec;
        for (u32 idx : indices) {
            ec = current_archive_->test_entry(idx);
            if (ec) break;
        }
        PostMessage(*this, WM_USER + 101, 0,
                     reinterpret_cast<LPARAM>(new std::error_code(ec)));
    }).detach();
}

// ---------------------------------------------------------------------------
// 删除选中条目
// ---------------------------------------------------------------------------
void MainWindow::DeleteSelected() {
    if (!current_archive_) return;

    auto indices = GetSelectedIndices();
    if (indices.empty()) return;

    if (dialogs::MessageBox(*this, _T("确认删除"),
            _T("确定要删除选中的条目吗？"), MB_YESNO | MB_ICONQUESTION) != IDYES) {
        return;
    }

    std::error_code ec = current_archive_->delete_entries(indices);
    if (ec) {
        dialogs::ShowError(*this, _T("删除失败"), ec.message().c_str());
        return;
    }

    RefreshList();
}

// ---------------------------------------------------------------------------
// 查找
// ---------------------------------------------------------------------------
void MainWindow::FindNext() {
    if (find_text_.empty()) return;
    if (!list_) return;

    int count = list_->GetCount();
    for (int i = static_cast<int>(find_pos_) + 1; i < count; ++i) {
        if (i < static_cast<int>(current_entries_.size())) {
            const auto& entry = current_entries_[i];
            // 大小写不敏感查找
            tstring name = util::to_lower(entry.name);
            tstring text = util::to_lower(find_text_);
            if (name.find(text) != tstring::npos) {
                list_->SelectItem(i);
                list_->EnsureVisible(i);
                find_pos_ = static_cast<size_t>(i);
                return;
            }
        }
    }

    // 没找到
    dialogs::MessageBox(*this, _T("查找"),
        _T("未找到匹配的文件。"), MB_OK | MB_ICONINFORMATION);
}

// ---------------------------------------------------------------------------
// 后台任务
// ---------------------------------------------------------------------------
void MainWindow::RunAsyncTask(std::function<void()> task,
                                  std::function<void(std::error_code)> on_complete) {
    task_running_ = true;
    task_cancelled_ = false;

    std::thread([this, task, on_complete]() {
        std::error_code ec;
        try {
            task();
        } catch (...) {
            ec = make_error_code(ArchiveError::InternalError);
        }

        if (on_complete) {
            on_complete(ec);
        }

        task_running_ = false;
    }).detach();
}

// ---------------------------------------------------------------------------
// IListContainerCallbackUI
// ---------------------------------------------------------------------------
DuiLib::CControlUI* MainWindow::CreateCell(DuiLib::CListUI* /*pList*/,
                                              int /*nItemIndex*/) {
    return nullptr;
}

void MainWindow::OnCellSelected(DuiLib::CListUI* /*pList*/,
                                  int /*nItemIndex*/) {
    UpdateStatusBar();
}

} // namespace ui
} // namespace bandzip
