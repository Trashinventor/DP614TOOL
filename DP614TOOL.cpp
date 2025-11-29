#include <windows.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <commdlg.h>
#include <string.h>
#include <commctrl.h>

#define MAX_WIDTH 40
#pragma comment(lib, "comdlg32.lib")
#pragma pack(push, 1)
#pragma comment(lib, "comctl32.lib")
int threshold1 = 128;
int threshold2 = 192;

typedef struct
{
    unsigned short bfType;
    unsigned int bfSize;
    unsigned short bfReserved1;
    unsigned short bfReserved2;
    unsigned int bfOffBits;
} BMPHeader;

typedef struct
{
    unsigned int biSize;
    int biWidth;
    int biHeight;
    unsigned short biPlanes;
    unsigned short biBitCount;
    unsigned int biCompression;
    unsigned int biSizeImage;
    int biXPelsPerMeter;
    int biYPelsPerMeter;
    unsigned int biClrUsed;
    unsigned int biClrImportant;
} BMPInfoHeader;
#pragma pack(pop)
int selectedBlockStyle = 0; // 0 = default

// Function to open file dialog and return the selected file path
std::string OpenFileDialog(HWND hwnd)
{
    OPENFILENAME ofn = {};            // Initializing the OPENFILENAME structure
    wchar_t fileName[MAX_PATH] = L""; // String to store the selected file name (wide character)

    // Fill in the OPENFILENAME structure with options
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hwnd;                                       // Owner window handle
    ofn.lpstrFilter = L"Bitmap Files\0*.bmp\0All Files\0*.*\0"; // Filter for bitmap and all files
    ofn.lpstrFile = fileName;                                   // Buffer to receive the file path
    ofn.nMaxFile = MAX_PATH;                                    // Maximum file path length
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;          // Flags to ensure the file exists
    ofn.lpstrTitle = L"Select an Image File";                   // Dialog title

    // Show the open file dialog and check if the user selected a file
    if (GetOpenFileName(&ofn))
    {
        // Convert the wide string (fileName) to a narrow string and return
        int bufferSize = WideCharToMultiByte(CP_UTF8, 0, fileName, -1, nullptr, 0, nullptr, nullptr);
        char *filePath = new char[bufferSize];
        WideCharToMultiByte(CP_UTF8, 0, fileName, -1, filePath, bufferSize, nullptr, nullptr);
        std::string result(filePath);
        delete[] filePath;
        return result; // Return the selected file path as a narrow string
    }

    return ""; // Return empty string if no file was selected
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Function prototypes
bool read_bmp_headers(const char *image_path, BMPHeader &bmpHeader, BMPInfoHeader &bmpInfoHeader, FILE *&file);
unsigned char *read_image_data(FILE *file, const BMPHeader &bmpHeader, int row_padded, int height);
void process_4bit_image(HWND hEdit, int width, int height, int row_padded, unsigned char *imageData);
void setup_window(HWND hwnd, HFONT &hFont, HWND &hEdit);

void generate_ascii_art(HWND hEditAscii, HWND hEditNumeric, const char *image_path)
{
    FILE *file = nullptr;
    BMPHeader bmpHeader;
    BMPInfoHeader bmpInfoHeader;

    file = fopen(image_path, "rb");
    if (!file)
    {
        MessageBox(hEditAscii, L"Error: Unable to open image file.", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    fread(&bmpHeader, sizeof(BMPHeader), 1, file);
    fread(&bmpInfoHeader, sizeof(BMPInfoHeader), 1, file);

    int width = bmpInfoHeader.biWidth;
    int height = bmpInfoHeader.biHeight;
    int row_padded = (width + 1) / 2;

    unsigned char *imageData = (unsigned char *)malloc(row_padded * height);
    fseek(file, bmpHeader.bfOffBits, SEEK_SET);
    fread(imageData, 1, row_padded * height, file);
    fclose(file);

    if (bmpInfoHeader.biBitCount != 4)
    {
        MessageBox(hEditAscii, L"Error: Unsupported BMP BitCount. This code handles 4-bit images.", L"Error", MB_OK | MB_ICONERROR);
        free(imageData);
        return;
    }
    wchar_t buffer1[8] = L"#";
    wchar_t buffer2[8] = L"@";
    wchar_t buffer3[8] = L"%";

    // If Option 2 is selected, read from the edit controls
    if (selectedBlockStyle == 2)
    {
        GetWindowText(GetDlgItem(GetParent(hEditAscii), 10), buffer1, 8);
        GetWindowText(GetDlgItem(GetParent(hEditAscii), 11), buffer2, 8);
        GetWindowText(GetDlgItem(GetParent(hEditAscii), 12), buffer3, 8);
    }

    wchar_t ascii_blocks[4];
    switch (selectedBlockStyle)
    {
    case 1:                          // ⬛ ⬜ ▪ ▫
        ascii_blocks[0] = L'\x2B1B'; // ⬛
        ascii_blocks[1] = L'\x2B1C'; // ⬜
        ascii_blocks[2] = L'\x25AA'; // ▪
        ascii_blocks[3] = L'\x25AB'; // ▫
        break;
    case 2: // Custom ASCII input
        ascii_blocks[0] = buffer1[0];
        ascii_blocks[1] = buffer2[0];
        ascii_blocks[2] = buffer3[0];
        ascii_blocks[3] = L'.';
        break;
    case 3: // $ & * .
        ascii_blocks[0] = L'$';
        ascii_blocks[1] = L'&';
        ascii_blocks[2] = L'*';
        ascii_blocks[3] = L'.';
        break;
    case 0: // Default █ ▓ ▒ ░
    default:
        ascii_blocks[0] = L'\x2588'; // █
        ascii_blocks[1] = L'\x2593'; // ▓
        ascii_blocks[2] = L'\x2592'; // ▒
        ascii_blocks[3] = L'\x2591'; // ░
        break;
    }
    extern int threshold1, threshold2; // Bring in from global scope if needed

    std::wstring ascii_art;
    std::wstring numeric_values;

    for (int y = 0; y < height; y++)
    {
        int flipped_y = height - 1 - y;
        for (int x = 0; x < width; x++)
        {
            int pixel_index = flipped_y * row_padded + (x / 2);
            unsigned char pixel_value = imageData[pixel_index];

            if (x % 2 == 0)
            {
                pixel_value >>= 4;
            }
            else
            {
                pixel_value &= 0x0F;
            }

            wchar_t ascii_char;

            // Convert 4-bit value (0–15) to grayscale (0–255)
            int numeric_value;

            if (pixel_value == 0)
            {
                ascii_char = ascii_blocks[0];
                numeric_value = 64;
            }
            else if (pixel_value == 1)
            {
                ascii_char = ascii_blocks[1];
                numeric_value = threshold1;
            }
            else if (pixel_value == 2)
            {
                ascii_char = ascii_blocks[2];
                numeric_value = threshold2;
            }
            else
            {
                ascii_char = ascii_blocks[3];
                numeric_value = 255;
            }

            ascii_art += ascii_char;
            numeric_values += std::to_wstring(numeric_value) + L" ";
        }
        ascii_art += L"\r\n";
        numeric_values += L"\r\n";
    }

    SetWindowText(hEditAscii, ascii_art.c_str());
    SetWindowText(hEditNumeric, numeric_values.c_str());

    free(imageData);
}

// Reads BMP headers and validates the file
bool read_bmp_headers(const char *image_path, BMPHeader &bmpHeader, BMPInfoHeader &bmpInfoHeader, FILE *&file)
{
    file = fopen(image_path, "rb");
    if (!file)
        return false;

    fread(&bmpHeader, sizeof(BMPHeader), 1, file);
    fread(&bmpInfoHeader, sizeof(BMPInfoHeader), 1, file);

    // Debugging the BMP header
    std::cout << "BMP Type: " << bmpHeader.bfType << std::endl;
    std::cout << "BMP Size: " << bmpHeader.bfSize << std::endl;
    std::cout << "BMP Offset: " << bmpHeader.bfOffBits << std::endl;
    std::cout << "Width: " << bmpInfoHeader.biWidth << ", Height: " << bmpInfoHeader.biHeight << std::endl;
    std::cout << "BitCount: " << bmpInfoHeader.biBitCount << std::endl;

    return bmpHeader.bfType == 0x4D42; // Valid BMP check
}

// Reads image data from the BMP file
unsigned char *read_image_data(FILE *file, const BMPHeader &bmpHeader, int row_padded, int height)
{
    unsigned char *imageData = (unsigned char *)malloc(row_padded * height);
    if (!imageData)
        return nullptr;

    fseek(file, bmpHeader.bfOffBits, SEEK_SET);
    fread(imageData, 1, row_padded * height, file);
    return imageData;
}

// Processes a 4-bit BMP image and generates ASCII art
void process_4bit_image(HWND hEdit, int width, int height, int row_padded, unsigned char *imageData)
{
    wchar_t ascii_blocks[] = {L'\x2591', L'\x2592', L'\x2593', L'\x2588'}; // Unicode shading blocks
    int grayscale_values[] = {255, 192, 128, 64};                          // Grayscale levels for mapping
    wchar_t ascii_art[height][width + 1];                                  // Array to store ASCII characters
    int ascii_values[height][width];                                       // Array to store numeric grayscale values

    for (int y = 0; y < height; y++)
    {
        int flipped_y = height - 1 - y; // Flip vertically for BMP coordinate system
        for (int x = 0; x < width; x++)
        {
            int pixel_index = flipped_y * row_padded + (x / 2); // Pixel index in row
            unsigned char pixel_value = imageData[pixel_index];

            // Extract the 4-bit value (high nibble for even columns, low nibble for odd columns)
            if (x % 2 == 0)
            {
                pixel_value >>= 4; // High nibble
            }
            else
            {
                pixel_value &= 0x0F; // Low nibble
            }

            // Map 4-bit value to ASCII character and numeric grayscale value
            wchar_t ascii_char;
            int numeric_value;
            if (pixel_value == 0)
            {
                ascii_char = ascii_blocks[0]; // Dark gray
                numeric_value = grayscale_values[3];
            }
            else if (pixel_value == 1)
            {
                ascii_char = ascii_blocks[1]; // Medium gray
                numeric_value = grayscale_values[2];
            }
            else if (pixel_value == 2)
            {
                ascii_char = ascii_blocks[2]; // Light gray
                numeric_value = grayscale_values[1];
            }
            else
            {
                ascii_char = ascii_blocks[3]; // White
                numeric_value = grayscale_values[0];
            }

            // Store the ASCII character and numeric value
            ascii_art[y][x] = ascii_char;
            ascii_values[y][x] = numeric_value;
        }
        ascii_art[y][width] = L'\0'; // Null-terminate each row
    }

    // Combine ASCII art into a single string for display
    std::wstring result;
    for (int i = 0; i < height; i++)
    {
        result += ascii_art[i];
        result += L"\r\n";
    }

    // Display ASCII art in the Edit control
    SetWindowText(hEdit, result.c_str());

    // Debugging output: Print numeric grayscale values to console
    std::cout << "Numeric Grayscale Values:\n";
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            std::cout << ascii_values[y][x] << " ";
        }
        std::cout << "\n";
    }
}

// Sets up window components like font and edit control
void setup_window(HWND hwnd, HFONT &hFont, HWND &hEdit)
{
    hFont = CreateFont(
        10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN,
        L"System");

    hEdit = CreateWindowEx(
        WS_EX_CLIENTEDGE, L"EDIT", L"Output will appear here...",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        50, 50, 400, 400,
        hwnd, (HMENU)2, GetModuleHandle(NULL), NULL);

    SendMessage(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

    CreateWindow(
        L"BUTTON", L"Generate",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        50, 500, 200, 50,
        hwnd, (HMENU)1, GetModuleHandle(NULL), NULL);
}

// Entry point
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    AllocConsole();
    FILE *fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    freopen_s(&fp, "CONIN$", "r", stdin);

    const wchar_t CLASS_NAME[] = L"MyUniqueWindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    INITCOMMONCONTROLSEX icex = {sizeof(INITCOMMONCONTROLSEX), ICC_BAR_CLASSES};
    InitCommonControlsEx(&icex);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"ASCII Art Generator", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1200, 600,
        NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

void SetFilePathToEditControl(HWND hwndEdit, const std::string &filePath)
{
    // Convert std::string to LPWSTR (wide character string)
    int bufferSize = MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), -1, nullptr, 0);
    LPWSTR wideFilePath = new wchar_t[bufferSize];

    MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), -1, wideFilePath, bufferSize);

    // Now you can safely pass wideFilePath (LPWSTR) to SetWindowText
    SetWindowText(hwndEdit, wideFilePath);

    delete[] wideFilePath; // Clean up the allocated memory
}

