/*
 * Dwarfplex is based on Webfort, created by Vitaly Pronkin on 14/05/14.
 * Copyright (c) 2020 white-rabbit, ISC license
*/

#include <iostream>
#include "keymap.hpp"
#include "parse_config.hpp"

#include "ColorText.h"
#include "df/interface_key.h"
#include "SDL_events.h"
#include "SDL_keysym.h"

using std::string;
using std::map;
using std::vector;
using std::endl;

using namespace DFHack;

static int32_t utf8_decode(const std::string& s)
{
    if (s.length() == 1) return s.at(0);
    // TODO
    return -1;
}

static std::map<std::string, SDL::Key> sdlKeyNames;

static void setSdlKeyNames()
{
    using namespace SDL;
    sdlKeyNames.clear();
    sdlKeyNames.insert({"Backspace", K_BACKSPACE});
    sdlKeyNames.insert({"Tab", K_TAB});
    sdlKeyNames.insert({"Clear", K_CLEAR});
    sdlKeyNames.insert({"Enter", K_RETURN});
    sdlKeyNames.insert({"Pause", K_PAUSE});
    sdlKeyNames.insert({"ESC", K_ESCAPE});
    sdlKeyNames.insert({"Space", K_SPACE});
    sdlKeyNames.insert({"Exclaim", K_EXCLAIM});
    sdlKeyNames.insert({"Quotedbl", K_QUOTEDBL});
    sdlKeyNames.insert({"Hash", K_HASH});
    sdlKeyNames.insert({"Dollar", K_DOLLAR});
    sdlKeyNames.insert({"Ampersand", K_AMPERSAND});
    sdlKeyNames.insert({"Quote", K_QUOTE});
    sdlKeyNames.insert({"Leftparen", K_LEFTPAREN});
    sdlKeyNames.insert({"Rightparen", K_RIGHTPAREN});
    sdlKeyNames.insert({"Asterisk", K_ASTERISK});
    sdlKeyNames.insert({"Plus", K_PLUS});
    sdlKeyNames.insert({"Comma", K_COMMA});
    sdlKeyNames.insert({"Minus", K_MINUS});
    sdlKeyNames.insert({"Period", K_PERIOD});
    sdlKeyNames.insert({"Slash", K_SLASH});
    sdlKeyNames.insert({"0", K_0});
    sdlKeyNames.insert({"1", K_1});
    sdlKeyNames.insert({"2", K_2});
    sdlKeyNames.insert({"3", K_3});
    sdlKeyNames.insert({"4", K_4});
    sdlKeyNames.insert({"5", K_5});
    sdlKeyNames.insert({"6", K_6});
    sdlKeyNames.insert({"7", K_7});
    sdlKeyNames.insert({"8", K_8});
    sdlKeyNames.insert({"9", K_9});
    sdlKeyNames.insert({"Colon", K_COLON});
    sdlKeyNames.insert({"Semicolon", K_SEMICOLON});
    sdlKeyNames.insert({"Less", K_LESS});
    sdlKeyNames.insert({"Equals", K_EQUALS});
    sdlKeyNames.insert({"Greater", K_GREATER});
    sdlKeyNames.insert({"Question", K_QUESTION});
    sdlKeyNames.insert({"At", K_AT});
    sdlKeyNames.insert({"Leftbracket", K_LEFTBRACKET});
    sdlKeyNames.insert({"Backslash", K_BACKSLASH});
    sdlKeyNames.insert({"Rightbracket", K_RIGHTBRACKET});
    sdlKeyNames.insert({"Caret", K_CARET});
    sdlKeyNames.insert({"Underscore", K_UNDERSCORE});
    sdlKeyNames.insert({"Backquote", K_BACKQUOTE});
    sdlKeyNames.insert({"a", K_a});
    sdlKeyNames.insert({"b", K_b});
    sdlKeyNames.insert({"c", K_c});
    sdlKeyNames.insert({"d", K_d});
    sdlKeyNames.insert({"e", K_e});
    sdlKeyNames.insert({"f", K_f});
    sdlKeyNames.insert({"g", K_g});
    sdlKeyNames.insert({"h", K_h});
    sdlKeyNames.insert({"i", K_i});
    sdlKeyNames.insert({"j", K_j});
    sdlKeyNames.insert({"k", K_k});
    sdlKeyNames.insert({"l", K_l});
    sdlKeyNames.insert({"m", K_m});
    sdlKeyNames.insert({"n", K_n});
    sdlKeyNames.insert({"o", K_o});
    sdlKeyNames.insert({"p", K_p});
    sdlKeyNames.insert({"q", K_q});
    sdlKeyNames.insert({"r", K_r});
    sdlKeyNames.insert({"s", K_s});
    sdlKeyNames.insert({"t", K_t});
    sdlKeyNames.insert({"u", K_u});
    sdlKeyNames.insert({"v", K_v});
    sdlKeyNames.insert({"w", K_w});
    sdlKeyNames.insert({"x", K_x});
    sdlKeyNames.insert({"y", K_y});
    sdlKeyNames.insert({"z", K_z});
    sdlKeyNames.insert({"Delete", K_DELETE});
    sdlKeyNames.insert({"Numpad 0", K_KP0});
    sdlKeyNames.insert({"Numpad 1", K_KP1});
    sdlKeyNames.insert({"Numpad 2", K_KP2});
    sdlKeyNames.insert({"Numpad 3", K_KP3});
    sdlKeyNames.insert({"Numpad 4", K_KP4});
    sdlKeyNames.insert({"Numpad 5", K_KP5});
    sdlKeyNames.insert({"Numpad 6", K_KP6});
    sdlKeyNames.insert({"Numpad 7", K_KP7});
    sdlKeyNames.insert({"Numpad 8", K_KP8});
    sdlKeyNames.insert({"Numpad 9", K_KP9});
    sdlKeyNames.insert({"Numpad Period", K_KP_PERIOD});
    sdlKeyNames.insert({"Numpad Divide", K_KP_DIVIDE});
    sdlKeyNames.insert({"Numpad Multiply", K_KP_MULTIPLY});
    sdlKeyNames.insert({"Numpad Plus", K_KP_PLUS});
    sdlKeyNames.insert({"Numpad Minus", K_KP_MINUS});
    sdlKeyNames.insert({"Numpad Enter", K_KP_ENTER});
    sdlKeyNames.insert({"Numpad Equals", K_KP_EQUALS});
    sdlKeyNames.insert({"Up", K_UP});
    sdlKeyNames.insert({"Down", K_DOWN});
    sdlKeyNames.insert({"Right", K_RIGHT});
    sdlKeyNames.insert({"Left", K_LEFT});
    sdlKeyNames.insert({"Insert", K_INSERT});
    sdlKeyNames.insert({"Home", K_HOME});
    sdlKeyNames.insert({"End", K_END});
    sdlKeyNames.insert({"Page Up", K_PAGEUP});
    sdlKeyNames.insert({"Page Down", K_PAGEDOWN});
    sdlKeyNames.insert({"F1", K_F1});
    sdlKeyNames.insert({"F2", K_F2});
    sdlKeyNames.insert({"F3", K_F3});
    sdlKeyNames.insert({"F4", K_F4});
    sdlKeyNames.insert({"F5", K_F5});
    sdlKeyNames.insert({"F6", K_F6});
    sdlKeyNames.insert({"F7", K_F7});
    sdlKeyNames.insert({"F8", K_F8});
    sdlKeyNames.insert({"F9", K_F9});
    sdlKeyNames.insert({"F10", K_F10});
    sdlKeyNames.insert({"F11", K_F11});
    sdlKeyNames.insert({"F12", K_F12});
    sdlKeyNames.insert({"F13", K_F13});
    sdlKeyNames.insert({"F14", K_F14});
    sdlKeyNames.insert({"F15", K_F15});
    sdlKeyNames.insert({"Numlock", K_NUMLOCK});
    sdlKeyNames.insert({"Capslock", K_CAPSLOCK});
    sdlKeyNames.insert({"Scrollock", K_SCROLLOCK});
    sdlKeyNames.insert({"Rshift", K_RSHIFT});
    sdlKeyNames.insert({"Lshift", K_LSHIFT});
    sdlKeyNames.insert({"Rctrl", K_RCTRL});
    sdlKeyNames.insert({"Lctrl", K_LCTRL});
    sdlKeyNames.insert({"Ralt", K_RALT});
    sdlKeyNames.insert({"Lalt", K_LALT});
    sdlKeyNames.insert({"Rmeta", K_RMETA});
    sdlKeyNames.insert({"Lmeta", K_LMETA});
    sdlKeyNames.insert({"Lsuper", K_LSUPER});
    sdlKeyNames.insert({"Rsuper", K_RSUPER});
    sdlKeyNames.insert({"Mode", K_MODE});
    sdlKeyNames.insert({"Compose", K_COMPOSE});
    sdlKeyNames.insert({"Help", K_HELP});
    sdlKeyNames.insert({"Print", K_PRINT});
    sdlKeyNames.insert({"Sysreq", K_SYSREQ});
    sdlKeyNames.insert({"Break", K_BREAK});
    sdlKeyNames.insert({"Menu", K_MENU});
    sdlKeyNames.insert({"Power", K_POWER});
    sdlKeyNames.insert({"Euro", K_EURO});
    sdlKeyNames.insert({"Undo", K_UNDO});
}

