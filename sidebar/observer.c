/**
 * @file
 * Sidebar observers
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @page sidebar_observers Sidebar observers
 *
 * Sidebar observers
 */

#include "config.h"
#include <stdbool.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "mutt_globals.h"
#include "mutt_menu.h"

/**
 * calc_divider - Decide what actions are required for the divider
 * @param wdata   Sidebar data
 * @param ascii   true, if `$ascii_chars` is set
 * @param div_str Divider string, `$sidebar_divider_char`
 * @retval num Action required, e.g. #WA_REPAINT
 *
 * If the divider changes width, then Window will need to be reflowed.
 */
static WindowActionFlags calc_divider(struct SidebarWindowData *wdata, bool ascii, const char *div_str)
{
  // Calculate the width of the delimiter in screen cells
  int width = mutt_strwidth(C_SidebarDividerChar);
  if (width < 1) // Bad character or empty
  {
    width = 1;
  }
  else if (ascii)
  {
    for (int i = 0; i < width; i++)
    {
      if (div_str[i] & ~0x7F) // high-bit is set
      {
        width = 1;
        break;
      }
    }
  }

  WindowActionFlags action = WA_REPAINT;
  if (width != wdata->divider_width)
    action = WA_REFLOW;

  wdata->divider_width = width;
  return action;
}

/**
 * sb_color_event - Color has changed
 * @param nc Notification
 */
static void sb_color_event(struct NotifyCallback *nc, struct MuttWindow *win)
{
  enum ColorId color = nc->event_subtype;

  switch (color)
  {
    case MT_COLOR_INDICATOR:
    case MT_COLOR_NORMAL:
    case MT_COLOR_SIDEBAR_DIVIDER:
    case MT_COLOR_SIDEBAR_FLAGGED:
    case MT_COLOR_SIDEBAR_HIGHLIGHT:
    case MT_COLOR_SIDEBAR_INDICATOR:
    case MT_COLOR_SIDEBAR_NEW:
    case MT_COLOR_SIDEBAR_ORDINARY:
    case MT_COLOR_SIDEBAR_SPOOLFILE:
    case MT_COLOR_SIDEBAR_UNREAD:
      mutt_debug(LL_NOTIFY, "color\n");
      win->actions |= WA_REPAINT;
      break;

    default:
      break;
  }
}

/**
 * sb_command_event - Command has changed
 * @param nc Notification
 */
static void sb_command_event(struct NotifyCallback *nc, struct MuttWindow *win)
{
  struct Command *cmd = nc->event_data;

  if ((cmd->parse != sb_parse_whitelist) && (cmd->parse != sb_parse_unwhitelist))
    return;

  mutt_debug(LL_NOTIFY, "command\n");
  win->actions |= WA_RECALC;
}

/**
 * sb_config_event - Config has changed
 * @param nc Notification
 */
static void sb_config_event(struct NotifyCallback *nc, struct MuttWindow *win)
{
  if (nc->event_subtype == NT_CONFIG_INITIAL_SET)
    return;

  struct EventConfig *ec = nc->event_data;

  if (!mutt_strn_equal(ec->name, "sidebar_", 8) && !mutt_str_equal(ec->name, "ascii_chars") &&
      !mutt_str_equal(ec->name, "folder") && !mutt_str_equal(ec->name, "spoolfile"))
  {
    return;
  }

  mutt_debug(LL_NOTIFY, "config\n");

  if (mutt_str_equal(ec->name, "sidebar_next_new_wrap"))
    return; // Affects the behaviour, but not the display

  if (mutt_str_equal(ec->name, "sidebar_visible"))
  {
    window_set_visible(win, C_SidebarVisible);
    win->parent->actions |= WA_REFLOW;
    return;
  }

  if (mutt_str_equal(ec->name, "sidebar_width"))
  {
    win->req_cols = C_SidebarWidth;
    win->parent->actions |= WA_REFLOW;
    return;
  }

  if (mutt_str_equal(ec->name, "sidebar_on_right") ||
      mutt_str_equal(ec->name, "spoolfile"))
  {
    win->actions |= WA_REPAINT;
    return;
  }

  if (mutt_str_equal(ec->name, "sidebar_divider_char") ||
      mutt_str_equal(ec->name, "ascii_chars"))
  {
    struct SidebarWindowData *wdata = sb_wdata_get(win);
    WindowActionFlags action = calc_divider(wdata, C_AsciiChars, C_SidebarDividerChar);
    if (action == WA_REFLOW)
      win->parent->actions |= WA_REFLOW;
    else
      win->actions |= action;
    return;
  }

  // All the remaining config changes...
  win->actions |= WA_RECALC;
}

/**
 * sb_window_event - Window has changed
 * @param nc Notification
 */
static void sb_window_event(struct NotifyCallback *nc, struct MuttWindow *win)
{
  if (nc->event_subtype == NT_WINDOW_FOCUS)
  {
    if (!mutt_window_is_visible(win))
      return;

    mutt_debug(LL_NOTIFY, "focus\n");
    win->actions |= WA_RECALC;
  }
  else if (nc->event_subtype == NT_WINDOW_DELETE)
  {
    struct EventWindow *ew = nc->event_data;
    if (ew->win != win)
      return;

    mutt_debug(LL_NOTIFY, "delete\n");
    notify_observer_remove(nc->current, sb_observer, win);
  }
}

/**
 * sb_observer - Listen for Events affecting the Sidebar Window - Implements ::observer_t
 * @param nc Notification data
 * @retval bool True, if successful
 */
int sb_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;

  struct MuttWindow *win = nc->global_data;

  switch (nc->event_type)
  {
    case NT_COLOR:
      sb_color_event(nc, win);
      break;
    case NT_COMMAND:
      sb_command_event(nc, win);
      break;
    case NT_CONFIG:
      sb_config_event(nc, win);
      break;
    case NT_MAILBOX:
      win->actions |= WA_RECALC;
      break;
    case NT_WINDOW:
      sb_window_event(nc, win);
      break;
    default:
      break;
  }

  return 0;
}

/**
 * sb_insertion_observer - Listen for new Dialogs - Implements ::observer_t
 */
int sb_insertion_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_WINDOW) || (nc->event_subtype != NT_WINDOW_DIALOG))
    return 0;

  struct EventWindow *ew = nc->event_data;
  if (ew->win->type != WT_DLG_INDEX)
    return 0;

  if (ew->flags & WN_VISIBLE)
    sb_win_init(ew->win);
  else if (ew->flags & WN_HIDDEN)
    sb_win_shutdown(ew->win);

  return 0;
}
