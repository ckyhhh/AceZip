// ============================================================================
// drag_drop.cpp - 拖拽支持实现
//
// BandzipClone - 类 Bandizip 的开源解压缩软件
// Copyright (C) 2024 BandzipClone Contributors
// Licensed under AGPLv3
// ============================================================================
#include "drag_drop.h"
#include "../utils/string_utils.h"
#include "../utils/file_utils.h"
#include "../utils/path_utils.h"
#include "../utils/logger.h"
#include "../core/archive.h"

#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <objbase.h>
#include <algorithm>

namespace bandzip {
namespace ui {

// ===========================================================================
// DropTarget
// ===========================================================================

DropTarget::DropTarget() {}

DropTarget::~DropTarget() {}

IFACEMETHODIMP DropTarget::QueryInterface(REFIID riid, void** ppv) {
    static const QITAB qit[] = {
        QITABENT(DropTarget, IDropTarget),
        { nullptr, 0 },
    };
    return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG) DropTarget::AddRef() {
    return InterlockedIncrement(&ref_count_);
}

IFACEMETHODIMP_(ULONG) DropTarget::Release() {
    LONG count = InterlockedDecrement(&ref_count_);
    if (count == 0) delete this;
    return count;
}

IFACEMETHODIMP DropTarget::DragEnter(IDataObject* pDataObj,
                                       DWORD grfKeyState,
                                       POINTL pt,
                                       DWORD* pdwEffect) {
    has_files_ = HasFiles(pDataObj);
    has_archives_ = false;

    if (has_files_) {
        auto files = ExtractFiles(pDataObj);
        for (const auto& f : files) {
            if (IsArchive(f)) {
                has_archives_ = true;
                break;
            }
        }
    }

    if (has_archives_ && !is_archive_open_) {
        *pdwEffect = DROPEFFECT_COPY;
    } else if (has_files_ && is_archive_open_) {
        *pdwEffect = DROPEFFECT_COPY;
    } else if (has_files_) {
        *pdwEffect = DROPEFFECT_COPY;
    } else {
        *pdwEffect = DROPEFFECT_NONE;
    }

    return S_OK;
}

IFACEMETHODIMP DropTarget::DragOver(DWORD grfKeyState,
                                       POINTL pt,
                                       DWORD* pdwEffect) {
    if (has_archives_ && !is_archive_open_) {
        *pdwEffect = DROPEFFECT_COPY;
    } else if (has_files_) {
        *pdwEffect = DROPEFFECT_COPY;
    } else {
        *pdwEffect = DROPEFFECT_NONE;
    }
    return S_OK;
}

IFACEMETHODIMP DropTarget::DragLeave() {
    has_files_ = false;
    has_archives_ = false;
    return S_OK;
}

IFACEMETHODIMP DropTarget::Drop(IDataObject* pDataObj,
                                   DWORD grfKeyState,
                                   POINTL pt,
                                   DWORD* pdwEffect) {
    if (!has_files_) {
        *pdwEffect = DROPEFFECT_NONE;
        return S_OK;
    }

    auto files = ExtractFiles(pDataObj);
    if (files.empty()) {
        *pdwEffect = DROPEFFECT_NONE;
        return S_OK;
    }

    // 判断是打开压缩包还是添加文件
    if (!is_archive_open_) {
        // 检查是否为压缩包
        for (const auto& f : files) {
            if (IsArchive(f)) {
                if (on_drop_archive_) {
                    on_drop_archive_(f);
                }
                *pdwEffect = DROPEFFECT_COPY;
                return S_OK;
            }
        }
    }

    // 添加文件
    if (on_drop_files_) {
        on_drop_files_(files);
    }

    *pdwEffect = DROPEFFECT_COPY;
    return S_OK;
}

bool DropTarget::HasFiles(IDataObject* pDataObj) {
    FORMATETC fmt = {};
    fmt.cfFormat = CF_HDROP;
    fmt.ptd = nullptr;
    fmt.dwAspect = DVASPECT_CONTENT;
    fmt.lindex = -1;
    fmt.tymed = TYMED_HGLOBAL;

    return pDataObj->QueryGetData(&fmt) == S_OK;
}

std::vector<tstring> DropTarget::ExtractFiles(IDataObject* pDataObj) {
    std::vector<tstring> files;

    FORMATETC fmt = {};
    fmt.cfFormat = CF_HDROP;
    fmt.ptd = nullptr;
    fmt.dwAspect = DVASPECT_CONTENT;
    fmt.lindex = -1;
    fmt.tymed = TYMED_HGLOBAL;

    STGMEDIUM medium = {};
    if (FAILED(pDataObj->GetData(&fmt, &medium))) {
        return files;
    }

    HDROP hDrop = static_cast<HDROP>(GlobalLock(medium.hGlobal));
    if (!hDrop) {
        ReleaseStgMedium(&medium);
        return files;
    }

    UINT count = DragQueryFile(hDrop, 0xFFFFFFFF, nullptr, 0);
    for (UINT i = 0; i < count; ++i) {
        tchar buf[MAX_PATH] = {0};
        DragQueryFile(hDrop, i, buf, MAX_PATH);
        files.push_back(tstring(buf));
    }

    GlobalUnlock(medium.hGlobal);
    ReleaseStgMedium(&medium);

    return files;
}

bool DropTarget::IsArchive(const tstring& path) {
    if (!util::file_exists(path)) return false;
    ArchiveFormat fmt = util::detect_format(path);
    return fmt != ArchiveFormat::Unknown;
}

// ===========================================================================
// DropSource
// ===========================================================================

DropSource::DropSource() {}

DropSource::~DropSource() {}

IFACEMETHODIMP DropSource::QueryInterface(REFIID riid, void** ppv) {
    static const QITAB qit[] = {
        QITABENT(DropSource, IDropSource),
        { nullptr, 0 },
    };
    return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG) DropSource::AddRef() {
    return InterlockedIncrement(&ref_count_);
}

IFACEMETHODIMP_(ULONG) DropSource::Release() {
    LONG count = InterlockedDecrement(&ref_count_);
    if (count == 0) delete this;
    return count;
}

IFACEMETHODIMP DropSource::QueryContinueDrag(BOOL fEscapePressed,
                                                DWORD grfKeyState) {
    if (fEscapePressed) {
        return DRAGDROP_S_CANCEL;
    }

    // 左键松开 -> 放下
    if (!(grfKeyState & MK_LBUTTON)) {
        return DRAGDROP_S_DROP;
    }

    // 右键松开 -> 取消
    if (!(grfKeyState & MK_RBUTTON) && (grfKeyState & MK_LBUTTON)) {
        // 继续
    }

    return S_OK;
}

IFACEMETHODIMP DropSource::GiveFeedback(DWORD dwEffect) {
    return DRAGDROP_S_USEDEFAULTCURSORS;
}

// ===========================================================================
// DataObject
// ===========================================================================

DataObject::DataObject(const std::vector<tstring>& files) : files_(files) {}

DataObject::~DataObject() {}

IFACEMETHODIMP DataObject::QueryInterface(REFIID riid, void** ppv) {
    static const QITAB qit[] = {
        QITABENT(DataObject, IDataObject),
        { nullptr, 0 },
    };
    return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG) DataObject::AddRef() {
    return InterlockedIncrement(&ref_count_);
}

IFACEMETHODIMP_(ULONG) DataObject::Release() {
    LONG count = InterlockedDecrement(&ref_count_);
    if (count == 0) delete this;
    return count;
}

IFACEMETHODIMP DataObject::GetData(FORMATETC* pformatetcIn,
                                      STGMEDIUM* pmedium) {
    if (pformatetcIn->cfFormat == CF_HDROP &&
        pformatetcIn->tymed & TYMED_HGLOBAL) {

        // 构建 DROPFILES 结构
        size_t total_len = sizeof(DROPFILES);
        for (const auto& f : files_) {
            total_len += (f.length() + 1) * sizeof(tchar);
        }
        total_len += sizeof(tchar);  // 末尾额外 null

        HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, total_len);
        if (!hGlobal) return E_OUTOFMEMORY;

        auto* df = static_cast<DROPFILES*>(GlobalLock(hGlobal));
        df->pFiles = sizeof(DROPFILES);
#ifdef _UNICODE
        df->fWide = TRUE;
#else
        df->fWide = FALSE;
#endif

        tchar* p = reinterpret_cast<tchar*>(
            reinterpret_cast<char*>(df) + sizeof(DROPFILES));
        for (const auto& f : files_) {
            _tcscpy_s(p, f.length() + 1, f.c_str());
            p += f.length() + 1;
        }
        *p = _T('\0');

        GlobalUnlock(hGlobal);

        pmedium->tymed = TYMED_HGLOBAL;
        pmedium->hGlobal = hGlobal;
        pmedium->pUnkForRelease = nullptr;

        return S_OK;
    }

