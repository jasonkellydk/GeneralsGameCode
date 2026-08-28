/*
** Command & Conquer Generals Zero Hour(tm)
** SDL3 operating-system display services.
*/

#include "Common/OSDisplay.h"

#include "Common/AsciiString.h"
#include "Common/UnicodeString.h"
#include "GameClient/GameText.h"

#include <SDL3/SDL.h>

namespace
{
Uint32 messageBoxFlags(UnsignedInt otherFlags)
{
	if (BitIsSet(otherFlags, OSDOF_ERRORICON) || BitIsSet(otherFlags, OSDOF_STOPICON))
		return SDL_MESSAGEBOX_ERROR;
	if (BitIsSet(otherFlags, OSDOF_EXCLAMATIONICON))
		return SDL_MESSAGEBOX_WARNING;
	return SDL_MESSAGEBOX_INFORMATION;
}
}

OSDisplayButtonType OSDisplayWarningBox(AsciiString prompt, AsciiString message,
	UnsignedInt buttonFlags, UnsignedInt otherFlags)
{
	if (TheGameText == nullptr)
		return OSDBT_ERROR;

	UnicodeString promptString = TheGameText->fetch(prompt);
	UnicodeString messageString = TheGameText->fetch(message);
	AsciiString promptUtf8;
	AsciiString messageUtf8;
	promptUtf8.translate(promptString);
	messageUtf8.translate(messageString);

	SDL_MessageBoxButtonData buttons[2]{};
	int buttonCount = 0;
	if (BitIsSet(buttonFlags, OSDBT_OK) || !BitIsSet(buttonFlags, OSDBT_CANCEL))
	{
		buttons[buttonCount++] = {
			SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "OK"
		};
	}
	if (BitIsSet(buttonFlags, OSDBT_CANCEL))
	{
		buttons[buttonCount++] = {
			SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 2, "Cancel"
		};
	}

	SDL_MessageBoxData data{};
	data.flags = messageBoxFlags(otherFlags);
	data.window = SDL_GetKeyboardFocus();
	data.title = promptUtf8.str();
	data.message = messageUtf8.str();
	data.numbuttons = buttonCount;
	data.buttons = buttons;

	int buttonId = 0;
	if (!SDL_ShowMessageBox(&data, &buttonId))
		return OSDBT_ERROR;
	return buttonId == 1 ? OSDBT_OK : OSDBT_CANCEL;
}

void OSDisplaySetBusyState(Bool busyDisplay, Bool busySystem)
{
	// SDL owns the platform-specific power-management implementation. The
	// display and system flags share SDL's screensaver inhibition capability.
	if (busyDisplay || busySystem)
		SDL_DisableScreenSaver();
	else
		SDL_EnableScreenSaver();
}
