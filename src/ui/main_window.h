// ============================================================================
// main_window.h - 主窗口
//
// BandzipClone 的主界面，包含：
// - 菜单栏（文件、编辑、视图、工具、帮助）
// - 工具栏（新建、打开、解压、压缩、测试、删除、查找）
// - 地址栏（显示当前打开的归档路径）
// - 文件列表（树形 + 列表双视图）
// - 状态栏（条目数、总大小、压缩率）
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#pragma once

#include "../core/archive.h"
#include "../core/archive_manager.h"

#include <UIlib.h>
#include <memory>
#include <vector>
#include <functional>

namespace bandzip {
namespace ui {

class MainWindow : public DuiLib::CWindowWnd,
                   public DuiLib::INotifyUI,
                   public DuiLib::IListContainerCallbackUI {
public:
    MainWindow();
    ~MainWindow() override;

    // CWindowWnd
    LPCTSTR GetWindowClassName() const override;
    UINT GetClassStyle() const override;
    void Notify(DuiLib::TNotifyUI& msg) override;
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override;
    void OnFinalMessage(HWND hWnd) override;

    // IListContainerCallbackUI
    DuiLib::CControlUI* CreateCell(DuiLib::CListUI* pList,
                                     int nItemIndex) override;
    void OnCellSelected(DuiLib::CListUI* pList,
                          int nItemIndex) override;

    // 公开接口
    bool Create();
    void Show();
    void Close();

    // 打开归档
    bool OpenArchive(const tstring& path, const tstring& password = tstring());

    // 创建新归档
    bool CreateArchive(const CreateOptions& opts);

    // 刷新列表
    void RefreshList();

    // 获取选中的条目索引
    std::vector<u32> GetSelectedIndices() const;

    // 拖拽支持
    void EnableDragDrop(bool enable) { drag_drop_enabled_ = enable; }

private:
    // 初始化 UI
    bool InitUI();
    bool InitMenu();
    bool InitToolbar();
    bool InitList();
    bool InitStatusBar();

    // 事件处理
    void OnClick(DuiLib::TNotifyUI& msg);
    void OnMenu(DuiLib::TNotifyUI& msg);
    void OnItemSelected(DuiLib::TNotifyUI& msg);
    void OnItemDblClick(DuiLib::TNotifyUI& msg);
    void OnTimer(DuiLib::TNotifyUI& msg);

    // 菜单事件
    void OnMenuFileNew();
    void OnMenuFileOpen();
    void OnMenuFileOpenAs();
    void OnMenuFileSave();
    void OnMenuFileSaveAs();
    void OnMenuFileProperties();
    void OnMenuFileExit();

    void OnMenuEditSelectAll();
    void OnMenuEditInvertSelection();
    void OnMenuEditFind();
    void OnMenuEditFindNext();
    void OnMenuEditDelete();

    void OnMenuViewLargeIcons();
    void OnMenuViewSmallIcons();
    void OnMenuViewList();
    void OnMenuViewDetails();
    void OnMenuViewTree();
    void OnMenuViewRefresh();

    void OnMenuToolsExtract();
    void OnMenuToolsExtractHere();
    void OnMenuToolsExtractTo();
    void OnMenuToolsTest();
    void OnMenuToolsAdd();
    void OnMenuToolsConvert();
    void OnMenuToolsSFX();
    void OnMenuToolsOptions();

    void OnMenuHelpAbout();
    void OnMenuHelpCheckUpdate();
    void OnMenuHelpHomepage();
    void OnMenuHelpManual();

    // 工具栏事件
    void OnToolbarNew();
    void OnToolbarOpen();
    void OnToolbarExtract();
    void OnToolbarAdd();
    void OnToolbarTest();
    void OnToolbarDelete();
    void OnToolbarFind();
    void OnToolbarBack();
    void OnToolbarUp();

    // 列表操作
    void AddEntryToList(const ArchiveEntry& entry);
    void ClearList();
    void UpdateStatusBar();
    void UpdateTitle();

    // 拖拽
    void OnDropFiles(HDROP hDrop);
    void OnDragEnter();
    void OnDragLeave();
    void OnDragOver(POINT pt);

    // 剪贴板
    void OnCopy();
    void OnPaste();

    // 键盘
    void OnKeyDown(WPARAM vk, LPARAM lParam);

    // 右键菜单
    void ShowContextMenu(POINT pt);

    // 进度对话框
    void ShowProgressDialog(const tstring& title);
    void UpdateProgressDialog(const ProgressInfo& info);
    void CloseProgressDialog();

    // 密码对话框
    bool ShowPasswordDialog(tstring& password);

    // 解压对话框
    bool ShowExtractDialog(const tstring& archive_path,
                            tstring& output_dir,
                            ExtractOptions& opts);

    // 压缩对话框
    bool ShowCompressDialog(const std::vector<tstring>& files,
                             CreateOptions& opts);

    // 提取选中条目到指定目录
    void ExtractSelectedTo(const tstring& output_dir);

    // 双击条目（预览）
    void PreviewEntry(u32 index);

    // 测试选中条目
    void TestSelected();

    // 删除选中条目
    void DeleteSelected();

    // 查找
    void FindNext();

    // 后台任务
    void RunAsyncTask(std::function<void()> task,
                       std::function<void(std::error_code)> on_complete = nullptr);

private:
    DuiLib::CPaintManagerUI paint_manager_;

    // UI 控件指针
    DuiLib::CVerticalLayoutUI* root_ = nullptr;
    DuiLib::CMenuElementUI* menu_file_ = nullptr;
    DuiLib::CMenuElementUI* menu_edit_ = nullptr;
    DuiLib::CMenuElementUI* menu_view_ = nullptr;
    DuiLib::CMenuElementUI* menu_tools_ = nullptr;
    DuiLib::CMenuElementUI* menu_help_ = nullptr;

    DuiLib::CHorizontalLayoutUI* toolbar_ = nullptr;
    DuiLib::CButtonUI* btn_new_ = nullptr;
    DuiLib::CButtonUI* btn_open_ = nullptr;
    DuiLib::CButtonUI* btn_extract_ = nullptr;
    DuiLib::CButtonUI* btn_add_ = nullptr;
    DuiLib::CButtonUI* btn_test_ = nullptr;
    DuiLib::CButtonUI* btn_delete_ = nullptr;
    DuiLib::CButtonUI* btn_find_ = nullptr;
    DuiLib::CButtonUI* btn_back_ = nullptr;
    DuiLib::CButtonUI* btn_up_ = nullptr;

    DuiLib::CHorizontalLayoutUI* address_bar_ = nullptr;
    DuiLib::CEditUI* edit_address_ = nullptr;

    DuiLib::CListUI* list_ = nullptr;
    DuiLib::CTreeViewUI* tree_ = nullptr;

    DuiLib::CStatusBarUI* status_bar_ = nullptr;
    DuiLib::CLabelUI* lbl_status_count_ = nullptr;
    DuiLib::CLabelUI* lbl_status_size_ = nullptr;
    DuiLib::CLabelUI* lbl_status_ratio_ = nullptr;

    // 状态
    std::unique_ptr<IArchive> current_archive_;
    tstring current_archive_path_;
    tstring current_password_;
    std::vector<ArchiveEntry> current_entries_;

    // 视图模式
    enum class ViewMode {
        Details,
        List,
        LargeIcons,
        SmallIcons,
        Tree,
    };
    ViewMode view_mode_ = ViewMode::Details;

    // 查找
    tstring find_text_;
    size_t find_pos_ = 0;

    // 拖拽
    bool drag_drop_enabled_ = true;
    bool is_dragging_ = false;

    // 后台任务
    std::atomic<bool> task_running_{false};
    std::atomic<bool> task_cancelled_{false};

    // 进度对话框
    class ProgressDialog* progress_dlg_ = nullptr;
};

} // namespace ui
} // namespace bandzip