    return DV_E_FORMATETC;
}

IFACEMETHODIMP DataObject::GetDataHere(FORMATETC* pformatetc,
                                          STGMEDIUM* pmedium) {
    return E_NOTIMPL;
}

IFACEMETHODIMP DataObject::QueryGetData(FORMATETC* pformatetc) {
    if (pformatetc->cfFormat == CF_HDROP &&
        pformatetc->tymed & TYMED_HGLOBAL) {
        return S_OK;
    }
    return DV_E_FORMATETC;
}

IFACEMETHODIMP DataObject::GetCanonicalFormatEtc(FORMATETC* pformatetcIn,
                                                     FORMATETC* pformatetcOut) {
    *pformatetcOut = *pformatetcIn;
    pformatetcOut->ptd = nullptr;
    return S_FALSE;
}

IFACEMETHODIMP DataObject::SetData(FORMATETC* pformatetc,
                                      STGMEDIUM* pmedium,
                                      BOOL fRelease) {
    return E_NOTIMPL;
}

IFACEMETHODIMP DataObject::EnumFormatEtc(DWORD dwDirection,
                                            IEnumFORMATETC** ppenumFormatEtc) {
    return OLE_S_USEREG;
}

IFACEMETHODIMP DataObject::DAdvise(FORMATETC* pformatetc,
                                      DWORD advf,
                                      IAdviseSink* pAdvSink,
                                      DWORD* pdwConnection) {
    return OLE_E_ADVISENOTSUPPORTED;
}

IFACEMETHODIMP DataObject::DUnadvise(DWORD dwConnection) {
    return OLE_E_ADVISENOTSUPPORTED;
}

IFACEMETHODIMP DataObject::EnumDAdvise(IEnumSTATDATA** ppenumAdvise) {
    return OLE_E_ADVISENOTSUPPORTED;
}

} // namespace ui
} // namespace bandzip
