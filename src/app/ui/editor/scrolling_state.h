/* Aseprite
 * Copyright (C) 2001-2013  David Capello
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef APP_UI_EDITOR_SCROLLING_STATE_H_INCLUDED
#define APP_UI_EDITOR_SCROLLING_STATE_H_INCLUDED

#include "app/ui/editor/editor_state.h"
#include "base/compiler_specific.h"

namespace app {

  class ScrollingState : public EditorState {
  public:
    ScrollingState();
    virtual ~ScrollingState();
    virtual bool isTemporalState() const OVERRIDE { return true; }
    virtual bool onMouseDown(Editor* editor, ui::MouseMessage* msg) OVERRIDE;
    virtual bool onMouseUp(Editor* editor, ui::MouseMessage* msg) OVERRIDE;
    virtual bool onMouseMove(Editor* editor, ui::MouseMessage* msg) OVERRIDE;
    virtual bool onMouseWheel(Editor* editor, ui::MouseMessage* msg) OVERRIDE;
    virtual bool onSetCursor(Editor* editor) OVERRIDE;
    virtual bool onKeyDown(Editor* editor, ui::KeyMessage* msg) OVERRIDE;
    virtual bool onKeyUp(Editor* editor, ui::KeyMessage* msg) OVERRIDE;
    virtual bool onUpdateStatusBar(Editor* editor) OVERRIDE;
  };

} // namespace app

#endif  // APP_UI_EDITOR_SCROLLING_STATE_H_INCLUDED
