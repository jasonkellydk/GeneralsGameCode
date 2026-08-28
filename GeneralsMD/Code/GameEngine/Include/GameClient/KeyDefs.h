/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

// FILE: KeyDefs.h ////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Westwood Studios Pacific.
//
//                       Confidential Information
//                Copyright (C) 2001 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
// Project:    RTS3
//
// File name:  KeyDefs.h
//
// Created:    Mike Morrison, 1995
//						 Colin Day, June 2001
//
// Desc:       Basic keyboard key definitions.
//
/** @todo NOTE: These key definitions are currently tied directly to the
*		Direct Input key codes, therefore making these definitions device
*		dependent even though this code lives on the device INdependent side
*		of the engine.  In the future to be truly device independent we
*		need to define our own key codes, and have a translation between
*		what we read from the device to our own system*/
//
//-----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////

#pragma once

// SYSTEM INCLUDES ////////////////////////////////////////////////////////////
#include <stdlib.h>
#ifndef DIRECTINPUT_VERSION
#	define DIRECTINPUT_VERSION	0x800
#endif

#include <SDL3/SDL.h>
#include <Lib/BaseType.h>

// Historical DIK spellings retained for save/config compatibility. Values are
// SDL3 scancodes, so this engine header no longer depends on DirectInput.
#define DIK_NUMPAD0 SDL_SCANCODE_KP_0
#define DIK_NUMPAD1 SDL_SCANCODE_KP_1
#define DIK_NUMPAD2 SDL_SCANCODE_KP_2
#define DIK_NUMPAD3 SDL_SCANCODE_KP_3
#define DIK_NUMPAD4 SDL_SCANCODE_KP_4
#define DIK_NUMPAD5 SDL_SCANCODE_KP_5
#define DIK_NUMPAD6 SDL_SCANCODE_KP_6
#define DIK_NUMPAD7 SDL_SCANCODE_KP_7
#define DIK_NUMPAD8 SDL_SCANCODE_KP_8
#define DIK_NUMPAD9 SDL_SCANCODE_KP_9
#define DIK_NUMPADPERIOD SDL_SCANCODE_KP_PERIOD
#define DIK_NUMPADSTAR SDL_SCANCODE_KP_MULTIPLY
#define DIK_NUMPADMINUS SDL_SCANCODE_KP_MINUS
#define DIK_NUMPADPLUS SDL_SCANCODE_KP_PLUS
#define DIK_NUMPADENTER SDL_SCANCODE_KP_ENTER
#define DIK_NUMPADSLASH SDL_SCANCODE_KP_DIVIDE
#define DIK_ESCAPE SDL_SCANCODE_ESCAPE
#define DIK_BACK SDL_SCANCODE_BACKSPACE
#define DIK_RETURN SDL_SCANCODE_RETURN
#define DIK_SPACE SDL_SCANCODE_SPACE
#define DIK_TAB SDL_SCANCODE_TAB
#define DIK_SYSRQ SDL_SCANCODE_PRINTSCREEN
#define DIK_CAPSLOCK SDL_SCANCODE_CAPSLOCK
#define DIK_NUMLOCK SDL_SCANCODE_NUMLOCKCLEAR
#define DIK_SCROLL SDL_SCANCODE_SCROLLLOCK
#define DIK_LCONTROL SDL_SCANCODE_LCTRL
#define DIK_RCONTROL SDL_SCANCODE_RCTRL
#define DIK_LALT SDL_SCANCODE_LALT
#define DIK_RALT SDL_SCANCODE_RALT
#define DIK_LSHIFT SDL_SCANCODE_LSHIFT
#define DIK_RSHIFT SDL_SCANCODE_RSHIFT
#define DIK_UPARROW SDL_SCANCODE_UP
#define DIK_DOWNARROW SDL_SCANCODE_DOWN
#define DIK_LEFTARROW SDL_SCANCODE_LEFT
#define DIK_RIGHTARROW SDL_SCANCODE_RIGHT
#define DIK_HOME SDL_SCANCODE_HOME
#define DIK_END SDL_SCANCODE_END
#define DIK_PGUP SDL_SCANCODE_PAGEUP
#define DIK_PGDN SDL_SCANCODE_PAGEDOWN
#define DIK_INSERT SDL_SCANCODE_INSERT
#define DIK_DELETE SDL_SCANCODE_DELETE
#define DIK_OEM_102 SDL_SCANCODE_NONUSBACKSLASH
#define DIK_KANA SDL_SCANCODE_INTERNATIONAL1
#define DIK_CONVERT SDL_SCANCODE_INTERNATIONAL4
#define DIK_NOCONVERT SDL_SCANCODE_INTERNATIONAL5
#define DIK_YEN SDL_SCANCODE_INTERNATIONAL3
#define DIK_CIRCUMFLEX SDL_SCANCODE_INTERNATIONAL6
#define DIK_KANJI SDL_SCANCODE_INTERNATIONAL2
#define DIK_0 SDL_SCANCODE_0
#define DIK_1 SDL_SCANCODE_1
#define DIK_2 SDL_SCANCODE_2
#define DIK_3 SDL_SCANCODE_3
#define DIK_4 SDL_SCANCODE_4
#define DIK_5 SDL_SCANCODE_5
#define DIK_6 SDL_SCANCODE_6
#define DIK_7 SDL_SCANCODE_7
#define DIK_8 SDL_SCANCODE_8
#define DIK_9 SDL_SCANCODE_9
#define DIK_A SDL_SCANCODE_A
#define DIK_B SDL_SCANCODE_B
#define DIK_C SDL_SCANCODE_C
#define DIK_D SDL_SCANCODE_D
#define DIK_E SDL_SCANCODE_E
#define DIK_F SDL_SCANCODE_F
#define DIK_G SDL_SCANCODE_G
#define DIK_H SDL_SCANCODE_H
#define DIK_I SDL_SCANCODE_I
#define DIK_J SDL_SCANCODE_J
#define DIK_K SDL_SCANCODE_K
#define DIK_L SDL_SCANCODE_L
#define DIK_M SDL_SCANCODE_M
#define DIK_N SDL_SCANCODE_N
#define DIK_O SDL_SCANCODE_O
#define DIK_P SDL_SCANCODE_P
#define DIK_Q SDL_SCANCODE_Q
#define DIK_R SDL_SCANCODE_R
#define DIK_S SDL_SCANCODE_S
#define DIK_T SDL_SCANCODE_T
#define DIK_U SDL_SCANCODE_U
#define DIK_V SDL_SCANCODE_V
#define DIK_W SDL_SCANCODE_W
#define DIK_X SDL_SCANCODE_X
#define DIK_Y SDL_SCANCODE_Y
#define DIK_Z SDL_SCANCODE_Z
#define DIK_F1 SDL_SCANCODE_F1
#define DIK_F2 SDL_SCANCODE_F2
#define DIK_F3 SDL_SCANCODE_F3
#define DIK_F4 SDL_SCANCODE_F4
#define DIK_F5 SDL_SCANCODE_F5
#define DIK_F6 SDL_SCANCODE_F6
#define DIK_F7 SDL_SCANCODE_F7
#define DIK_F8 SDL_SCANCODE_F8
#define DIK_F9 SDL_SCANCODE_F9
#define DIK_F10 SDL_SCANCODE_F10
#define DIK_F11 SDL_SCANCODE_F11
#define DIK_F12 SDL_SCANCODE_F12
#define DIK_MINUS SDL_SCANCODE_MINUS
#define DIK_EQUALS SDL_SCANCODE_EQUALS
#define DIK_LBRACKET SDL_SCANCODE_LEFTBRACKET
#define DIK_RBRACKET SDL_SCANCODE_RIGHTBRACKET
#define DIK_SEMICOLON SDL_SCANCODE_SEMICOLON
#define DIK_APOSTROPHE SDL_SCANCODE_APOSTROPHE
#define DIK_GRAVE SDL_SCANCODE_GRAVE
#define DIK_BACKSLASH SDL_SCANCODE_BACKSLASH
#define DIK_COMMA SDL_SCANCODE_COMMA
#define DIK_PERIOD SDL_SCANCODE_PERIOD
#define DIK_SLASH SDL_SCANCODE_SLASH

