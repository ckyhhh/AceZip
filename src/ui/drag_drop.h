// ============================================================================
// drag_drop.h - 拖拽支持
//
// 实现 IDropTarget 接口，支持：
// - 拖入压缩包到主窗口打开
// - 拖入文件到主窗口创建压缩包
// - 从主窗口拖出文件解压
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#pragma once

#include "../core/archive.h"
#include <oleidl.h>
#include <shobjidl.h>
#include <vector>
#include <string>
#include <functional>

namespace bandzip {
namespace ui {

class DropTarget : public IDropTarget {
public:
    DropTarget();
    virtual ~DropTarget();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    // IDropTarget
    IFACEMETHODIMP DragEnter(IDataObject* pDataObj,
                              DWORD grfKeyState,
                              POINTL pt,
                              DWORD* pdwEffect) override;
    IFACEMETHODIMP DragOver(DWORD grfKeyState,
                              POINTL pt,
                              DWORD* pdwEffect) override;
    IFACEMETHODIMP DragLeave() override;
    IFACEMETHODIMP Drop(IDataObject* pDataObj,
                         DWORD grfKeyState,
                         POINTL pt,
                         DWORD* pdwEffect) override;

    // 配置
    void SetOnDropFiles(std::function<void(const std::vector<tstring>&)> cb) {
        on_drop_files_ = std::move(cb);
    }

    void SetOnDropArchive(std::function<void(const tstring&)> cb) {
        on_drop_archive_ = std::move(cb);
    }

    void SetIsArchiveOpen(bool is_open) { is_archive_open_ = is_open; }

private:
    LONG ref_count_ = 1;

    bool is_archive_open_ = false;
    bool has_files_ = false;
    bool has_archives_ = false;

    std::function<void(const std::vector<tstring>&)> on_drop_files_;
    std::function<void(const tstring&)> on_drop_archive_;

    // 检查 IDataObject 是否包含文件
    bool HasFiles(IDataObject* pDataObj);

    // 提取文件列表
    std::vector<tstring> ExtractFiles(IDataObject* pDataObj);

    // 检查是否为压缩包
    bool IsArchive(const tstring& path);
};

// ---------------------------------------------------------------------------
// IDropSource - 从列表拖出
// ---------------------------------------------------------------------------
class DropSource : public IDropSource {
public:
    DropSource();
    virtual ~DropSource();

    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    IFACEMETHODIMP QueryContinueDrag(BOOL fEscapePressed,
                                       DWORD grfKeyState) override;
    IFACEMETHODIMP GiveFeedback(DWORD dwEffect) override;

private:
    LONG ref_count_ = 1;
};

// ---------------------------------------------------------------------------
// IDataObject 实现 - 用于拖出文件
// ---------------------------------------------------------------------------
class DataObject : public IDataObject {
public:
    DataObject(const std::vector<tstring>& files);
    virtual ~DataObject();

    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    IFACEMETHODIMP GetData(FORMATETC* pformatetcIn,
                            STGMEDIUM* pmedium) override;
    IFACEMETHODIMP GetDataHere(FORMATETC* pformatetc,
                                STGMEDIUM* pmedium) override;
    IFACEMETHODIMP QueryGetData(FORMATETC* pformatetc) override;
    IFACEMETHODIMP GetCanonicalFormatEtc(FORMATETC* pformatetcIn,
                                          FORMATETC* pformatetcOut) override;
    IFACEMETHODIMP SetData(FORMATETC* pformatetc,
                            STGMEDIUM* pmedium,
                            BOOL fRelease) override;
    IFACEMETHODIMP EnumFormatEtc(DWORD dwDirection,
                                   IEnumFORMATETC** ppenumFormatEtc) override;
    IFACEMETHODIMP DAdvise(FORMATETC* pformatetc,
                            DWORD advf,
                            IAdviseSink* pAdvSink,
                            DWORD* pdwConnection) override;
    IFACEMETHODIMP DUnadvise(DWORD dwConnection) override;
    IFACEMETHODIMP EnumDAdvise(IEnumSTATDATA** ppenumAdvise) override;

private:
    LONG ref_count_ = 1;
    std::vector<tstring> files_;
};

} // namespace ui
} // namespace bandzip
