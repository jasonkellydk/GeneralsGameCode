/*
** Command & Conquer Generals Zero Hour(tm)
** SDL3 platform window boundary for GeneralsMD.
*/

#include "PreRTS.h"

#include "Platform/SDLPlatformWindow.h"

#include "GameClient/ClientInstance.h"
#include "Common/StackDump.h"

#ifdef RTS_ENABLE_CRASHDUMP
#include "Common/MiniDumper.h"
#endif

#include <filesystem>
#include <system_error>

extern CComModule _Module;

HWND ApplicationHWnd = nullptr;

namespace
{
SDL_Window *s_platformWindow = nullptr;

// Direct3D 9 examines the native Alt+Enter message before the window
// procedure sees it. SDL3 owns fullscreen transitions for GeneralsMD, so
// present the key to SDL as a normal key message and prevent the D3D runtime
// from performing a second, independent transition.
bool SDLCALL s_windowsMessageHook(void *, MSG *message)
{
	if (message != nullptr && message->hwnd == ApplicationHWnd &&
		(message->message == WM_SYSKEYDOWN || message->message == WM_SYSKEYUP) &&
		message->wParam == VK_RETURN && (message->lParam & (1L << 29)) != 0)
	{
		message->message = message->message == WM_SYSKEYDOWN ? WM_KEYDOWN : WM_KEYUP;
	}

	return true;
}

#ifdef RTS_ENABLE_CRASHDUMP
LONG WINAPI s_unhandledExceptionFilter(_EXCEPTION_POINTERS *exceptionInfo)
{
	DumpExceptionInfo(exceptionInfo->ExceptionRecord->ExceptionCode, exceptionInfo);
	if (TheMiniDumper != nullptr && TheMiniDumper->IsInitialized())
	{
		TheMiniDumper->TriggerMiniDumpForException(exceptionInfo, DumpType_Minimal);
		TheMiniDumper->TriggerMiniDumpForException(exceptionInfo, DumpType_Full);
		MiniDumper::shutdownMiniDumper();
	}
	return EXCEPTION_EXECUTE_HANDLER;
}
#else
LONG WINAPI s_unhandledExceptionFilter(_EXCEPTION_POINTERS *)
{
	return EXCEPTION_EXECUTE_HANDLER;
}
#endif
} // namespace

bool SDLPlatformWindow::initialize(int width, int height, bool fullscreen)
{
	if (m_window != nullptr)
		return true;

	if (!SDL_Init(SDL_INIT_VIDEO))
		return false;

	// Let SDL select the appropriate custom cursor representation for the
	// monitor's content scale. SDL3Mouse supplies the high-DPI images.
	SDL_SetHint(SDL_HINT_MOUSE_DPI_SCALE_CURSORS, "1");

	SDL_WindowFlags flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
	if (fullscreen)
		flags |= SDL_WINDOW_FULLSCREEN;

	m_window = SDL_CreateWindow("Command and Conquer Generals", width, height, flags);
	if (m_window == nullptr)
	{
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		return false;
	}

	if (!fullscreen)
	{
		SDL_SetWindowBordered(m_window, true);
		SDL_SetWindowResizable(m_window, true);
	}
	SDL_SetWindowMinimumSize(m_window, 640, 480);

	const SDL_PropertiesID properties = SDL_GetWindowProperties(m_window);
	m_nativeHandle = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
	if (m_nativeHandle == nullptr)
	{
		SDL_DestroyWindow(m_window);
		m_window = nullptr;
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		return false;
	}

	ApplicationHWnd = static_cast<HWND>(m_nativeHandle);
	s_platformWindow = m_window;
	SDL_SetWindowsMessageHook(s_windowsMessageHook, nullptr);
	return true;
}

void SDLPlatformWindow::show()
{
	if (m_window == nullptr)
		return;

	SDL_ShowWindow(m_window);
	SDL_RaiseWindow(m_window);
}

void SDLPlatformWindow::hide()
{
	if (s_platformWindow != nullptr)
		SDL_HideWindow(s_platformWindow);
}

int SDLPlatformWindow::showMessageBox(const char *message, const char *title, int type)
{
	SDL_MessageBoxButtonData buttons[3]{};
	int buttonCount = 1;

	if (type == 1)
	{
		buttons[0] = { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "OK" };
	}
	else if (type == 2)
	{
		buttons[0] = { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 6, "Yes" };
		buttons[1] = { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 7, "No" };
		buttonCount = 2;
	}
	else
	{
		buttons[0] = { 0, 3, "Abort" };
		buttons[1] = { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 4, "Retry" };
		buttons[2] = { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 5, "Ignore" };
		buttonCount = 3;
	}

	SDL_MessageBoxData data{};
	data.flags = SDL_MESSAGEBOX_INFORMATION;
	data.window = s_platformWindow;
	data.title = title != nullptr ? title : "Generals";
	data.message = message != nullptr ? message : "";
	data.numbuttons = buttonCount;
	data.buttons = buttons;

	int buttonId = 0;
	if (!SDL_ShowMessageBox(&data, &buttonId))
		return 0;
	return buttonId;
}

void SDLPlatformWindow::shutdown()
{
	if (m_window != nullptr)
		SDL_DestroyWindow(m_window);

	m_window = nullptr;
	m_nativeHandle = nullptr;
	ApplicationHWnd = nullptr;
	s_platformWindow = nullptr;
	SDL_SetWindowsMessageHook(nullptr, nullptr);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void *SDLPlatformWindow::window()
{
	return s_platformWindow;
}

void *SDLPlatformWindow::nativeHandle()
{
	if (s_platformWindow == nullptr)
		return nullptr;

	const SDL_PropertiesID properties = SDL_GetWindowProperties(s_platformWindow);
	return SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
}

void *SDLPlatformWindow::nativeInstance() const
{
	return GetModuleHandleA(nullptr);
}

void SDLPlatformWindow::setTitle(const char *title)
{
	if (s_platformWindow != nullptr)
		SDL_SetWindowTitle(s_platformWindow, title != nullptr ? title : "");
}

bool SDLPlatformWindow::isMinimized()
{
	return s_platformWindow != nullptr &&
		(SDL_GetWindowFlags(s_platformWindow) & SDL_WINDOW_MINIMIZED) != 0;
}

bool SDLPlatformWindow::setWorkingDirectoryToExecutable()
{
	const char *basePath = SDL_GetBasePath();
	if (basePath == nullptr)
		return false;

	std::error_code error;
	std::filesystem::current_path(std::filesystem::path(basePath), error);
	return !error;
}

bool SDLPlatformWindow::restoreExistingInstance(const char *instanceName)
{
	if (instanceName == nullptr)
		return false;

	HWND existingWindow = FindWindowA(instanceName, nullptr);
	if (existingWindow == nullptr)
		return false;

	SetForegroundWindow(existingWindow);
	ShowWindow(existingWindow, SW_RESTORE);
	return true;
}

void SDLPlatformWindow::initializeRuntimeModule()
{
	_Module.Init(nullptr, GetModuleHandleA(nullptr));
}

void SDLPlatformWindow::installCrashHandler()
{
	SetUnhandledExceptionFilter(s_unhandledExceptionFilter);
}