bool KeyMap::loadKeyBindings(DFHack::color_ostream& out, const string& file)
{
    setSdlKeyNames();
    
    const std::vector<config_symbol> symbols = parse_config_file(file);
    out.color(COLOR_RED);
    if (symbols.empty())
    {
        out << "No interface key config symbols found." << endl;
        return false;
    }
    
    df::interface_key key = df::enums::interface_key::NONE;
    int32_t repeat = 0;
    (void)repeat;
    
    for (const config_symbol& symbol : symbols)
    {
        if (symbol.op == "BIND")
        {
            if (symbol.args.size() != 2)
            {
                out << "Failed to parse keybinding instruction " << symbol << endl;
                return false;
            }
            
            // interface key
            if (!find_enum_item(&key, symbol.args.at(0)))
            {
                out << "Unknown interface key " << symbol.args.at(1);
                return false;
            }
            
            // repeat
            if (symbol.args.at(1) == "REPEAT_NOT")
            {
                repeat = 0;
            }
            else if (symbol.args.at(1) == "REPEAT_SLOW")
            {
                repeat = 1;
            }
            else if (symbol.args.at(1) == "REPEAT_FAST")
            {
                repeat = 2;
            }
            else
            {
                out << "unknown flag " << symbol.args.at(1) << endl;
                return false;
            }
        }
        else if (symbol.op == "SYM")
        {
            if (symbol.args.size() != 2)
            {
                out << "Failed to parse keybinding instruction " << symbol
                    << " (wrong number of args)" << endl;
                return false;
            }
            
            KeyEvent ev;
            ev.type = EventType::type_key;
            ev.mod = std::stoi(symbol.args.at(0));
            
            auto iter = sdlKeyNames.find(symbol.args.at(1));
            if (iter == sdlKeyNames.end())
            {
                out << "Failed to find SDL key named \"" << symbol.args.at(1) << "\"" << endl;
                return false;
            }
            ev.key = iter->second;
            
            keymap.emplace(std::move(ev), key);
        }
        else if (symbol.op == "BUTTON")
        {
            // TODO
        }
        else if (symbol.op == "KEY")
        {
            if (symbol.args.size() != 1)
            {
                out << "Failed to parse keybinding instruction " << symbol
                    << " (wrong number of args)" << endl;
                return false;
            }
            
            KeyEvent ev;
            ev.type = EventType::type_unicode;
            
            // TODO unicode
            std::string charname = symbol.args.at(0);
            ev.unicode = utf8_decode(charname);
            
            keymap.emplace(std::move(ev), key);
        }
        else
        {
            out << "Unknown operator (first entry) in " << symbol << endl;
            return false;
        }
    }
    
    out.color(COLOR_RESET);
    return true;
}

