/*
 * Dwarfplex is based on Webfort, created by Vitaly Pronkin on 14/05/14.
 * Copyright (c) 2014 mifki, ISC license.
 * Copyright (c) 2020 white-rabbit, ISC license
 */
 
#include "dfplex.hpp"

#include <stdint.h>
#include <iostream>
#include <map>
#include <vector>
#include <list>
#include <cassert>

#include "tinythread.h"

#include "MemAccess.h"
#include "PluginManager.h"
#include "modules/EventManager.h"
#include "modules/MapCache.h"
#include "modules/Gui.h"
#include "modules/World.h"
#include "modules/Screen.h"
#include "modules/Renderer.h"
#include "df/graphic.h"
#include "df/enabler.h"
#include "df/renderer.h"
#include "df/announcements.h"
#include "df/announcement_flags.h"
#include "df/building.h"
#include "df/buildings_other_id.h"
#include "df/d_init.h"
#include "df/unit.h"
#include "df/ui_sidebar_menus.h"
#include "df/ui_unit_view_mode.h"
#include "df/items_other_id.h"
#include "df/report.h"
#include "df/ui_build_selector.h"
#include "df/viewscreen.h"
#include "df/viewscreen_dwarfmodest.h"
#include "df/viewscreen_createquotast.h"
#include "df/viewscreen_customize_unitst.h"
#include "df/viewscreen_loadgamest.h"
#include "df/viewscreen_meetingst.h"
#include "df/viewscreen_movieplayerst.h"
#include "df/viewscreen_optionst.h"
#include "df/viewscreen_textviewerst.h"
#include "df/viewscreen_titlest.h"
#include "df/viewscreen_unitst.h"
#include "df/world.h"

#include "Client.hpp"
#include "server.hpp"
#include "input.hpp"
#include "command.hpp"
#include "hackutil.hpp"
#include "keymap.hpp"
#include "config.hpp"
#include "dfplex.hpp"

using namespace DFHack;
using namespace df::enums;
using df::global::world;
using std::string;
using std::vector;
using df::global::gps;
using df::global::init;

using df::global::enabler;
using df::renderer;

screenbuf_t screenbuf;
static std::vector<std::pair<uint8_t, std::string>> header_status; // messages shown at top of screen to replace *PAUSED*. colour, string
static const size_t K_BEVEL = 1;

// retrieves cached screenbuf's tile.
ClientTile& screentile(int x, int y)
{
    // returned for safety in some circumstances.
    static ClientTile dummy;
    
    if (x < 0 || y < 0 || x >= gps->dimx || y >= gps->dimy)
    {
        return dummy;
    }
    
    size_t index = x * gps->dimy + y;
    if (index >= sizeof(screenbuf_t) / sizeof(ClientTile))
    {
        return dummy;
    }
    
    return screenbuf[index];
}

void write_to_screen(int x, int y, std::string s, uint8_t fg=7, uint8_t bg=0, bool bold=true)
{
    for (size_t i = 0; i < s.length(); ++i)
    {
        if (static_cast<int>(x + i) < gps->dimx && y < gps->dimy && static_cast<int>(x + i) >= 0 && y >= 0)
        {
            ClientTile& tile = screentile(x + i, y);
            tile.pen.ch = s.at(i);
            tile.pen.bold = bold;
            tile.pen.bg = bg;
            tile.pen.fg = fg;
            tile.is_map = false;
            tile.is_text = true;
        }
    }
}

inline bool on_screen(const Coord& c)
{
    return c.z == 0 && c.x >= 0 && c.y >= 0 && c.x < gps->dimx && c.y < gps->dimy;
}