// USER INCLUDES //////////////////////////////////////////////////////////////

// FORWARD REFERENCES /////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// TYPE DEFINES ///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#ifndef DIK_OEM_102
#define DIK_OEM_102         0x56    /* < > | on UK/Germany keyboards  */
#endif

#ifndef DIK_KANA
#define DIK_KANA            0x70    /* (Japanese keyboard)            */
#endif

#ifndef DIK_CONVERT
#define DIK_CONVERT         0x79    /* (Japanese keyboard)            */
#endif

#ifndef DIK_NOCONVERT
#define DIK_NOCONVERT       0x7B    /* (Japanese keyboard)            */
#endif

#ifndef DIK_YEN
#define DIK_YEN             0x7D    /* (Japanese keyboard)            */
#endif

#ifndef DIK_CIRCUMFLEX
#define DIK_CIRCUMFLEX      0x90    /* (Japanese keyboard)            */
#endif

#ifndef DIK_KANJI
#define DIK_KANJI           0x94    /* (Japanese keyboard)            */
#endif

//=============================================================================
/** The key tables */
//=============================================================================

enum KeyDefType CPP_11(: UnsignedByte)
{
	// keypad keys ----------------------------------------------------------------
	KEY_KP0 								= DIK_NUMPAD0,
	KEY_KP1 								= DIK_NUMPAD1,
	KEY_KP2 								= DIK_NUMPAD2,
	KEY_KP3 								= DIK_NUMPAD3,
	KEY_KP4 								= DIK_NUMPAD4,
	KEY_KP5 								= DIK_NUMPAD5,
	KEY_KP6 								= DIK_NUMPAD6,
	KEY_KP7 								= DIK_NUMPAD7,
	KEY_KP8 								= DIK_NUMPAD8,
	KEY_KP9 								= DIK_NUMPAD9,
	KEY_KPDEL 							= DIK_NUMPADPERIOD,
	KEY_KPSTAR 							= DIK_NUMPADSTAR,
	KEY_KPMINUS 						= DIK_NUMPADMINUS,
	KEY_KPPLUS 							= DIK_NUMPADPLUS,