std::string KeyMap::getCommandNames(const std::set<df::interface_key>& keys)
{
    std::stringstream ss;
    ss << "{";
    bool first = true;
    for (const df::interface_key& key : keys)
    {
        if (!first)
        {
            ss << ", ";
        }
        first = false;
        ss << getCommandName(key);
    }
    ss << "}";
    if (ss.str().length() >= 256 && keys.size() > 1)
    {
        return "{" + std::to_string(keys.size()) + " keys...}";
    }
    return ss.str();
}

std::string KeyMap::getCommandName(df::interface_key key)
{
    return DFHack::enum_item_key(key);
}

std::set<df::interface_key> KeyMap::toInterfaceKey(const KeyEvent & match){
    std::set<df::interface_key> bindings;
    
    if (match.interface_keys.get())
    {
        bindings.insert(match.interface_keys.get()->begin(), match.interface_keys.get()->end());
    }
    
    std::pair<std::multimap<KeyEvent,df::interface_key>::iterator,std::multimap<KeyEvent,df::interface_key>::iterator> its;

    for (its = keymap.equal_range(match); its.first != its.second; ++its.first)
        bindings.insert((its.first)->second);

    return bindings;
}

KeyEvent& KeyEvent::operator=(const KeyEvent& other)
{
    type = other.type;
    mod = other.mod;
    unicode = other.unicode;
    key = other.key;
    button = other.button;
    if (other.interface_keys)
    {
        interface_keys.reset(new std::set<df::interface_key>(
            *other.interface_keys.get()
        ));
    }
    return *this;
}

std::ostream& operator<<(std::ostream& a, const KeyEvent& match)
{
    a << "KeyEvent {";
    switch (match.type)
    {
    case EventType::type_unicode:
        a << "unicode ";
        a << (int)match.unicode;
        break;
    case EventType::type_key:
        a << "key ";
        a << (int)match.key;
        if (match.unicode){
            a<<", unicode " << (int)match.unicode;
        }
        a << ", mod " << (int)match.mod;
        break;
    case EventType::type_button:
        a << "button ";
        a << (int)match.button;
        break;
    case EventType::type_interface:
        a << "interface_keys:";
        if (match.interface_keys)
        {
            for (const df::interface_key& key : *match.interface_keys.get())
            {
                a << " " << enum_item_key(key);
            }
        }
        break;
    }
    a << "}";
    return a;
}

KeyMap keybindings;