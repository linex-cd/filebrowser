// diskviewer.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "filebrowser.h"

#include <shellapi.h>

#pragma comment(lib, "comctl32.lib")


#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <stdio.h>

#define IDC_TREEVIEW 1001
#define IDC_LISTVIEW 1002
#define ID_LISTVIEW_DELETE 40001
#define ID_LISTVIEW_RENAME 40002
#define ID_LISTVIEW_COPY 40003
#define ID_LISTVIEW_PASTE 40004
#define ID_LISTVIEW_REFRESH 40005
#define ID_LISTVIEW_PROPERTIES 40006

const char g_szClassName[] = "myWindowClass";
static char currentPath[MAX_PATH] = { 0 };
static char copiedFilePath[MAX_PATH] = { 0 };
static BOOL copyOperation = FALSE; // TRUE: Copy, FALSE: Cut

// Function prototypes
void InitTreeViewItems(HWND hwndTV);
void AddItemsToListView(HWND hwndLV, LPCSTR path);
void InitListViewColumns(HWND hwndLV);
void AddItemsToTreeView(HWND hwndTV, HTREEITEM hParent, LPCSTR path);
void DisplayFileContent(HWND hwndLV, LPCSTR path);
void ShowContextMenu(HWND hwnd, POINT pt);
void RefreshListView(HWND hwndLV, LPCSTR path);
void DeleteSelectedItem(HWND hwndLV);
void RenameSelectedItem(HWND hwndLV);
void CopySelectedItem(HWND hwndLV);
void PasteItem(HWND hwndLV);
void ShowFileProperties(HWND hwndLV);

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static HWND hwndTV, hwndLV;
    switch (msg)
    {
    case WM_CREATE:
    {
        INITCOMMONCONTROLSEX icex;
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_TREEVIEW_CLASSES | ICC_LISTVIEW_CLASSES;
        InitCommonControlsEx(&icex);

        RECT rcClient;
        GetClientRect(hwnd, &rcClient);

        hwndTV = CreateWindowEx(0, WC_TREEVIEW, TEXT("Tree View"),
            WS_VISIBLE | WS_CHILD | WS_BORDER | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS,
            0, 0, rcClient.right / 3, rcClient.bottom,
            hwnd, (HMENU)IDC_TREEVIEW, GetModuleHandle(NULL), NULL);

        hwndLV = CreateWindowEx(0, WC_LISTVIEW, TEXT("List View"),
            WS_VISIBLE | WS_CHILD | WS_BORDER | LVS_REPORT | LVS_EDITLABELS,
            rcClient.right / 3, 0, rcClient.right - rcClient.right / 3, rcClient.bottom,
            hwnd, (HMENU)IDC_LISTVIEW, GetModuleHandle(NULL), NULL);
        ListView_SetExtendedListViewStyle(hwndLV, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

        InitTreeViewItems(hwndTV);
        InitListViewColumns(hwndLV);
    }
    break;
    case WM_NOTIFY:
    {
        LPNMHDR lpNMHdr = (LPNMHDR)lParam;
        if (lpNMHdr->idFrom == IDC_TREEVIEW && lpNMHdr->code == TVN_SELCHANGED)
        {
            LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lParam;
            TVITEM item = pnmtv->itemNew;
            char path[MAX_PATH] = { 0 };
            HTREEITEM hItem = item.hItem;
            TVITEM tvItem;
            char buffer[MAX_PATH] = { 0 };

            // Traverse the tree to get the full path
            while (hItem)
            {
                ZeroMemory(&tvItem, sizeof(tvItem));
                tvItem.hItem = hItem;
                tvItem.mask = TVIF_TEXT;
                tvItem.pszText = buffer;
                tvItem.cchTextMax = sizeof(buffer);
                TreeView_GetItem(hwndTV, &tvItem);
                char temp[MAX_PATH];
                strcpy(temp, buffer);
                strcat(temp, "\\");
                strcat(temp, path);
                strcpy(path, temp);
                hItem = TreeView_GetParent(hwndTV, hItem);
            }

            // 去掉最后的反斜杠
            if (path[strlen(path) - 1] == '\\') {
                path[strlen(path) - 1] = '\0';
            }

            strcpy(currentPath, path); // 保存当前路径
            AddItemsToListView(hwndLV, path);

            // Load subdirectories into the TreeView if not loaded yet
            if (TreeView_GetChild(hwndTV, item.hItem) == NULL)
            {
                AddItemsToTreeView(hwndTV, item.hItem, path);
            }
        }
        else if (lpNMHdr->idFrom == IDC_LISTVIEW && lpNMHdr->code == NM_DBLCLK)
        {
            LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lParam;
            char buffer[MAX_PATH];
            ListView_GetItemText(hwndLV, lpnmitem->iItem, 0, buffer, sizeof(buffer));
            char fullPath[MAX_PATH];
            sprintf(fullPath, "%s\\%s", currentPath, buffer);
            DisplayFileContent(hwndLV, fullPath);
        }
        else if (lpNMHdr->idFrom == IDC_LISTVIEW && lpNMHdr->code == NM_RCLICK)
        {
            POINT pt;
            GetCursorPos(&pt);
            ShowContextMenu(hwnd, pt);
        }
    }
    break;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_LISTVIEW_DELETE:
            DeleteSelectedItem(hwndLV);
            break;
        case ID_LISTVIEW_RENAME:
            RenameSelectedItem(hwndLV);
            break;
        case ID_LISTVIEW_COPY:
            CopySelectedItem(hwndLV);
            break;
        case ID_LISTVIEW_PASTE:
            PasteItem(hwndLV);
            break;
        case ID_LISTVIEW_REFRESH:
            RefreshListView(hwndLV, currentPath);
            break;
        case ID_LISTVIEW_PROPERTIES:
            ShowFileProperties(hwndLV);
            break;
        }
        break;
    case WM_SIZE:
    {
        RECT rcClient;
        GetClientRect(hwnd, &rcClient);
        SetWindowPos(hwndTV, NULL, 0, 0, rcClient.right / 3, rcClient.bottom, SWP_NOZORDER);
        SetWindowPos(hwndLV, NULL, rcClient.right / 3, 0, rcClient.right - rcClient.right / 3, rcClient.bottom, SWP_NOZORDER);
    }
    break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void InitTreeViewItems(HWND hwndTV)
{
    TVINSERTSTRUCT tvins;
    HTREEITEM hPrev = (HTREEITEM)TVI_ROOT;

    char szDrive[] = "A:\\";
    DWORD dwDrives = GetLogicalDrives();
    for (char c = 'A'; c <= 'Z'; c++)
    {
        if (dwDrives & 1)
        {
            szDrive[0] = c;
            tvins.hParent = TVI_ROOT;
            tvins.hInsertAfter = hPrev;
            tvins.item.mask = TVIF_TEXT | TVIF_CHILDREN;
            tvins.item.pszText = szDrive;
            tvins.item.cChildren = 1;
            hPrev = TreeView_InsertItem(hwndTV, &tvins);
        }
        dwDrives >>= 1;
    }
}

void InitListViewColumns(HWND hwndLV)
{
    LVCOLUMN lvc;

    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    lvc.iSubItem = 0;
    lvc.pszText = "Name";
    lvc.cx = 200;
    ListView_InsertColumn(hwndLV, 0, &lvc);

    lvc.iSubItem = 1;
    lvc.pszText = "Type";
    lvc.cx = 100;
    ListView_InsertColumn(hwndLV, 1, &lvc);

    lvc.iSubItem = 2;
    lvc.pszText = "Size";
    lvc.cx = 100;
    ListView_InsertColumn(hwndLV, 2, &lvc);

    lvc.iSubItem = 3;
    lvc.pszText = "Creation Time";
    lvc.cx = 150;
    ListView_InsertColumn(hwndLV, 3, &lvc);

    lvc.iSubItem = 4;
    lvc.pszText = "Modification Time";
    lvc.cx = 150;
    ListView_InsertColumn(hwndLV, 4, &lvc);
}

void AddItemsToListView(HWND hwndLV, LPCSTR path)
{
    ListView_DeleteAllItems(hwndLV);

    // 添加返回上级目录的项
    LVITEM lvI;
    lvI.mask = LVIF_TEXT;
    lvI.iItem = 0;
    lvI.iSubItem = 0;
    lvI.pszText = "..";
    ListView_InsertItem(hwndLV, &lvI);
    ListView_SetItemText(hwndLV, lvI.iItem, 1, "Parent"); // Type
    ListView_SetItemText(hwndLV, lvI.iItem, 2, ""); // Size
    ListView_SetItemText(hwndLV, lvI.iItem, 3, ""); // Creation Time
    ListView_SetItemText(hwndLV, lvI.iItem, 4, ""); // Modification Time

    WIN32_FIND_DATA ffd;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    char szDir[MAX_PATH];
    sprintf(szDir, "%s\\*", path);

    hFind = FindFirstFile(szDir, &ffd);

    if (INVALID_HANDLE_VALUE == hFind)
        return;

    int itemIndex = 1;
    do
    {
        if (strcmp(ffd.cFileName, ".") != 0 && strcmp(ffd.cFileName, "..") != 0)
        {
            lvI.mask = LVIF_TEXT;
            lvI.iItem = itemIndex++;
            lvI.iSubItem = 0;
            lvI.pszText = ffd.cFileName;
            ListView_InsertItem(hwndLV, &lvI);

            ListView_SetItemText(hwndLV, lvI.iItem, 1, (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? "Directory" : "File");

            char sizeText[32];
            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                strcpy(sizeText, "<DIR>");
            }
            else
            {
                char sizeText[20];
                ULONGLONG size = ((ULONGLONG)ffd.nFileSizeHigh << 32) + ffd.nFileSizeLow;

                ULONGLONG range_GB = (ULONGLONG)(1024 * 1024) * (ULONGLONG)(1024 );
                ULONGLONG range_TB = (ULONGLONG)(1024 * 1024) * (ULONGLONG)(1024 * 1024);

                if (size < 1024)
                {
                    sprintf(sizeText, "%llu B", size);
                }
                else if (size >= 1024 && size < 1024*1024)
                {
                    sprintf(sizeText, "%llu KB", size / 1024);
                }
                else if (size >= 1024*1024 && size < 1024 * 1024 * 1024)
                {
                    sprintf(sizeText, "%llu MB", size / 1024 / 1024);
                }
                else if (size >= range_GB && size < range_TB)
                {
                    sprintf(sizeText, "%llu GB", size / range_GB);
                }
                else if (size >= range_TB)
                {
                    sprintf(sizeText, "%llu TB", size / range_TB);
                }

                
                ListView_SetItemText(hwndLV, lvI.iItem, 2, sizeText);

               
            }
            

            // Convert FILETIME to system time and then to a string
            SYSTEMTIME stUTC, stLocal;
            char creationTime[64], modificationTime[64];

            FileTimeToSystemTime(&ffd.ftCreationTime, &stUTC);
            SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
            sprintf(creationTime, "%02d/%02d/%04d %02d:%02d:%02d",
                stLocal.wDay, stLocal.wMonth, stLocal.wYear,
                stLocal.wHour, stLocal.wMinute, stLocal.wSecond);
            ListView_SetItemText(hwndLV, lvI.iItem, 3, creationTime);

            FileTimeToSystemTime(&ffd.ftLastWriteTime, &stUTC);
            SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
            sprintf(modificationTime, "%02d/%02d/%04d %02d:%02d:%02d",
                stLocal.wDay, stLocal.wMonth, stLocal.wYear,
                stLocal.wHour, stLocal.wMinute, stLocal.wSecond);
            ListView_SetItemText(hwndLV, lvI.iItem, 4, modificationTime);
        }
    } while (FindNextFile(hFind, &ffd) != 0);

    FindClose(hFind);
}


void AddItemsToTreeView(HWND hwndTV, HTREEITEM hParent, LPCSTR path)
{
    TVINSERTSTRUCT tvins;
    HTREEITEM hPrev = (HTREEITEM)TVI_FIRST;

    WIN32_FIND_DATA ffd;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    char szDir[MAX_PATH];
    sprintf(szDir, "%s\\*", path);

    hFind = FindFirstFile(szDir, &ffd);

    if (INVALID_HANDLE_VALUE == hFind)
        return;

    do
    {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (strcmp(ffd.cFileName, ".") != 0 && strcmp(ffd.cFileName, "..") != 0)
            {
                tvins.hParent = hParent;
                tvins.hInsertAfter = hPrev;
                tvins.item.mask = TVIF_TEXT | TVIF_CHILDREN;
                tvins.item.pszText = ffd.cFileName;
                tvins.item.cChildren = 1;
                hPrev = TreeView_InsertItem(hwndTV, &tvins);
            }
        }
    } while (FindNextFile(hFind, &ffd) != 0);

    FindClose(hFind);
}

