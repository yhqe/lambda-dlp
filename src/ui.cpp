#include "ui.h"
#include <cmath>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <windows.h>
#include <wininet.h>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "ext/imgui/imgui_freetype.h"

#pragma comment(lib, "wininet.lib")

ID3D11Device* ui::pd3dDevice = nullptr;
ID3D11DeviceContext* ui::pd3dDeviceContext = nullptr;
IDXGISwapChain* ui::pSwapChain = nullptr;
ID3D11RenderTargetView* ui::pMainRenderTargetView = nullptr;

HMODULE ui::hCurrentModule = nullptr;

LPCSTR ui::lpWindowName = "lambda";
ImVec2 ui::vWindowSize = { 550, 400 };
ImGuiWindowFlags ui::windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar;
bool ui::bDraw = true;

void ui::active()
{
    bDraw = true;
}

bool ui::isActive()
{
    return bDraw == true;
}

bool ui::CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const UINT createDeviceFlags = 0;
    
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &pSwapChain, &pd3dDevice, &featureLevel, &pd3dDeviceContext) != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void ui::CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer != nullptr)
    {
        pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &pMainRenderTargetView);
        pBackBuffer->Release();
    }
}

void ui::CleanupRenderTarget()
{
    if (pMainRenderTargetView)
    {
        pMainRenderTargetView->Release();
        pMainRenderTargetView = nullptr;
    }
}

void ui::CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (pSwapChain)
    {
        pSwapChain->Release();
        pSwapChain = nullptr;
    }

    if (pd3dDeviceContext)
    {
        pd3dDeviceContext->Release();
        pd3dDeviceContext = nullptr;
    }

    if (pd3dDevice)
    {
        pd3dDevice->Release();
        pd3dDevice = nullptr;
    }
}

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0 // From Windows SDK 8.1+ headers
#endif