	// regular keys ---------------------------------------------------------------
	KEY_ESC 								= DIK_ESCAPE,
	KEY_BACKSPACE 					= DIK_BACK,
	KEY_ENTER 							= DIK_RETURN,
	KEY_SPACE 							= DIK_SPACE,
	KEY_TAB 								= DIK_TAB,
	KEY_F1 									= DIK_F1,
	KEY_F2 									= DIK_F2,
	KEY_F3 									= DIK_F3,
	KEY_F4 									= DIK_F4,
	KEY_F5 									= DIK_F5,
	KEY_F6 									= DIK_F6,
	KEY_F7 									= DIK_F7,
	KEY_F8 									= DIK_F8,
	KEY_F9 									= DIK_F9,
	KEY_F10 								= DIK_F10,
	KEY_F11 								= DIK_F11,
	KEY_F12 								= DIK_F12,
	KEY_A 									= DIK_A,
	KEY_B 									= DIK_B,
	KEY_C 									= DIK_C,
	KEY_D 									= DIK_D,
	KEY_E 									= DIK_E,
	KEY_F 									= DIK_F,
	KEY_G 									= DIK_G,
	KEY_H 									= DIK_H,
	KEY_I 									= DIK_I,
	KEY_J 									= DIK_J,
	KEY_K 									= DIK_K,
	KEY_L 									= DIK_L,
	KEY_M 									= DIK_M,
	KEY_N 									= DIK_N,
	KEY_O 									= DIK_O,
	KEY_P 									= DIK_P,
	KEY_Q 									= DIK_Q,
	KEY_R 									= DIK_R,
	KEY_S 									= DIK_S,
	KEY_T 									= DIK_T,
	KEY_U 									= DIK_U,
	KEY_V 									= DIK_V,
	KEY_W 									= DIK_W,
	KEY_X 									= DIK_X,
	KEY_Y 									= DIK_Y,
	KEY_Z 									= DIK_Z,
	KEY_1 									= DIK_1,
	KEY_2 									= DIK_2,
	KEY_3 									= DIK_3,
	KEY_4 									= DIK_4,
	KEY_5 									= DIK_5,
	KEY_6 									= DIK_6,
	KEY_7 									= DIK_7,
	KEY_8 									= DIK_8,
	KEY_9 									= DIK_9,
	KEY_0 									= DIK_0,
	KEY_MINUS 							= DIK_MINUS,
	KEY_EQUAL 							= DIK_EQUALS,
	KEY_LBRACKET 						= DIK_LBRACKET,
	KEY_RBRACKET 						= DIK_RBRACKET,
	KEY_SEMICOLON 					= DIK_SEMICOLON,
	KEY_APOSTROPHE 					= DIK_APOSTROPHE,
	KEY_TICK 								= DIK_GRAVE,
	KEY_BACKSLASH 					= DIK_BACKSLASH,
	KEY_COMMA 							= DIK_COMMA,
	KEY_PERIOD 							= DIK_PERIOD,
	KEY_SLASH 							= DIK_SLASH,