void DisplayFileContent(HWND hwndLV, LPCSTR path)
{
    if (strcmp(path, "..") == 0)
    {
        // Go up one directory level
        char* pLastSlash = strrchr(currentPath, '\\');
        if (pLastSlash != NULL)
        {
            *pLastSlash = '\0'; // Truncate the current path at the last backslash
            if (strlen(currentPath) == 2 && currentPath[1] == ':') // Handle root directory case
            {
                strcat(currentPath, "\\");
            }
        }
        RefreshListView(hwndLV, currentPath);
    }

    DWORD attributes = GetFileAttributes(path);
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY))
    {
        // If it's a directory, change current path and refresh the ListView
        strcpy(currentPath, path);
        AddItemsToListView(hwndLV, path);
    }
    else
    {
        // If it's a file, open it
        ShellExecute(NULL, "open", path, NULL, NULL, SW_SHOW);
    }
}

void ShowContextMenu(HWND hwnd, POINT pt)
{
    HMENU hMenu = CreatePopupMenu();
    InsertMenu(hMenu, 0, MF_BYPOSITION | MF_STRING, ID_LISTVIEW_DELETE, "Delete");
    InsertMenu(hMenu, 1, MF_BYPOSITION | MF_STRING, ID_LISTVIEW_RENAME, "Rename");
    InsertMenu(hMenu, 2, MF_BYPOSITION | MF_STRING, ID_LISTVIEW_COPY, "Copy");
    InsertMenu(hMenu, 3, MF_BYPOSITION | MF_STRING, ID_LISTVIEW_PASTE, "Paste");
    InsertMenu(hMenu, 4, MF_BYPOSITION | MF_STRING, ID_LISTVIEW_REFRESH, "Refresh");
    InsertMenu(hMenu, 5, MF_BYPOSITION | MF_STRING, ID_LISTVIEW_PROPERTIES, "Properties");

    // Disable Paste if nothing is copied
    if (copiedFilePath[0] == '\0')
    {
        EnableMenuItem(hMenu, ID_LISTVIEW_PASTE, MF_BYCOMMAND | MF_GRAYED);
    }

    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

void RefreshListView(HWND hwndLV, LPCSTR path)
{
    AddItemsToListView(hwndLV, path);
}

void DeleteSelectedItem(HWND hwndLV)
{
    int iSelected = ListView_GetNextItem(hwndLV, -1, LVNI_SELECTED);
    if (iSelected != -1)
    {
        char buffer[MAX_PATH];
        ListView_GetItemText(hwndLV, iSelected, 0, buffer, sizeof(buffer));
        char fullPath[MAX_PATH];
        sprintf(fullPath, "%s\\%s", currentPath, buffer);

        // Show confirmation dialog
        char confirmMessage[MAX_PATH + 50];
        sprintf(confirmMessage, "Are you sure you want to delete '%s'?", buffer);
        int result = MessageBox(hwndLV, confirmMessage, "Confirm Delete", MB_YESNO | MB_ICONQUESTION);

        // Proceed with deletion if user confirmed
        if (result == IDYES)
        {
            if (DeleteFile(fullPath) || RemoveDirectory(fullPath))
            {
                ListView_DeleteItem(hwndLV, iSelected);
            }
            else
            {
                MessageBox(hwndLV, "Delete operation failed", "Error", MB_ICONERROR);
            }
        }
    }
}


void RenameSelectedItem(HWND hwndLV)
{
    int iSelected = ListView_GetNextItem(hwndLV, -1, LVNI_SELECTED);
    if (iSelected != -1)
    {
        ListView_EditLabel(hwndLV, iSelected);
    }
}

void CopySelectedItem(HWND hwndLV)
{
    int iSelected = ListView_GetNextItem(hwndLV, -1, LVNI_SELECTED);
    if (iSelected != -1)
    {
        char fileName[MAX_PATH];
        ListView_GetItemText(hwndLV, iSelected, 0, fileName, sizeof(fileName));
        sprintf(copiedFilePath, "%s\\%s", currentPath, fileName);
        copyOperation = TRUE; // Set the operation to Copy
    }
}


void PasteItem(HWND hwndLV)
{
    if (copiedFilePath[0] != '\0')
    {
        char fileName[MAX_PATH];
        char* pFileName = strrchr(copiedFilePath, '\\');
        if (pFileName != NULL)
        {
            pFileName++; // Move past the backslash to get the file name
            strcpy(fileName, pFileName);
        }
        else
        {
            strcpy(fileName, copiedFilePath); // Fallback, should not happen
        }

        char destPath[MAX_PATH];
        sprintf(destPath, "%s\\%s", currentPath, fileName);

        // Check if file already exists in destination and modify name if necessary
        int copyIndex = 1;
        while (GetFileAttributes(destPath) != INVALID_FILE_ATTRIBUTES)
        {
            sprintf(destPath, "%s\\Copy (%d) of %s", currentPath, copyIndex++, fileName);
        }

        if (CopyFile(copiedFilePath, destPath, FALSE))
        {
            RefreshListView(hwndLV, currentPath);
            copiedFilePath[0] = '\0'; // Clear the copied file path
        }
        else
        {
            MessageBox(hwndLV, "Paste operation failed", "Error", MB_ICONERROR);
        }
    }
}

void ShowFileProperties(HWND hwndLV)
{
    int iSelected = ListView_GetNextItem(hwndLV, -1, LVNI_SELECTED);
    if (iSelected != -1)
    {
        char buffer[MAX_PATH];
        ListView_GetItemText(hwndLV, iSelected, 0, buffer, sizeof(buffer));
        char fullPath[MAX_PATH];
        sprintf(fullPath, "%s\\%s", currentPath, buffer);

        SHELLEXECUTEINFO sei;
        memset(&sei, 0, sizeof(sei));
        sei.cbSize = sizeof(SHELLEXECUTEINFO);
        sei.lpVerb = "properties";
        sei.lpFile = fullPath;
        sei.nShow = SW_SHOW;
        sei.fMask = SEE_MASK_INVOKEIDLIST;

    
        ShellExecuteEx(&sei);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEX wc;
    HWND hwnd;
    MSG Msg;

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = 0;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = g_szClassName;
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc))
    {
        MessageBox(NULL, "Window Registration Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    hwnd = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        g_szClassName,
        "File Browser",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL);

    if (hwnd == NULL)
    {
        MessageBox(NULL, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    while (GetMessage(&Msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
    }
    return Msg.wParam;
}