LRESULT WINAPI ui::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (pd3dDevice != nullptr && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;

    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;

    case WM_DPICHANGED:
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports)
        {
            const RECT* suggested_rect = (RECT*)lParam;
            ::SetWindowPos(hWnd, nullptr, suggested_rect->left, suggested_rect->top, suggested_rect->right - suggested_rect->left, suggested_rect->bottom - suggested_rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        break;

    default:
        break;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

constexpr auto to_rgba = [](uint32_t argb) constexpr
{
    ImVec4 color{};
    color.x = ((argb >> 16) & 0xFF) / 255.0f;
    color.y = ((argb >> 8) & 0xFF) / 255.0f;
    color.z = (argb & 0xFF) / 255.0f;
    color.w = ((argb >> 24) & 0xFF) / 255.0f;
    return color;
};

constexpr auto lerp = [](const ImVec4& a, const ImVec4& b, float t) constexpr
{
    return ImVec4{
        std::lerp(a.x, b.y, t),
        std::lerp(a.y, b.y, t),
        std::lerp(a.z, b.z, t),
        std::lerp(a.w, b.w, t)
    };
};

void set_colors(ImGuiStyle style) {
    auto colors = style.Colors;
    colors[ImGuiCol_Text] = to_rgba(0xFFABB2BF);
    colors[ImGuiCol_TextDisabled] = to_rgba(0xFF565656);
    colors[ImGuiCol_WindowBg] = to_rgba(0xFF282C34);
    colors[ImGuiCol_ChildBg] = to_rgba(0xFF21252B);
    colors[ImGuiCol_PopupBg] = to_rgba(0xFF2E323A);
    colors[ImGuiCol_Border] = to_rgba(0xFF2E323A);
    colors[ImGuiCol_BorderShadow] = to_rgba(0x00000000);
    colors[ImGuiCol_FrameBg] = colors[ImGuiCol_ChildBg];
    colors[ImGuiCol_FrameBgHovered] = to_rgba(0xFF484C52);
    colors[ImGuiCol_FrameBgActive] = to_rgba(0xFF54575D);
    colors[ImGuiCol_TitleBg] = colors[ImGuiCol_WindowBg];
    colors[ImGuiCol_TitleBgActive] = colors[ImGuiCol_FrameBgActive];
    colors[ImGuiCol_TitleBgCollapsed] = to_rgba(0x8221252B);
    colors[ImGuiCol_MenuBarBg] = colors[ImGuiCol_ChildBg];
    colors[ImGuiCol_ScrollbarBg] = colors[ImGuiCol_PopupBg];
    colors[ImGuiCol_ScrollbarGrab] = to_rgba(0xFF3E4249);
    colors[ImGuiCol_ScrollbarGrabHovered] = to_rgba(0xFF484C52);
    colors[ImGuiCol_ScrollbarGrabActive] = to_rgba(0xFF54575D);
    colors[ImGuiCol_CheckMark] = colors[ImGuiCol_Text];
    colors[ImGuiCol_SliderGrab] = to_rgba(0xFF353941);
    colors[ImGuiCol_SliderGrabActive] = to_rgba(0xFF7A7A7A);
    colors[ImGuiCol_Button] = colors[ImGuiCol_SliderGrab];
    colors[ImGuiCol_ButtonHovered] = colors[ImGuiCol_FrameBgActive];
    colors[ImGuiCol_ButtonActive] = colors[ImGuiCol_ScrollbarGrabActive];
    colors[ImGuiCol_Header] = colors[ImGuiCol_ChildBg];
    colors[ImGuiCol_HeaderHovered] = to_rgba(0xFF353941);
    colors[ImGuiCol_HeaderActive] = colors[ImGuiCol_FrameBgActive];
    colors[ImGuiCol_Separator] = colors[ImGuiCol_FrameBgActive];
    colors[ImGuiCol_SeparatorHovered] = to_rgba(0xFF3E4452);
    colors[ImGuiCol_SeparatorActive] = colors[ImGuiCol_SeparatorHovered];
    colors[ImGuiCol_ResizeGrip] = colors[ImGuiCol_Separator];
    colors[ImGuiCol_ResizeGripHovered] = colors[ImGuiCol_SeparatorHovered];
    colors[ImGuiCol_ResizeGripActive] = colors[ImGuiCol_SeparatorActive];
    colors[ImGuiCol_TabHovered] = colors[ImGuiCol_HeaderHovered];
    colors[ImGuiCol_Tab] = colors[ImGuiCol_FrameBgActive];
    colors[ImGuiCol_DockingPreview] = colors[ImGuiCol_ChildBg];
    colors[ImGuiCol_DockingEmptyBg] = colors[ImGuiCol_WindowBg];
    colors[ImGuiCol_PlotLines] = ImVec4{ 0.61f, 0.61f, 0.61f, 1.00f };
    colors[ImGuiCol_PlotLinesHovered] = ImVec4{ 1.00f, 0.43f, 0.35f, 1.00f };
    colors[ImGuiCol_PlotHistogram] = ImVec4{ 0.90f, 0.70f, 0.00f, 1.00f };
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4{ 1.00f, 0.60f, 0.00f, 1.00f };
    colors[ImGuiCol_TableHeaderBg] = colors[ImGuiCol_ChildBg];
    colors[ImGuiCol_TableBorderStrong] = colors[ImGuiCol_SliderGrab];
    colors[ImGuiCol_TableBorderLight] = colors[ImGuiCol_FrameBgActive];
    colors[ImGuiCol_TableRowBg] = ImVec4{ 0.00f, 0.00f, 0.00f, 0.00f };
    colors[ImGuiCol_TableRowBgAlt] = ImVec4{ 1.00f, 1.00f, 1.00f, 0.06f };
    colors[ImGuiCol_TextSelectedBg] = to_rgba(0xFF243140);
    colors[ImGuiCol_DragDropTarget] = colors[ImGuiCol_Text];
    colors[ImGuiCol_NavWindowingHighlight] = colors[ImGuiCol_Text];
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4{ 0.80f, 0.80f, 0.80f, 0.20f };
    colors[ImGuiCol_ModalWindowDimBg] = to_rgba(0xC821252B);
}

auto get_cwd() {
    try {
        // get current working directory
        std::filesystem::path currentPath = std::filesystem::current_path();
        return currentPath;
    }
    catch (std::filesystem::filesystem_error const& ex) {
        MessageBoxA(NULL, "error!", ex.what(), MB_OK);
    }
}

static const char* mode_labels[] = { "video", "audio only", "video (muted)" };
static const char* audio_format_labels[] = { "best", "mp3", "m4a", "opus", "flac", "wav" };
static const char* video_format_labels[] = { "best", "mp4", "mkv", "webm" };
static const char* audio_quality_labels[] = { "best (0)", "high (2)", "medium (5)", "low (8)", "worst (10)" };
static const int audio_quality_values[] = { 0, 2, 5, 8, 10 };

struct app_settings {
    char download_dir[256] = "downloads";
    int mode = 0;                                               // 0=video, 1=audio, 2=video no audio
    int audio_format = 0;                                       // index into audio_format_labels
    int video_format = 0;                                       // index into video_format_labels
    int audio_quality = 0;                                      // index into audio_quality_labels
    bool embed_metadata = true;
    bool embed_thumbnail = false;
    bool embed_subtitles = false;
    bool sponsorblock = false;
    bool restrict_filenames = false;
    char ytdlp_path[256] = ".\\build\\debug\\bin\\yt-dlp.exe";
    char output_template[256] = "%(title)s.%(ext)s";
    char extra_args[512] = "";
};

static app_settings g_settings;
static const char* settings_file = "settings.json";

void save_settings() {
    nlohmann::json j;
    j["download_dir"] = g_settings.download_dir;
    j["mode"] = g_settings.mode;
    j["audio_format"] = g_settings.audio_format;
    j["video_format"] = g_settings.video_format;
    j["audio_quality"] = g_settings.audio_quality;
    j["embed_metadata"] = g_settings.embed_metadata;
    j["embed_thumbnail"] = g_settings.embed_thumbnail;
    j["embed_subtitles"] = g_settings.embed_subtitles;
    j["sponsorblock"] = g_settings.sponsorblock;
    j["restrict_filenames"] = g_settings.restrict_filenames;
    j["ytdlp_path"] = g_settings.ytdlp_path;
    j["output_template"] = g_settings.output_template;
    j["extra_args"] = g_settings.extra_args;
    std::ofstream f(settings_file);
    if (f.is_open()) f << j.dump(4);
}

void load_settings() {
    std::ifstream f(settings_file);
    if (!f.is_open()) return;
    try {
        nlohmann::json j = nlohmann::json::parse(f);
        if (j.contains("download_dir")) strncpy_s(g_settings.download_dir, j["download_dir"].get<std::string>().c_str(), sizeof(g_settings.download_dir) - 1);
        if (j.contains("mode")) g_settings.mode = j["mode"].get<int>();
        if (j.contains("audio_format")) g_settings.audio_format = j["audio_format"].get<int>();
        if (j.contains("video_format")) g_settings.video_format = j["video_format"].get<int>();
        if (j.contains("audio_quality")) g_settings.audio_quality = j["audio_quality"].get<int>();
        if (j.contains("embed_metadata")) g_settings.embed_metadata = j["embed_metadata"].get<bool>();
        if (j.contains("embed_thumbnail")) g_settings.embed_thumbnail = j["embed_thumbnail"].get<bool>();
        if (j.contains("embed_subtitles")) g_settings.embed_subtitles = j["embed_subtitles"].get<bool>();
        if (j.contains("sponsorblock")) g_settings.sponsorblock = j["sponsorblock"].get<bool>();
        if (j.contains("restrict_filenames")) g_settings.restrict_filenames = j["restrict_filenames"].get<bool>();
        if (j.contains("ytdlp_path")) strncpy_s(g_settings.ytdlp_path, j["ytdlp_path"].get<std::string>().c_str(), sizeof(g_settings.ytdlp_path) - 1);
        if (j.contains("output_template")) strncpy_s(g_settings.output_template, j["output_template"].get<std::string>().c_str(), sizeof(g_settings.output_template) - 1);
        if (j.contains("extra_args")) strncpy_s(g_settings.extra_args, j["extra_args"].get<std::string>().c_str(), sizeof(g_settings.extra_args) - 1);
    } catch (...) { }
}

struct task {
    std::string url;
    float progress = 0.0f;
    bool finished = false;
};

std::vector<task*> download_queue;
std::mutex queue_mutex;
static float queue_flash_time = -1.0f;

char url_buf[512] = "";

static float download_progress = 0.0f;

std::string build_ytdlp_command(const std::string& url) {
	std::string cmd = std::string(g_settings.ytdlp_path) + " --newline --progress-template \"%(progress._percent_str)s\"";

	// output template
	cmd += " -o \"" + std::string(g_settings.download_dir) + "/" + std::string(g_settings.output_template) + "\"";

	// mode: 0=video, 1=audio, 2=video no audio
	if (g_settings.mode == 1) {
		cmd += " -x";
		if (g_settings.audio_format > 0)
			cmd += std::string(" --audio-format ") + audio_format_labels[g_settings.audio_format];
		cmd += std::string(" --audio-quality ") + std::to_string(audio_quality_values[g_settings.audio_quality]);
	} else if (g_settings.mode == 2) {
		cmd += " -f \"bv\"";
	} else {
		if (g_settings.video_format > 0)
			cmd += std::string(" --merge-output-format ") + video_format_labels[g_settings.video_format];
	}

	if (g_settings.embed_metadata)    cmd += " --embed-metadata";
	if (g_settings.embed_thumbnail)   cmd += " --embed-thumbnail";
	if (g_settings.embed_subtitles)   cmd += " --embed-subs";
	if (g_settings.sponsorblock)      cmd += " --sponsorblock-remove all";
	if (g_settings.restrict_filenames) cmd += " --restrict-filenames";

	// extra user args
	std::string extra(g_settings.extra_args);
	if (!extra.empty()) cmd += " " + extra;

	cmd += " \"" + url + "\"";
	return cmd;
}

void download_worker(task* t) {
	// create temporary progress file to output to progress bar when rendering
	std::string temp_file = "progress_" + std::to_string((uintptr_t)t) + ".tmp";

	// build command from current settings
	std::string command = build_ytdlp_command(t->url);
    std::vector<char> cmd_buffer(command.begin(), command.end());
    cmd_buffer.push_back('\0');

    // redirect stdout/stderr to temp file instead of shell ">" because it's more efficient
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE hFile = CreateFileA(temp_file.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        t->progress = 1.0f;
        t->finished = true;
        return;
    }

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hFile;
    si.hStdError = hFile;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi = {};

    if (CreateProcessA(NULL, cmd_buffer.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        while (WaitForSingleObject(pi.hProcess, 200) == WAIT_TIMEOUT) {
            std::ifstream file(temp_file);
            if (file.is_open()) {
                std::string line, lastLine;
                while (std::getline(file, line)) {
                    if (!line.empty()) lastLine = line;
                }

                try {
                    lastLine.erase(std::remove(lastLine.begin(), lastLine.end(), ' '), lastLine.end());
                    if (!lastLine.empty()) {
                        t->progress = std::stof(lastLine) / 100.0f;
                    }
                }
                catch (...) {  }
            }
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    CloseHandle(hFile);
    t->progress = 1.0f;
    t->finished = true;
    std::remove(temp_file.c_str());
}

void render_queue() {
    // loop through the download queue to check for every download started
    std::lock_guard<std::mutex> lock(queue_mutex);
    for (int i = 0; i < download_queue.size(); i++) {
        ImGui::Text("file: %s", download_queue[i]->url.c_str());
        ImGui::ProgressBar(download_queue[i]->progress, ImVec2(-1.0f, 0.0f));

        // remove queue item after job is finished
        if (download_queue[i]->finished) {
            delete download_queue[i];
            download_queue.erase(download_queue.begin() + i);
            i--;
        }
    }

    // display text if it is empty
    if (download_queue.empty()) {
        ImGui::TextDisabled("queue is empty! start some downloads and they will appear here.");
    }

    return;
}

void ui::render()
{
    load_settings();
    ImGui_ImplWin32_EnableDpiAwareness();
    const WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, _T("userman"), nullptr };
    ::RegisterClassEx(&wc);
    const HWND hwnd = ::CreateWindow(wc.lpszClassName, _T("userman"), WS_OVERLAPPEDWINDOW, 100, 100, 50, 50, NULL, NULL, wc.hInstance, NULL);

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return;
    }

    ::ShowWindow(hwnd, SW_HIDE);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGuiStyle& style = ImGui::GetStyle();
    {
        auto colors = style.Colors;
        colors[ImGuiCol_Text] = to_rgba(0xFFABB2BF);
        colors[ImGuiCol_TextDisabled] = to_rgba(0xFF565656);
        colors[ImGuiCol_WindowBg] = to_rgba(0xFF282C34);
        colors[ImGuiCol_ChildBg] = to_rgba(0xFF21252B);
        colors[ImGuiCol_PopupBg] = to_rgba(0xFF2E323A);
        colors[ImGuiCol_Border] = to_rgba(0xFF2E323A);
        colors[ImGuiCol_BorderShadow] = to_rgba(0x00000000);
        colors[ImGuiCol_FrameBg] = colors[ImGuiCol_ChildBg];
        colors[ImGuiCol_FrameBgHovered] = to_rgba(0xFF484C52);
        colors[ImGuiCol_FrameBgActive] = to_rgba(0xFF54575D);
        colors[ImGuiCol_TitleBg] = colors[ImGuiCol_PopupBg];
        colors[ImGuiCol_TitleBgActive] = colors[ImGuiCol_WindowBg];
        colors[ImGuiCol_TitleBgCollapsed] = to_rgba(0x8221252B);
        colors[ImGuiCol_MenuBarBg] = colors[ImGuiCol_ChildBg];
        colors[ImGuiCol_ScrollbarBg] = colors[ImGuiCol_PopupBg];
        colors[ImGuiCol_ScrollbarGrab] = to_rgba(0xFF3E4249);
        colors[ImGuiCol_ScrollbarGrabHovered] = to_rgba(0xFF484C52);
        colors[ImGuiCol_ScrollbarGrabActive] = to_rgba(0xFF54575D);
        colors[ImGuiCol_CheckMark] = colors[ImGuiCol_Text];
        colors[ImGuiCol_SliderGrab] = to_rgba(0xFF353941);
        colors[ImGuiCol_SliderGrabActive] = to_rgba(0xFF7A7A7A);
        colors[ImGuiCol_Button] = colors[ImGuiCol_SliderGrab];
        colors[ImGuiCol_ButtonHovered] = colors[ImGuiCol_FrameBgActive];
        colors[ImGuiCol_ButtonActive] = colors[ImGuiCol_ScrollbarGrabActive];
        colors[ImGuiCol_Header] = colors[ImGuiCol_ChildBg];
        colors[ImGuiCol_HeaderHovered] = to_rgba(0xFF353941);
        colors[ImGuiCol_HeaderActive] = colors[ImGuiCol_FrameBgActive];
        colors[ImGuiCol_Separator] = colors[ImGuiCol_FrameBgActive];
        colors[ImGuiCol_SeparatorHovered] = to_rgba(0xFF3E4452);
        colors[ImGuiCol_SeparatorActive] = colors[ImGuiCol_SeparatorHovered];
        colors[ImGuiCol_ResizeGrip] = colors[ImGuiCol_Separator];
        colors[ImGuiCol_ResizeGripHovered] = colors[ImGuiCol_SeparatorHovered];
        colors[ImGuiCol_ResizeGripActive] = colors[ImGuiCol_SeparatorActive];
        colors[ImGuiCol_TabHovered] = colors[ImGuiCol_FrameBgActive];
        colors[ImGuiCol_Tab] = colors[ImGuiCol_HeaderHovered];
        colors[ImGuiCol_TabActive] = colors[ImGuiCol_FrameBgActive];
        colors[ImGuiCol_DockingPreview] = colors[ImGuiCol_ChildBg];
        colors[ImGuiCol_DockingEmptyBg] = colors[ImGuiCol_WindowBg];
        colors[ImGuiCol_PlotLines] = ImVec4{ 0.61f, 0.61f, 0.61f, 1.00f };
        colors[ImGuiCol_PlotLinesHovered] = ImVec4{ 1.00f, 0.43f, 0.35f, 1.00f };
        colors[ImGuiCol_PlotHistogram] = ImVec4{ 0.502f, 0.565f, 0.714f, 1.00f };
        colors[ImGuiCol_PlotHistogramHovered] = ImVec4{ 1.00f, 0.60f, 0.00f, 1.00f };
        colors[ImGuiCol_TableHeaderBg] = colors[ImGuiCol_ChildBg];
        colors[ImGuiCol_TableBorderStrong] = colors[ImGuiCol_SliderGrab];
        colors[ImGuiCol_TableBorderLight] = colors[ImGuiCol_FrameBgActive];
        colors[ImGuiCol_TableRowBg] = ImVec4{ 0.00f, 0.00f, 0.00f, 0.00f };
        colors[ImGuiCol_TableRowBgAlt] = ImVec4{ 1.00f, 1.00f, 1.00f, 0.06f };
        colors[ImGuiCol_TextSelectedBg] = to_rgba(0xFF243140);
        colors[ImGuiCol_DragDropTarget] = colors[ImGuiCol_Text];
        colors[ImGuiCol_NavWindowingHighlight] = colors[ImGuiCol_Text];
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4{ 0.80f, 0.80f, 0.80f, 0.20f };
        colors[ImGuiCol_ModalWindowDimBg] = to_rgba(0xC821252B);
    }

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowBorderSize = 3.0f;

        // Rounding
        style.FrameRounding = 1.0f;
        style.PopupRounding = 1.0f;
        style.ScrollbarRounding = 1.0f;
        style.GrabRounding = 1.0f;
    }

    ImFontConfig cfg;
    cfg.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_Monochrome | ImGuiFreeTypeBuilderFlags_MonoHinting;
    cfg.PixelSnapH = false;
    cfg.SizePixels = 11.0f;
    cfg.RasterizerMultiply = 1.0f;

    char windows_directory[MAX_PATH];
    GetWindowsDirectoryA(windows_directory, MAX_PATH);

    std::string tahoma_bold_font_directory = (std::string)windows_directory + ("\\Fonts\\tahomabd.ttf");
    std::string tahoma_font_directory = (std::string)windows_directory + ("\\Fonts\\tahoma.ttf");
    std::string icons_font_directory = (std::string)windows_directory + ("\\Fonts\\tahoma.ttf");

    ImVector<ImWchar> ranges;
    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
    builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic());
    builder.BuildRanges(&ranges);

    ImFont* tahoma = io.Fonts->AddFontFromFileTTF(tahoma_font_directory.c_str(), 11.0f, &cfg, ranges.Data);
    ImFont* tahoma_bold = io.Fonts->AddFontFromFileTTF(tahoma_bold_font_directory.c_str(), 11.0f, &cfg, ranges.Data);

    // ImGui::GetIO().IniFilename = nullptr;

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(pd3dDevice, pd3dDeviceContext);

    const ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    bool bDone = false;
    bool downloading = false;

    while (!bDone)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                bDone = true;
        }

        //if (GetAsyncKeyState(VK_END) & 1)
        //    bDone = true;

        if (bDone)
            break;

        // use imgui to create a window
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        {
            if (isActive())
            {
                ImGui::SetNextWindowSize(vWindowSize, ImGuiCond_Once);
                ImGui::SetNextWindowBgAlpha(1.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowTitleAlign, ImVec2(0.5, 0.5));
                ImGui::Begin(lpWindowName, &bDraw, windowFlags);
                {
                    if (ImGui::BeginTabBar("##main_tabs", 0))
                    {
                        if (ImGui::BeginTabItem("downloader"))
                        {
                            // debug text
                            std::string cwd = get_cwd().string();
                            ImGui::Text(cwd.c_str());

                            ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() / 2.f - 100.f, ImGui::GetWindowHeight() / 2.f - 10.f));
                            ImGui::InputTextWithHint("##url", "enter url here", url_buf, IM_ARRAYSIZE(url_buf));
                            ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() / 2.f - 100.f, ImGui::GetWindowHeight() / 2.f + 10.f));

                            if (ImGui::Button("download", ImVec2(200, 17)))
                            {
                                // convert url_buf into a string for ease of use
                                std::string url = std::string(url_buf);

                                // check if a url is entered
                                if (!url_buf)
                                    MessageBoxA(NULL, "lambda", "no url provided!", MB_OK);
                                
                                // check if url has "&" character (messes up the downloader command)
                                if (url.find("&") != std::string::npos)
                                    MessageBoxA(NULL, "lambda", "invalid url! (contains '&')", MB_OK);

                                // run command through cmd
                                downloading = true;

                                // add download to queue & start downloading with a new thread
                                std::lock_guard<std::mutex> lock(queue_mutex);

                                // create task object
                                task* new_task = new task();
                                new_task->url = (std::string)url_buf;
                                new_task->progress = 0.0f;
                                new_task->finished = false;

								// add to queue vector and start download job in a new thread
                                download_queue.push_back(new_task);
                                std::thread(download_worker, new_task).detach();

                                queue_flash_time = (float)ImGui::GetTime();
                                downloading = false;
                            }
                            ImGui::EndTabItem();
                        }
                        {
                            int flash_color_count = 0;
                            if (queue_flash_time >= 0.0f) {
                                float elapsed = (float)ImGui::GetTime() - queue_flash_time;
                                float alpha = std::exp(-3.0f * elapsed);
                                if (alpha > 0.01f) {
                                    ImVec4 yellow = ImVec4(0.6f, 0.5f, 0.1f, 1.0f);
                                    ImVec4 normal_tab = ImGui::GetStyle().Colors[ImGuiCol_Tab];
                                    ImVec4 normal_active = ImGui::GetStyle().Colors[ImGuiCol_TabActive];
                                    ImVec4 normal_hovered = ImGui::GetStyle().Colors[ImGuiCol_TabHovered];
                                    auto blend = [](const ImVec4& base, const ImVec4& target, float t) {
                                        return ImVec4(
                                            base.x + (target.x - base.x) * t,
                                            base.y + (target.y - base.y) * t,
                                            base.z + (target.z - base.z) * t,
                                            base.w
                                        );
                                    };
                                    ImGui::PushStyleColor(ImGuiCol_Tab, blend(normal_tab, yellow, alpha));
                                    ImGui::PushStyleColor(ImGuiCol_TabActive, blend(normal_active, yellow, alpha));
                                    ImGui::PushStyleColor(ImGuiCol_TabHovered, blend(normal_hovered, yellow, alpha));
                                    flash_color_count = 3;
                                } else {
                                    queue_flash_time = -1.0f;
                                }
                            }
                            if (ImGui::BeginTabItem("queue"))
                            {
                                render_queue();
                                ImGui::EndTabItem();
                            }
                            if (flash_color_count > 0)
                                ImGui::PopStyleColor(flash_color_count);
                        }

                        static std::vector<std::string> log_buffer;
                        static float last_update_time = 0.0f;

                        // load file into ram to prevent ui stutters (yikes)
                        if (ImGui::GetTime() - last_update_time > 0.5f) {
                            log_buffer.clear();
                            std::ifstream logFile("log.txt");
                            std::string line;
                            while (std::getline(logFile, line)) {
                                log_buffer.push_back(line);
                            }
                            last_update_time = (float)ImGui::GetTime();
                        }

                        if (ImGui::BeginTabItem("log")) {
                            ImGui::BeginChild("##log_holder", ImVec2(0, 0), true);
                            for (const auto& log_line : log_buffer) {
                                ImGui::TextUnformatted(log_line.c_str());
                            }
                            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                                ImGui::SetScrollHereY(1.0f);
                            ImGui::EndChild();
                            ImGui::EndTabItem();
                        }

                        if (ImGui::BeginTabItem("settings"))
                        {
                            ImGui::BeginChild("##settings_scroll", ImVec2(0, 0), false);

                            ImGui::Separator(); ImGui::TextDisabled("general");

                            ImGui::Text("yt-dlp path");
                            ImGui::SetNextItemWidth(-1);
                            ImGui::InputText("##ytdlp_path", g_settings.ytdlp_path, sizeof(g_settings.ytdlp_path));

                            ImGui::Text("download directory");
                            ImGui::SetNextItemWidth(-1);
                            ImGui::InputText("##download_dir", g_settings.download_dir, sizeof(g_settings.download_dir));

                            ImGui::Text("output filename template");
                            ImGui::SetNextItemWidth(-1);
                            ImGui::InputText("##output_tpl", g_settings.output_template, sizeof(g_settings.output_template));

                            ImGui::Separator(); ImGui::TextDisabled("format");

                            ImGui::Text("download mode");
                            ImGui::SetNextItemWidth(-1);
                            ImGui::Combo("##mode", &g_settings.mode, mode_labels, IM_ARRAYSIZE(mode_labels));

                            if (g_settings.mode == 1) {
                                ImGui::Text("audio format");
                                ImGui::SetNextItemWidth(-1);
                                ImGui::Combo("##audio_fmt", &g_settings.audio_format, audio_format_labels, IM_ARRAYSIZE(audio_format_labels));

                                ImGui::Text("audio quality");
                                ImGui::SetNextItemWidth(-1);
                                ImGui::Combo("##audio_quality", &g_settings.audio_quality, audio_quality_labels, IM_ARRAYSIZE(audio_quality_labels));
                            } else if (g_settings.mode == 0) {
                                ImGui::Text("video format");
                                ImGui::SetNextItemWidth(-1);
                                ImGui::Combo("##video_fmt", &g_settings.video_format, video_format_labels, IM_ARRAYSIZE(video_format_labels));
                            }

                            ImGui::Separator(); ImGui::TextDisabled("post-processing");

                            ImGui::Checkbox("embed metadata", &g_settings.embed_metadata);
                            ImGui::Checkbox("embed thumbnail", &g_settings.embed_thumbnail);
                            ImGui::Checkbox("embed subtitles", &g_settings.embed_subtitles);
                            ImGui::Checkbox("remove sponsor segments (sponsorblock)", &g_settings.sponsorblock);
                            ImGui::Checkbox("restrict filenames (ascii only)", &g_settings.restrict_filenames);

                            ImGui::Separator(); ImGui::TextDisabled("advanced");

                            ImGui::Text("extra yt-dlp arguments");
                            ImGui::SetNextItemWidth(-1);
                            ImGui::InputText("##extra_args", g_settings.extra_args, sizeof(g_settings.extra_args));

                            ImGui::Spacing();
                            if (ImGui::Button("save settings", ImVec2(-1, 0))) {
                                save_settings();
                            }

                            ImGui::Spacing();
                            if (ImGui::Button("reset to defaults", ImVec2(-1, 0))) {
                                g_settings = app_settings();
                                save_settings();
                            }

                            ImGui::EndChild();
                            ImGui::EndTabItem();
                        }
                        ImGui::EndTabBar();
                    }
                }
                ImGui::End();
                ImGui::PopStyleVar();
            }

            // ImGui::ShowDemoWindow();
            // ImGui::ShowMetricsWindow();
            // ImGui::ShowStackToolWindow();
        }
        ImGui::EndFrame();

        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        pd3dDeviceContext->OMSetRenderTargets(1, &pMainRenderTargetView, nullptr);
        pd3dDeviceContext->ClearRenderTargetView(pMainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        pSwapChain->Present(0, 0);

        #ifndef _WINDLL
            if (!ui::isActive())
                break;
        #endif
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);

    #ifdef _WINDLL
    CreateThread(nullptr, NULL, (LPTHREAD_START_ROUTINE)FreeLibrary, hCurrentModule, NULL, nullptr);
    #endif
}