std::string selectedFilePath = "";

// WindowProc function
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HWND hEditAscii, hEditNumeric, hBrowseButton, hEditPath, hComboBox, hEditSymbol1,
        hEditSymbol2, hEditSymbol3, hSlider1, hSlider2, hSliderLabel1, hSliderLabel2;
    static HBRUSH hBrushBackground;
    static COLORREF textColor = RGB(255, 255, 255); // Black text (inverted)
    static COLORREF backgroundColor = RGB(0, 0, 0); // White background (inverted)

    switch (uMsg)
    {
    case WM_CREATE:
    {
        // Create a solid brush for the background
        hBrushBackground = CreateSolidBrush(backgroundColor);

        HFONT hFont = CreateFont(
            14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN,
            L"system");
        HFONT Font_2 = CreateFont(
            11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN,
            L"consolas");

        // ASCII art text box
        hEditAscii = CreateWindowEx(
            WS_EX_CLIENTEDGE, L"EDIT", L"ASCII Art will appear here...",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | WS_VSCROLL | ES_READONLY,
            50, 50, 350, 400,
            hwnd, (HMENU)2, GetModuleHandle(NULL), NULL);

        // Numeric grayscale values text box
        hEditNumeric = CreateWindowEx(
            WS_EX_CLIENTEDGE, L"EDIT", L"Numeric values will appear here...",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | WS_VSCROLL | ES_READONLY,
            420, 50, 350, 400,
            hwnd, (HMENU)3, GetModuleHandle(NULL), NULL);

        // Path display text box (not editable)
        hEditPath = CreateWindowEx(
            WS_EX_CLIENTEDGE, L"EDIT", L"File path will appear here...",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY,
            50, 460, 600, 25,
            hwnd, (HMENU)4, GetModuleHandle(NULL), NULL);

        // Browse button to open file dialog
        hBrowseButton = CreateWindow(
            L"BUTTON", L"Browse",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            670, 460, 100, 25,
            hwnd, (HMENU)5, GetModuleHandle(NULL), NULL);

        SendMessage(hEditAscii, WM_SETFONT, (WPARAM)Font_2, TRUE);
        SendMessage(hEditNumeric, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(hEditPath, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Generate button
        CreateWindow(
            L"BUTTON", L"Generate",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            670, 500, 100, 50,
            hwnd, (HMENU)1, GetModuleHandle(NULL), NULL);

        hComboBox = CreateWindowEx(
            0, L"COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            790, 50, 200, 200, // x, y, width, height
            hwnd, (HMENU)6,    // Control ID
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL);

        SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"UNICODE");
        SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"Option 2");
        SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)L"Custom");

        // Optional: Set default selection
        SendMessage(hComboBox, CB_SETCURSEL, 0, 0); // Select first item

        // Edit boxes for custom ASCII input (used in Option 2)
        hEditSymbol1 = CreateWindowEx(
            WS_EX_CLIENTEDGE, L"EDIT", L"#", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            790, 90, 60, 25, hwnd, (HMENU)10, GetModuleHandle(NULL), NULL);

        hEditSymbol2 = CreateWindowEx(
            WS_EX_CLIENTEDGE, L"EDIT", L"@", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            860, 90, 60, 25, hwnd, (HMENU)11, GetModuleHandle(NULL), NULL);

        hEditSymbol3 = CreateWindowEx(
            WS_EX_CLIENTEDGE, L"EDIT", L"%", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            930, 90, 60, 25, hwnd, (HMENU)12, GetModuleHandle(NULL), NULL);

        EnableWindow(hEditSymbol1, FALSE);
        EnableWindow(hEditSymbol2, FALSE);
        EnableWindow(hEditSymbol3, FALSE);

        // Label and slider 1
        hSliderLabel1 = CreateWindow(
            L"STATIC", L"Threshold 1:", WS_CHILD | WS_VISIBLE,
            790, 140, 100, 20,
            hwnd, NULL, GetModuleHandle(NULL), NULL);

        hSlider1 = CreateWindowEx(
            0, TRACKBAR_CLASS, L"",
            WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
            790, 160, 200, 30,
            hwnd, (HMENU)20, GetModuleHandle(NULL), NULL);

        SendMessage(hSlider1, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
        SendMessage(hSlider1, TBM_SETPOS, TRUE, threshold1);

        // Label and slider 2
        hSliderLabel2 = CreateWindow(
            L"STATIC", L"Threshold 2:", WS_CHILD | WS_VISIBLE,
            790, 200, 100, 20,
            hwnd, NULL, GetModuleHandle(NULL), NULL);

        hSlider2 = CreateWindowEx(
            0, TRACKBAR_CLASS, L"",
            WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
            790, 220, 200, 30,
            hwnd, (HMENU)21, GetModuleHandle(NULL), NULL);

        SendMessage(hSlider2, TBM_SETRANGE, TRUE, MAKELPARAM(0, 255));
        SendMessage(hSlider2, TBM_SETPOS, TRUE, threshold2);

        // Value display for Threshold 1
        CreateWindow(
            L"STATIC", L"192", WS_CHILD | WS_VISIBLE,
            1000, 160, 40, 20, // X/Y next to slider 1
            hwnd, (HMENU)30, GetModuleHandle(NULL), NULL);

        // Value display for Threshold 2
        CreateWindow(
            L"STATIC", L"128", WS_CHILD | WS_VISIBLE,
            1000, 220, 40, 20, // X/Y next to slider 2
            hwnd, (HMENU)31, GetModuleHandle(NULL), NULL);

        return 0;
    }

    case WM_CTLCOLOREDIT:
    {
        HDC hdcEdit = (HDC)wParam;
        SetTextColor(hdcEdit, textColor);     // Inverted text color (black)
        SetBkColor(hdcEdit, backgroundColor); // Inverted background color (white)
        return (LRESULT)hBrushBackground;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == 5)
        { // "Browse" button clicked
            // Open the file dialog to select an image file
            selectedFilePath = OpenFileDialog(hwnd); // Store the file path globally

            // If a file was selected, update the path textbox
            if (!selectedFilePath.empty())
            {
                SetFilePathToEditControl(hEditPath, selectedFilePath);
            }
            else
            {
                SetWindowText(hEditPath, L"File not selected.");
            }
        }

        if (LOWORD(wParam) == 1)
        { // "Generate ASCII Art" button clicked
            // If a valid file path is provided
            if (!selectedFilePath.empty())
            {
                // Generate ASCII art from the selected file
                generate_ascii_art(hEditAscii, hEditNumeric, selectedFilePath.c_str());
            }
            else
            {
                MessageBox(hwnd, L"Please select a valid file first.", L"Error", MB_OK | MB_ICONERROR);
            }
        }

        if (LOWORD(wParam) == 6 && HIWORD(wParam) == CBN_SELCHANGE)
        {
            HWND hCombo = (HWND)lParam;
            selectedBlockStyle = (int)SendMessage(hCombo, CB_GETCURSEL, 0, 0);

            // Enable or disable text fields based on selection
            BOOL enableCustom = (selectedBlockStyle == 2); // Option 2
            EnableWindow(hEditSymbol1, enableCustom);
            EnableWindow(hEditSymbol2, enableCustom);
            EnableWindow(hEditSymbol3, enableCustom);
        }

        return 0;
    case WM_HSCROLL:
    {
        if ((HWND)lParam == hSlider1)
        {
            threshold1 = SendMessage(hSlider1, TBM_GETPOS, 0, 0);
        }
        else if ((HWND)lParam == hSlider2)
        {
            threshold2 = SendMessage(hSlider2, TBM_GETPOS, 0, 0);
        }
        if ((HWND)lParam == hSlider1)
        {
            threshold1 = SendMessage(hSlider1, TBM_GETPOS, 0, 0);

            wchar_t valueStr[8];
            swprintf_s(valueStr, L"%d", threshold1);
            SetWindowText(GetDlgItem(hwnd, 30), valueStr);
        }
        else if ((HWND)lParam == hSlider2)
        {
            threshold2 = SendMessage(hSlider2, TBM_GETPOS, 0, 0);

            wchar_t valueStr[8];
            swprintf_s(valueStr, L"%d", threshold2);
            SetWindowText(GetDlgItem(hwnd, 31), valueStr);
        }

        return 0;
    }

    case WM_DESTROY:
        DeleteObject(hBrushBackground);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}