// scrapes some info from the screenbuffer
// (only because it's not clear how to pull this info from DF directly.)
void scrape_screenbuf(Client* cl)
{
    UIState& ui = cl->ui;
    ui.m_construction_plan.clear();
    
    int32_t vx, vy, vz;
    Gui::getViewCoords(vx, vy, vz);
    
    // cursor position on the screen
    int32_t cx = cl->ui.m_cursorcoord.x - vx + K_BEVEL;
    int32_t cy = cl->ui.m_cursorcoord.y - vy + K_BEVEL;
    int32_t cz = cl->ui.m_cursorcoord.z - vz;
    Coord cursor = { cx, cy, cz };
    
    // floodfill Xs to find construction plan
    if (df::global::ui->main.mode == df::ui_sidebar_mode::Build)
    if
    (
        get_current_menu_id() == "dwarfmode/Build/Material/Groups"
        || startsWith(get_current_menu_id(), "dwarfmode/Build/Position")
    )
    {
        std::vector<Coord> frontier{ {cx, cy, cz} };
        // FIXME(debug): for some reason, std::set<Coord> behaves incoorectly,
        // even with a comparator and operator< defined.
        std::set<std::tuple<int32_t, int32_t, int32_t>> visited;
        
        while (!frontier.empty())
        {
            Coord c = frontier.back();
            frontier.pop_back();
            if (contains(visited,std::tuple<int32_t, int32_t, int32_t>{c.x, c.y, c.z}))
            {
                continue;
            }
            visited.insert(std::tuple<int32_t, int32_t, int32_t>{c.x, c.y, c.z});
            if (on_screen(c))
            {
                auto& tile = screentile(c.x, c.y);
                if (tile.is_map && tile.pen.ch == 'X')
                {
                    uint8_t col = pen_colour(tile.pen);
                    if (col == 0x05 || col == 0x44 || col == 0x02
                        || col == 0x45 || col == 0x42)
                    {
                        ui.m_construction_plan.emplace_back(
                            pen_colour(tile.pen),
                            c - cursor
                        );
                        
                        frontier.emplace_back(
                            c.x - 1, c.y, c.z
                        );
                        frontier.emplace_back(
                            c.x + 1, c.y, c.z
                        );
                        frontier.emplace_back(
                            c.x, c.y - 1, c.z
                        );
                        frontier.emplace_back(
                            c.x, c.y + 1, c.z
                        );
                    }
                }
            }
        }
    }
}

// checks if the client's cursor is at the given coordinates on the screen.
bool screenbuf_cursor(Client* cl, int32_t x, int32_t y, int32_t z)
{
    if (!cl->ui.m_cursorcoord_set) return false;
    
    // TODO: some more logic to check if cursor isn't visible.
    // (On some screens, the cursorcoord is set but not drawn)
    
    if (!on_screen({x, y, z})) return false;
    
    const auto& tile = screentile(x, y);
    if (!tile.is_map) return false;
    
    int32_t vx, vy, vz;
    Gui::getViewCoords(vx, vy, vz);
    
    int32_t my_cx = cl->ui.m_cursorcoord.x - vx + K_BEVEL;
    int32_t my_cy = cl->ui.m_cursorcoord.y - vy + K_BEVEL;
    int32_t my_cz = cl->ui.m_cursorcoord.z - vz;
    
    return (x == my_cx && y == my_cy && z == my_cz);
}

// sets status message at the top of the screen
static void set_status()
{
    header_status.clear();
    std::string playermsg = std::to_string(get_client_count()) + "P";
    header_status.emplace_back(64 | (3 << 3) | 7, playermsg);
    // TODO: read game state for this.
    // Note that vanilla DF has a SIEGE status message, so we should too.
    bool siege = is_siege();
    if (global_pause)
    {
    	uint8_t col = 64 | (2 << 3) | 3;
    	if (siege)
    	{
    	    col = 64 | (4 << 3) | 5;
    	}
        header_status.emplace_back(col,"*PAUSED*");
    }
    else if (is_paused() && !global_pause)
    {
	    uint8_t col = 64 | (6 << 3) | 6;
        if (siege)
    	{
    	    col = 64 | (4 << 3) | 6;
    	}
        header_status.emplace_back(col,"*PENDING*");
    }
    if (siege)
    {
	    header_status.emplace_back(64 | (4 << 3) | 6, " SIEGE ");
    }
    
    // add 5 blank tiles to hide what's behind
    header_status.emplace_back(64, std::string(5, (char)219));
}

std::string key_display_name(int32_t key)
{
    if (key == ' ')
    {
        return "Space";
    }
    if (key >= 0x20 && key < 0x7f)
    {
        return std::string(1, static_cast<char>(key));
    }
    else
    {
        return "?";
    }
}

std::string blink_cursor()
{
	unsigned char cursor = 219;
	
    if ((frames_elapsed / 24) % 2)
        return std::string(1, cursor);
    else
        return std::string(1, '_');
}

bool blank_sidebar()
{
    DFHack::Gui::DwarfmodeDims dims = Gui::getDwarfmodeViewDims();
    if (dims.menu_on)
    {
        for (int x = dims.menu_x1; x < dims.menu_x2; ++x)
        {
            for (int y = dims.y1; y < dims.y2; ++y)
            {
                write_to_screen(x, y, " ", 0, 0, 0);
            }
        }
    }
    
    return dims.menu_on;
}