	// special keys ---------------------------------------------------------------
	KEY_SYSREQ 							= DIK_SYSRQ,

	KEY_CAPS 								= DIK_CAPSLOCK,
	KEY_NUM 								= DIK_NUMLOCK,
	KEY_SCROLL 							= DIK_SCROLL,
	KEY_LCTRL 							= DIK_LCONTROL,
	KEY_LALT 								= DIK_LALT,
	KEY_LSHIFT 							= DIK_LSHIFT,
	KEY_RSHIFT 							= DIK_RSHIFT,

	KEY_UP 									= DIK_UPARROW,
	KEY_DOWN 								= DIK_DOWNARROW,
	KEY_LEFT 								= DIK_LEFTARROW,
	KEY_RIGHT 							= DIK_RIGHTARROW,
	KEY_RALT 								= DIK_RALT,
	KEY_RCTRL 							= DIK_RCONTROL,
	KEY_HOME 								= DIK_HOME,
	KEY_END 								= DIK_END,
	KEY_PGUP 								= DIK_PGUP,
	KEY_PGDN 								= DIK_PGDN,
	KEY_INS 								= DIK_INSERT,
	KEY_DEL 								= DIK_DELETE,
	KEY_KPENTER 						= DIK_NUMPADENTER,
	KEY_KPSLASH 						= DIK_NUMPADSLASH,

	KEY_102 								= DIK_OEM_102,

	// Japanese keyboard keys -----------------------------------------------------
	KEY_KANA 								= DIK_KANA,
	KEY_CONVERT 						= DIK_CONVERT,
	KEY_NOCONVERT 					= DIK_NOCONVERT,
	KEY_YEN 								= DIK_YEN,
	KEY_CIRCUMFLEX 					= DIK_CIRCUMFLEX,
	KEY_KANJI 							= DIK_KANJI,

	// specials -------------------------------------------------------------------
	KEY_NONE								= 0x00,		///< to report end of key stream
	KEY_LOST								= 0xFF,		///< to report lost keyboard focus

};

enum
{
	KEY_COUNT = 256
};

// state for keyboard IO ------------------------------------------------------
enum
{
	KEY_STATE_NONE								= 0x0000, // No modifier state
	KEY_STATE_UP									= 0x0001,	// Key is up (default state)
	KEY_STATE_DOWN								= 0x0002,	// Key is down
	KEY_STATE_LCONTROL						= 0x0004,	// Left control is pressed
	KEY_STATE_RCONTROL						= 0x0008,	// Right control is pressed
	KEY_STATE_LSHIFT							= 0x0010,	// left shift is pressed
	KEY_STATE_RSHIFT							= 0x0020,	// right shift is pressed
	KEY_STATE_LALT								= 0x0040,	// left alt is pressed
	KEY_STATE_RALT								= 0x0080,	// right alt is pressed
	KEY_STATE_AUTOREPEAT					= 0x0100,	// Key is down due to autorepeat (only seen in conjunction with KEY_STATE_DOWN)
	KEY_STATE_CAPSLOCK						= 0x0200, // Caps Lock key is on.
	KEY_STATE_SHIFT2							= 0x0400, // Alternate shift key is pressed (I think this is for foreign keyboards..)

	// modifier combinations when left/right isn't a factor
	KEY_STATE_CONTROL		= (KEY_STATE_LCONTROL | KEY_STATE_RCONTROL),
	KEY_STATE_SHIFT			= (KEY_STATE_LSHIFT | KEY_STATE_RSHIFT | KEY_STATE_SHIFT2 ),
	KEY_STATE_ALT				= (KEY_STATE_LALT | KEY_STATE_RALT)

};

// INLINING ///////////////////////////////////////////////////////////////////

// EXTERNALS //////////////////////////////////////////////////////////////////
