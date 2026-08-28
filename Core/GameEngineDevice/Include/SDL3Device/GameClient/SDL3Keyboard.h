#pragma once

#include "GameClient/Keyboard.h"
#include <SDL3/SDL.h>

class SDL3Keyboard : public Keyboard
{
public:
	SDL3Keyboard() = default;
	~SDL3Keyboard() override = default;
	void init() override;
	void reset() override;
	void update() override;
	Bool getCapsState() override;
protected:
	void getKey(KeyboardIO *key) override;
private:
	int m_scanCode = 0;
	Bool m_previousState[SDL_SCANCODE_COUNT]{};
};