void modify_screenbuf(Client* cl)
{
    // modifications only occur in dwarfmode
    if (!is_dwarf_mode()) return;
    
    if (gps->dimx < 2 || gps->dimy < 2) return;
    
    df::viewscreen* vs;
    virtual_identity* id;
    UPDATE_VS(vs, id);
    
    DFHack::Gui::DwarfmodeDims dims = Gui::getDwarfmodeViewDims();
    
    // menu options (custom keys)
    if (id == &df::viewscreen_dwarfmodest::_identity &&
        df::global::ui->main.mode == df::enums::ui_sidebar_mode::Default)
    {
        if (dims.menu_on)
        {
            const int menu_additions_x = dims.menu_x1 + 1;
            const int menu_additions_y = dims.y1 + 22;
            // amount of space
            
            const int ymax = gps->dimy - 2;
            int x = menu_additions_x;
            int y = menu_additions_y + 1;
            for (int32_t i = 0; i < 5 && y < ymax; ++i)
            {
                switch (i)
                {
                case 0:
                    // uniplex
                    if (MULTIPLEXKEY)
                    {
                        std::string keystr = key_display_name(MULTIPLEXKEY);
                        write_to_screen(x, y, keystr, 4, 0, 1);
                        
                        write_to_screen(x + keystr.length(), y, 
                            (plexing)
                                ? ": Disable Multiplex"
                                : ": Enable Multiplex",
                            7, 0, 0
                        );
                        
                        ++y;
                    }
                    break;
                case 1:
                    // debug view key
                    if (!plexing) break;
                    if (DEBUGKEY)
                    {
                        std::string keystr = key_display_name(DEBUGKEY);
                        write_to_screen(x, y, keystr, 4, 0, 1);
                        
                        write_to_screen(
                            x + keystr.length(), y, 
                            ": Debug Info",
                            7, 0, 0
                        );
                        
                        ++y;
                    }
                    break;
                case 2:
                    // camera switch
                    if (!plexing) break;
                    if (NEXT_CLIENT_POS_KEY || PREV_CLIENT_POS_KEY)
                    {
                        std::string s = "";
                        if (PREV_CLIENT_POS_KEY)
                        {
                            s += key_display_name(PREV_CLIENT_POS_KEY);
                        }
                        if (NEXT_CLIENT_POS_KEY)
                        {
                            s += key_display_name(NEXT_CLIENT_POS_KEY);
                        }
                    
                        write_to_screen(x, y, s, 4, 0, 1);
                        
                        write_to_screen(x + s.length(), y, 
                            (cl->ui.m_stored_camera_return)
                                ? ": Return to last Position"
                                : ": Follow Client",
                            7, 0, 0
                        );
                        
                        ++y;
                        
                        if (!cl->ui.m_stored_camera_return)
                        {
                            std::string follow_name = "";
                            uint8_t follow_colour = 7;
                            bool follow_bold = false;
                            if (cl->ui.m_following_client && cl->ui.m_client_screen_cycle.get() && cl->ui.m_client_screen_cycle != cl->id)
                            {
                                Client* spectatee = get_client(cl->ui.m_client_screen_cycle.get());
                                follow_name = cl->ui.m_client_screen_cycle->nick;
                                follow_colour = cl->ui.m_client_screen_cycle->nick_colour;
                                if (!follow_name.length())
                                {
                                    follow_name = "(Anonymous)";
                                    // don't tell anyone about this.
                                    follow_colour = 1 +
                                        reinterpret_cast<uintptr_t>(cl->ui.m_client_screen_cycle.get()) % 7;
                                }
                                else
                                {
                                    follow_name = "\"" + follow_name + "\"";
                                    follow_bold = true;
                                }
                                
                                // triangles flanking name
                                // open if the player is following someone else, closed otherwise.
                                if (spectatee && spectatee->ui.m_following_client)
                                {
                                    follow_name = ">" + follow_name + "<";
                                }
                                else
                                {
                                    follow_name = std::string(1, 16) + follow_name + std::string(1,17);
                                }
                            }
                            write_to_screen((dims.menu_x2 + dims.menu_x1) / 2 - follow_name.length() / 2, y, 
                                follow_name,
                                follow_colour, 0, follow_bold
                            );
                        }
                        
                        ++y;
                    }
                    break;
                case 3:
                    // chat name key
                    if (!plexing) break;
                    if (CHAT_NAME_KEY)
                    {
                        std::string s = key_display_name(CHAT_NAME_KEY);
                    
                        write_to_screen(x, y, s, 4, 0, 1);
                        
                        write_to_screen(x + s.length(), y, 
                            ": User Config",
                            7, 0, 0
                        );
                        
                        ++y;
                    }
                    break;
                case 4:
                    // chat key
                    if (!plexing) break;
                    if (CHAT_ENABLED && CHATKEY)
                    {
                        if (!CHAT_NAME_REQUIRED || cl->id->nick.length())
                        {
                            std::string s = key_display_name(CHATKEY);
                            
                            write_to_screen(x, y, s, 4, 0, 1);
                            
                            write_to_screen(x + s.length(), y, 
                                ": Send Chat Message",
                                7, 0, 0
                            );
                        }
                        else
                        {
                            write_to_screen(x, y, 
                                "(Set username to chat)",
                                7, 0, 0
                            );
                        }
                        
                        ++y;
                    }
                    break;
                }
            }
            
            if (cl->ui.m_dfplex_chat_config)
            {
                blank_sidebar();
                
                int x = dims.menu_x1 + 1;
                int y = dims.y1 + 1;
                
                write_to_screen(x, y, "N", 4, 0, 1);
                write_to_screen(x + 1, y, 
                    ": Set Username",
                    7, 0, 0
                );
                ++y;
                std::string uname_s = cl->id->nick;
                if (cl->ui.m_dfplex_chat_name_entering)
                {
                    uname_s += blink_cursor();
                }
                write_to_screen(x, y, 
                    uname_s,
                    cl->id->nick_colour, 0, 1
                );
                
                y+= 2;
                write_to_screen(x + 8, y, 
                    ": Set Colour",
                    7, 0, 0
                );
                for (int32_t i = 0; i < 8; ++i)
                {
                    write_to_screen(x + i, y, 
                        std::string(1, '0' + i),
                        i, (i == cl->id->nick_colour) ? 7 : 0, 1
                    );
                }
                
                y += 2;
                
                if (CHAT_ENABLED)
                {
                    write_to_screen(x, y, "H", 4, 0, 1);
                    write_to_screen(x + 1, y, 
                        (cl->ui.m_dfplex_hide_chat) ? ": Show Chat" : ": Hide Chat",
                        7, 0, 0
                    );
                    y += 1;
                }
            }
        }
    }
    
    // remaining modifications only occur in multiplex mode.
    if (!plexing) return;

    // header edit.
	size_t header_index = 0;
	for (auto& pair : header_status)
	{
        auto& col = pair.first;
        auto& status = pair.second;
        for (char c : status)
        {
            auto& io_tile = screentile(header_index + 1, 0);
		    io_tile.pen.ch = c;
		    io_tile.pen.fg = col & 7;
		    io_tile.pen.bg = (col >> 3) & 7;
		    io_tile.pen.bold = !!(col & 64);
            ++header_index;
        }
	}

    int32_t vx, vy, vz;
    Gui::getViewCoords(vx, vy, vz);
    
    if (id == &df::viewscreen_dwarfmodest::_identity)
    {
        // chat messages
        if (CHAT_ENABLED)
        {
            int32_t x = 1;
            int32_t y = gps->dimy - 1;
            const uint32_t width = CHAT_WIDTH;
            int32_t top = gps->dimy - 1 - CHAT_HEIGHT;
            bool hide_expired = true;
            
            bool hide_all = (cl->ui.m_dfplex_hide_chat);
            if (cl->ui.m_dfplex_chat_entering)
            {
                top = 1;
                hide_expired = false;
                hide_all = false;
            }
            
            if (cl->ui.m_dfplex_chat_entering)
            {
                std::stringstream ss;
                ss << cl->ui.m_dfplex_chat_message;
                    
                std::vector<std::string> lines = word_wrap_lines(ss.str(), width);
                
                if (lines.empty()) lines.emplace_back();
                lines.back() += blink_cursor();
                
                y -= lines.size();
                
                uint8_t fgcol = 0;
                if (lines.size() > CHAT_MESSAGE_LINES)
                {
                    fgcol = 4;
                }
                
                for (size_t i = 0; i < lines.size(); ++i)
                {
                    write_to_screen(x, y + i, lines.at(i), fgcol, 0, 1);
                }
                
                if (cl->id->nick.length())
                {
                    --y;
                    write_to_screen(x, y, cl->id->nick + ":", cl->id->nick_colour, 0, 1);
                }
            }
            
            // show other messages
            for (size_t i = g_chatlog.m_messages.size(); i --> 0;)
            {
                if (hide_all) break;
                
                ChatMessage& message = g_chatlog.m_messages.at(i);
                
                if (hide_expired && message.is_expired(cl)) break;
                
                const bool flash = message.is_flash(cl);
                
                std::vector<std::string> lines = word_wrap_lines(message.m_contents, width);
                y -= lines.size() + 1;
                
                if (y < top)
                {
                    message.expire(cl);
                    
                    // early out
                    if (i < g_chatlog.m_active_message_index) break;
                }
                else
                {
                    for (size_t i = 0; i < lines.size(); ++i)
                    {
                        write_to_screen(x, y + i, lines.at(i), (flash ? 7 : 0), 0, 1);
                    }
                }
                
                if (message.m_sender->nick.length())
                {
                    --y;
                    write_to_screen(x, y, message.m_sender->nick + ":", message.m_sender->nick_colour, 0, 1);
                }
            }
        }
        
        // cursors
        int32_t my_cx = cl->ui.m_cursorcoord.x - vx + K_BEVEL;
        int32_t my_cy = cl->ui.m_cursorcoord.y - vy + K_BEVEL;
        int32_t my_cz = cl->ui.m_cursorcoord.z - vz;
        
        for (size_t i = 0; i < get_client_count(); ++i)
        {
            Client* client;
            client = get_client(i);
            if (client == cl) continue;
            
            // find cursor position.
            UIState& ui = client->ui;
            if (ui.m_cursorcoord_set && ui.m_cursorcoord.x >= 0 && ui.m_cursorcoord.y >= 0)
            {
                // convert global to local coords
                int32_t cx = ui.m_cursorcoord.x - vx + K_BEVEL;
                int32_t cy = ui.m_cursorcoord.y - vy + K_BEVEL;
                int32_t cz = ui.m_cursorcoord.z - vz;
                
                // designation coordinate
                int32_t dx = -1, dy = -1, dz = 0;
                if (ui.m_designationcoord_share && ui.m_designationcoord_set && ui.m_designationcoord.x >= 0 && ui.m_designationcoord.y >= 0)
                {
                    dx = ui.m_designationcoord.x - vx + K_BEVEL;
                    dy = ui.m_designationcoord.y - vy + K_BEVEL;
                    dz = ui.m_designationcoord.z - vz;
                }
                else if (ui.m_squadcoord_share && ui.m_squadcoord_start_set && ui.m_squadcoord_start.x >= 0 && ui.m_squadcoord_start.y >= 0)
                {
                    dx = ui.m_squadcoord_start.x - vx + K_BEVEL;
                    dy = ui.m_squadcoord_start.y - vy + K_BEVEL;
                    dz = ui.m_squadcoord_start.z - vz;
                }
                else if (ui.m_burrowcoord_share && ui.m_burrowcoord_set && ui.m_burrowcoord.x >= 0 && ui.m_burrowcoord.y >= 0)
                {
                    dx = ui.m_burrowcoord.x - vx + K_BEVEL;
                    dy = ui.m_burrowcoord.y - vy + K_BEVEL;
                    dz = ui.m_burrowcoord.z - vz;
                }
                
                if (ui.m_construction_plan.empty())
                {
                    if (on_screen({cx, cy, cz}))
                    {
                        auto& io_tile = screentile(cx, cy);
                        if (io_tile.is_map && !screenbuf_cursor(cl, cx, cy, cz))
                        {
                            io_tile.pen.fg = 1; io_tile.pen.bg = 0; io_tile.pen.bold = true;
                            io_tile.pen.ch = 'X';
            		        if (CURSOR_IS_TEXT) io_tile.is_text = true;
                        }
                    }
                    
                    // flicker
                    if ((frames_elapsed / 12) % 2)
                    {
                        // designation rectangle
                        for (int32_t tx = 0; tx < gps->dimx; ++tx)
                        {
                            for (int32_t ty = 0; ty < gps->dimy; ++ty)
                            {
                                auto& io_tile = screentile(tx, ty);
                                if (io_tile.is_map)
                                {
                                    int32_t tz = 0;
                                    if (dx >= 0 && dy >= 0 &&
                                        in_range(tx, dx, cx) && in_range(ty, dy, cy) && in_range(tz, dz, cz))
                                    {
                                        if (!screenbuf_cursor(cl, cx, cy, cz))
                            		    {
                                            io_tile.pen.fg = 1; io_tile.pen.bg = 0; io_tile.pen.bold = true;
                                            io_tile.pen.ch = '+';
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                else
                {
                    #ifdef DFPLEX_CONSTUCTION_FLICKER
                    // flicker
                    if ((frames_elapsed / 12) % 2)
                    #endif
                    {
                        // display construction / build plan
                        for (const auto& pair : ui.m_construction_plan)
                        {
                            Coord bcoord = Coord{ cx, cy, cz } + pair.second;
                            uint8_t colour = pair.first;
                            if (on_screen(bcoord))
                            {
                                auto& tile = screentile(bcoord.x, bcoord.y);
                                if (tile.is_map && !screenbuf_cursor(cl, bcoord.x, bcoord.y, bcoord.z))
                                {
                                    // remap colour
                                    if (colour == 0x02) colour = 0x01;
                                    if (colour == 0x42) colour = 0x41;
                                    if (colour == 0x45) colour = 0x43;
                                    if (colour == 0x05) colour = 0x03;
                                    if (colour == 0x44) colour = 0x04;
                                    
                                    set_pen_colour(tile.pen, colour);
                                    tile.pen.ch = 'X';
                                }
                            }
                        }
                    }
                }
    	    }
        }
    }
}

void transfer_screenbuf_client(Client* client)
{
    if (client)
    {
        // status message in the dwarf mode screen border, e.g. *PAUSED*
        set_status();
        
        modify_screenbuf(client);
        
        size_t count = std::min(std::max(0, gps->dimx * gps->dimy), 256 * 256);
        if (gps->dimx != client->dimx || gps->dimy != client->dimy)
        {
            // refresh client's screen
            memset(client->sc, 0, sizeof(client->sc));
            client->dimx = gps->dimx;
            client->dimy = gps->dimy;
        }
        for (size_t i = 0; i < count; ++i)
        {
            ClientTile ct = screenbuf[i];
            
            // compare to see if the tile has changed
            if (client->sc[i] != ct)
            {
                // copy the tile to the client.
                client->sc[i] = ct;
                assert(client->sc[i].modified);
            }
        }
    }
}

void transfer_screenbuf_to_all()
{
    // copy to everybody
    size_t i = 0;
    for (Client* client = get_client(i); client; i++, client = get_client(i))
    {
        transfer_screenbuf_client(client);
    }
}

static void update_tilebuf(int x, int y)
{
    if (x >= gps->dimx || y >= gps->dimy) return;
    assert(0 <= x && x < gps->dimx);
    assert(0 <= y && y < gps->dimy);
    const int tile = x * gps->dimy + y;
    if (tile >= 256 * 256) return;
    
    // FIXME: is true the right thing to pass as the 3rd argument here?
    Screen::Pen pen = Screen::readTile(x, y, true);

    bool is_map, is_overworld, is_text;
    is_text = is_text_tile(x, y, is_map, is_overworld);
    
    screenbuf[tile] = ClientTile{ pen, 1, is_text, is_overworld, is_map };
}

void read_screenbuf_tiles()
{
    assert(!!gps);
    assert(!!df::global::enabler);

    for (int32_t x = 0; x < gps->dimx; x++) {
        for (int32_t y = 0; y < gps->dimy; y++) {
            update_tilebuf(x, y);
        }
    }
}

// no longer used
void hook_renderer()
{ }

void unhook_renderer()
{ }

static int _dimy = 1000;

static void paranoid_resize(int32_t x, int32_t y)
{
    Screen::invalidate();
    int32_t _x = std::min(std::max(x, 80), 255);
    int32_t _y = std::min(std::max(y, 25), 255);
    enabler->renderer->grid_resize(_x, _y);
    assert(gps->dimx == _x);
    assert(gps->dimy == _y);
    Screen::invalidate();
}

static int32_t prev_dimx=0, prev_dimy=0;

void set_size(int32_t w, int32_t h)
{
    // swap out screen for our own.
    prev_dimx = gps->dimx;
    prev_dimy = gps->dimy;
    
    paranoid_resize(w, h);
}

void perform_render()
{
    df::viewscreen* vs;
    virtual_identity* id;
    UPDATE_VS(vs, id);
    (void)id;
    // draw the screen
    vs->render();
    
    // read the screen
    read_screenbuf_tiles();
}

void restore_size()
{
    // restore screen size to how it was before perform_render()
    paranoid_resize(prev_dimx, prev_dimy);
}