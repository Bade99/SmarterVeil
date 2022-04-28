#pragma once

//NOTE(fran): Many vks cant be mapped to strings by GetKeyNameText, mainly extended ones, MapVirtualKeyEx is able to produce the appropiate scancode but GetKeyNameText simply refuses to work with extended keys. Gotta do it manually, which means we'll only return strings in English

// Virtual Keys, Standard Set
static constexpr s8 vkTranslation[256] = {
const_temp_s(u8""), //0x00
const_temp_s(u8"Left click"), //0x01
const_temp_s(u8"Right click"), //0x02
const_temp_s(u8"Control-break processing"), //0x03
const_temp_s(u8"Middle click"), //0x04
const_temp_s(u8"X1 click"), //0x05
const_temp_s(u8"X2 click"), //0x06
const_temp_s(u8"undefined 0x07"), //0x07
const_temp_s(u8"Backspace"), //0x08
const_temp_s(u8"Tab"), //0x09
const_temp_s(u8"undefined 0x0A"), //0x0A
const_temp_s(u8"undefined 0x0B"), //0x0B
const_temp_s(u8"Clear"), //0x0C
const_temp_s(u8"Enter"), //0x0D
const_temp_s(u8"undefined 0x0E"), //0x0E
const_temp_s(u8"undefined 0x0F"), //0x0F
const_temp_s(u8"Shift"), //0x10
const_temp_s(u8"Ctrl"), //0x11
const_temp_s(u8"Alt"), //0x12
const_temp_s(u8"Pause") , //0x13
const_temp_s(u8"Caps lock") , //0x14
const_temp_s(u8"Kana mode") , //0x15
const_temp_s(u8"IME on"), //0x16
const_temp_s(u8"Junja mode"), //0x17
const_temp_s(u8"Final mode"), //0x18
//{0x19, _t("Hanja mode")}, //TODO(fran)?: depending on the keyboard it should read Hanja or Kanji
const_temp_s(u8"Kanji mode"), //0x19
const_temp_s(u8"IME off"), //0x1A
const_temp_s(u8"Escape"), //0x1B
const_temp_s(u8"IME convert"), //0x1C
const_temp_s(u8"IME nonconvert"), //0x1D
const_temp_s(u8"IME accept"), //0x1E
const_temp_s(u8"IME mode change"), //0x1F
const_temp_s(u8"Space"), //0x20
const_temp_s(u8"Page up"), //0x21
const_temp_s(u8"Page down"), //0x22
const_temp_s(u8"End"), //0x23
const_temp_s(u8"Home"), //0x24
const_temp_s(u8"Left arrow"), //0x25
const_temp_s(u8"Up arrow"), //0x26
const_temp_s(u8"Right arrow"), //0x27
const_temp_s(u8"Down arrow"), //0x28
const_temp_s(u8"Select"), //0x29
const_temp_s(u8"Print"), //0x2A
const_temp_s(u8"Execute"), //0x2B
const_temp_s(u8"Print screen"), //0x2C
const_temp_s(u8"Insert"), //0x2D
const_temp_s(u8"Delete"), //0x2E
const_temp_s(u8"Help"), //0x2F
const_temp_s(u8"0"), //0x30
const_temp_s(u8"1"), //0x31
const_temp_s(u8"2"), //0x32
const_temp_s(u8"3"), //0x33
const_temp_s(u8"4"), //0x34
const_temp_s(u8"5"), //0x35
const_temp_s(u8"6"), //0x36
const_temp_s(u8"7"), //0x37
const_temp_s(u8"8"), //0x38
const_temp_s(u8"9"), //0x39
const_temp_s(u8"undefined 0x3A"), //0x3A
const_temp_s(u8"undefined 0x3B"), //0x3B
const_temp_s(u8"undefined 0x3C"), //0x3C
const_temp_s(u8"undefined 0x3D"), //0x3D
const_temp_s(u8"undefined 0x3E"), //0x3E
const_temp_s(u8"undefined 0x3F"), //0x3F
const_temp_s(u8"undefined 0x40"), //0x40
const_temp_s(u8"A"), //0x41
const_temp_s(u8"B"), //0x42
const_temp_s(u8"C"), //0x43
const_temp_s(u8"D"), //0x44
const_temp_s(u8"E"), //0x45
const_temp_s(u8"F"), //0x46
const_temp_s(u8"G"), //0x47
const_temp_s(u8"H"), //0x48
const_temp_s(u8"I"), //0x49
const_temp_s(u8"J"), //0x4A
const_temp_s(u8"K"), //0x4A
const_temp_s(u8"L"), //0x4C
const_temp_s(u8"M"), //0x4D
const_temp_s(u8"N"), //0x4E
const_temp_s(u8"O"), //0x4F
const_temp_s(u8"P"), //0x50
const_temp_s(u8"Q"), //0x51
const_temp_s(u8"R"), //0x52
const_temp_s(u8"S"), //0x53
const_temp_s(u8"T"), //0x54
const_temp_s(u8"U"), //0x55
const_temp_s(u8"V"), //0x56
const_temp_s(u8"W"), //0x57
const_temp_s(u8"X"), //0x58
const_temp_s(u8"Y"), //0x59
const_temp_s(u8"Z"), //0x5A
const_temp_s(u8"Windows left"), //0x5B
const_temp_s(u8"Windows right"), //0x5C
const_temp_s(u8"Applications"), //0x5D
const_temp_s(u8"undefined 0x5E"), //0x5E
const_temp_s(u8"Sleep"), //0x5F
const_temp_s(u8"Keypad 0"), //0x60
const_temp_s(u8"Keypad 1"), //0x61
const_temp_s(u8"Keypad 2"), //0x62
const_temp_s(u8"Keypad 3"), //0x63
const_temp_s(u8"Keypad 4"), //0x64
const_temp_s(u8"Keypad 5"), //0x65
const_temp_s(u8"Keypad 6"), //0x66
const_temp_s(u8"Keypad 7"), //0x67
const_temp_s(u8"Keypad 8"), //0x68
const_temp_s(u8"Keypad 9"), //0x69
const_temp_s(u8"Multiply"), //0x6A
const_temp_s(u8"Add"), //0x6B
const_temp_s(u8"Separator"), //0x6C
const_temp_s(u8"Subtract"), //0x6D
const_temp_s(u8"Decimal"), //0x6E
const_temp_s(u8"Divide"), //0x6F
const_temp_s(u8"F1"), //0x70
const_temp_s(u8"F2"), //0x71
const_temp_s(u8"F3"), //0x72
const_temp_s(u8"F4"), //0x73
const_temp_s(u8"F5"), //0x74
const_temp_s(u8"F6"), //0x75
const_temp_s(u8"F7"), //0x76
const_temp_s(u8"F8"), //0x77
const_temp_s(u8"F9"), //0x78
const_temp_s(u8"F10"), //0x79
const_temp_s(u8"F11"), //0x7A
const_temp_s(u8"F12"), //0x7B
const_temp_s(u8"F13"), //0x7C
const_temp_s(u8"F14"), //0x7D
const_temp_s(u8"F15"), //0x7E
const_temp_s(u8"F16"), //0x7F
const_temp_s(u8"F17"), //0x80
const_temp_s(u8"F18"), //0x81
const_temp_s(u8"F19"), //0x82
const_temp_s(u8"F20"), //0x83
const_temp_s(u8"F21"), //0x84
const_temp_s(u8"F22"), //0x85
const_temp_s(u8"F23"), //0x86
const_temp_s(u8"F24"), //0x87
const_temp_s(u8"UI nav view"), //0x88
const_temp_s(u8"UI nav menu"), //0x89
const_temp_s(u8"UI nav up"), //0x8A
const_temp_s(u8"UI nav down"), //0x8B
const_temp_s(u8"UI nav left"), //0x8C
const_temp_s(u8"UI nav right"), //0x8D
const_temp_s(u8"UI nav accept"), //0x8E
const_temp_s(u8"UI nav cancel"), //0x8F
const_temp_s(u8"Num lock"), //0x90
const_temp_s(u8"Scroll lock"), //0x91
const_temp_s(u8"OEM specific 0x92") , //0x92
const_temp_s(u8"OEM specific 0x93") , //0x93
const_temp_s(u8"OEM specific 0x94") , //0x94
const_temp_s(u8"OEM specific 0x95") , //0x95
const_temp_s(u8"OEM specific 0x96") , //0x96
const_temp_s(u8"undefined 0x97") , //0x97
const_temp_s(u8"undefined 0x98") , //0x98
const_temp_s(u8"undefined 0x99") , //0x99
const_temp_s(u8"undefined 0x9A") , //0x9A
const_temp_s(u8"undefined 0x9B") , //0x9B
const_temp_s(u8"undefined 0x9C") , //0x9C
const_temp_s(u8"undefined 0x9D") , //0x9D
const_temp_s(u8"undefined 0x9E") , //0x9E
const_temp_s(u8"undefined 0x9F") , //0x9F
// VK_L* and VK_R* for Alt, Ctrl and Shift vks are used only as params to GetAsyncKeyState(), GetKeyState(). No other API or msg uses this
const_temp_s(u8"Left Shift") , //0xA0
const_temp_s(u8"Right Shift") , //0xA1
const_temp_s(u8"Left Control") , //0xA2
const_temp_s(u8"Right Control") , //0xA3
const_temp_s(u8"Left Alt") , //0xA4
const_temp_s(u8"Right Alt") , //0xA5
const_temp_s(u8"Browser Back"), //0xA6
const_temp_s(u8"Browser Forward"), //0xA7
const_temp_s(u8"Browser Refresh"), //0xA8
const_temp_s(u8"Browser Stop"), //0xA9
const_temp_s(u8"Browser Search"), //0xAA
const_temp_s(u8"Browser Favorites"), //0xAB
const_temp_s(u8"Browser Home"), //0xAC
const_temp_s(u8"Mute"), //0xAD
const_temp_s(u8"Volume Down"), //0xAE
const_temp_s(u8"Volume Up"), //0xAF
const_temp_s(u8"Next Track"), //0xB0
const_temp_s(u8"Prev Track"), //0xB1
const_temp_s(u8"Stop"), //0xB2
const_temp_s(u8"Play/Pause"), //0xB3
const_temp_s(u8"Start Mail"), //0xB4
const_temp_s(u8"Select Media"), //0xB5
const_temp_s(u8"Start app 1"), //0xB6
const_temp_s(u8"Start app 2"), //0xB7
const_temp_s(u8"undefined 0xB8") , //0xB8
const_temp_s(u8"undefined 0xB9") , //0xB9
const_temp_s(u8"OEM specific 0xBA"), //0xBA // ';:' for US
const_temp_s(u8"+"), //0xBB // '+' any country
const_temp_s(u8","), //0xBC // ',' any country
const_temp_s(u8"-"), //0xBD // '-' any country
const_temp_s(u8"."), //0xBE // '.' any country
const_temp_s(u8"OEM specific 0xBF"), //0xBF // '/?' for US
const_temp_s(u8"OEM specific 0xC0"), //0xC0 // '`~' for US
const_temp_s(u8"undefined 0xC1"), //0xC1
const_temp_s(u8"undefined 0xC2"), //0xC2
const_temp_s(u8"Gamepad A"), //0xC3 // reserved
const_temp_s(u8"Gamepad B"), //0xC4 // reserved
const_temp_s(u8"Gamepad X"), //0xC5 // reserved
const_temp_s(u8"Gamepad Y"), //0xC6 // reserved
const_temp_s(u8"Gamepad Right Shoulder"), //0xC7 // reserved
const_temp_s(u8"Gamepad Left Shoulder"), //0xC8 // reserved
const_temp_s(u8"Gamepad Left Trigger"), //0xC9 // reserved
const_temp_s(u8"Gamepad Right Trigger"), //0xCA // reserved
const_temp_s(u8"Gamepad Dpad Up"), //0xCB // reserved
const_temp_s(u8"Gamepad Dpad Down"), //0xCC // reserved
const_temp_s(u8"Gamepad Dpad Left"), //0xCD // reserved
const_temp_s(u8"Gamepad Dpad Right"), //0xCE // reserved
const_temp_s(u8"Gamepad Menu"), //0xCF // reserved
const_temp_s(u8"Gamepad View"), //0xD0 // reserved
const_temp_s(u8"Gamepad Left Thumb Button"), //0xD1 // reserved
const_temp_s(u8"Gamepad Right Thumb Button"), //0xD2 // reserved
const_temp_s(u8"Gamepad Left Thumb Up"), //0xD3 // reserved
const_temp_s(u8"Gamepad Left Thumb Down"), //0xD4 // reserved
const_temp_s(u8"Gamepad Left Thumb Right"), //0xD5 // reserved
const_temp_s(u8"Gamepad Left Thumb Left"), //0xD6 // reserved
const_temp_s(u8"Gamepad Right Thumb Up"), //0xD7 // reserved
const_temp_s(u8"Gamepad Right Thumb Down"), //0xD8 // reserved
const_temp_s(u8"Gamepad Right Thumb Right"), //0xD9 // reserved
const_temp_s(u8"Gamepad Right Thumb Left"), //0xDA // reserved
const_temp_s(u8"OEM specific 0xDB"), //0xDB //  '[{' for US
const_temp_s(u8"OEM specific 0xDC"), //0xDC //  '\|' for US
const_temp_s(u8"OEM specific 0xDD"), //0xDD //  ']}' for US
const_temp_s(u8"OEM specific 0xDE"), //0xDE //  ''"' for US
const_temp_s(u8"OEM specific 0xDF"), //0xDF
const_temp_s(u8"undefined 0xE0"), //0xE0
const_temp_s(u8"OEM specific 0xE1"), //0xE1 //  'AX' key on Japanese AX kbd
const_temp_s(u8"OEM specific 0xE2"), //0xE2 //  " < > " or "\ | " on RT 102-key kbd.
const_temp_s(u8"OEM specific 0xE3"), //0xE3 //  Help key on ICO
const_temp_s(u8"OEM specific 0xE4"), //0xE4 //  00 key on ICO
const_temp_s(u8"IME process"), //0xE5
const_temp_s(u8"OEM specific 0xE6"), //0xE6 // VK_ICO_CLEAR
const_temp_s(u8"Packet"), //0xE7 // Some crazy unicode stuff
const_temp_s(u8"undefined 0xE8"), //0xE8
const_temp_s(u8"OEM specific 0xE9"), //0xE9 // VK_OEM_RESET      
const_temp_s(u8"OEM specific 0xEA"), //0xEA // VK_OEM_JUMP       
const_temp_s(u8"OEM specific 0xEB"), //0xEB // VK_OEM_PA1        
const_temp_s(u8"OEM specific 0xEC"), //0xEC // VK_OEM_PA2        
const_temp_s(u8"OEM specific 0xED"), //0xED // VK_OEM_PA3        
const_temp_s(u8"OEM specific 0xEE"), //0xEE // VK_OEM_WSCTRL     
const_temp_s(u8"OEM specific 0xEF"), //0xEF // VK_OEM_CUSEL      
const_temp_s(u8"OEM specific 0xF0"), //0xF0 // VK_OEM_ATTN       
const_temp_s(u8"OEM specific 0xF1"), //0xF1 // VK_OEM_FINISH     
const_temp_s(u8"OEM specific 0xF2"), //0xF2 // VK_OEM_COPY       
const_temp_s(u8"OEM specific 0xF3"), //0xF3 // VK_OEM_AUTO       
const_temp_s(u8"OEM specific 0xF4"), //0xF4 // VK_OEM_ENLW       
const_temp_s(u8"OEM specific 0xF5"), //0xF5 // VK_OEM_BACKTAB    
const_temp_s(u8"Attn"), //0xF6
const_temp_s(u8"CrSel"), //0xF7
const_temp_s(u8"ExSel"), //0xF8
const_temp_s(u8"Erase EOF"), //0xF9
const_temp_s(u8"Play"), //0xFA
const_temp_s(u8"Zoom"), //0xFB
const_temp_s(u8"undefined 0xFC"), //0xFC //VK_NONAME
const_temp_s(u8"PA1"), //0xFD
const_temp_s(u8"Clear"), //0xFE
const_temp_s(u8"undefined 0xFF"), //0xFF
};

s8 VkToString(u8 vk) {
	s8 res = vkTranslation[vk];
	return res;
}