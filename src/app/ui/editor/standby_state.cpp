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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/ui/editor/standby_state.h"

#include "app/app.h"
#include "app/commands/commands.h"
#include "app/commands/params.h"
#include "app/document_location.h"
#include "app/ini_file.h"
#include "app/tools/ink.h"
#include "app/tools/tool.h"
#include "app/ui/color_bar.h"
#include "app/ui/editor/drawing_state.h"
#include "app/ui/editor/editor.h"
#include "app/ui/editor/editor_customization_delegate.h"
#include "app/ui/editor/handle_type.h"
#include "app/ui/editor/moving_cel_state.h"
#include "app/ui/editor/moving_pixels_state.h"
#include "app/ui/editor/pixels_movement.h"
#include "app/ui/editor/scrolling_state.h"
#include "app/ui/editor/tool_loop_impl.h"
#include "app/ui/editor/transform_handles.h"
#include "app/ui/status_bar.h"
#include "app/ui_context.h"
#include "app/util/misc.h"
#include "gfx/rect.h"
#include "raster/layer.h"
#include "raster/mask.h"
#include "raster/sprite.h"
#include "ui/alert.h"
#include "ui/message.h"
#include "ui/system.h"
#include "ui/view.h"

#include <allegro.h>

namespace app {

using namespace ui;

enum WHEEL_ACTION { WHEEL_NONE,
                    WHEEL_ZOOM,
                    WHEEL_VSCROLL,
                    WHEEL_HSCROLL,
                    WHEEL_FG,
                    WHEEL_BG,
                    WHEEL_FRAME };

static CursorType rotated_size_cursors[] = {
  kSizeRCursor,
  kSizeTRCursor,
  kSizeTCursor,
  kSizeTLCursor,
  kSizeLCursor,
  kSizeBLCursor,
  kSizeBCursor,
  kSizeBRCursor
};

static CursorType rotated_rotate_cursors[] = {
  kRotateRCursor,
  kRotateTRCursor,
  kRotateTCursor,
  kRotateTLCursor,
  kRotateLCursor,
  kRotateBLCursor,
  kRotateBCursor,
  kRotateBRCursor
};

#pragma warning(disable:4355) // warning C4355: 'this' : used in base member initializer list

StandbyState::StandbyState()
  : m_decorator(new Decorator(this))
{
}

StandbyState::~StandbyState()
{
  delete m_decorator;
}

void StandbyState::onAfterChangeState(Editor* editor)
{
  editor->setDecorator(m_decorator);
}

void StandbyState::onCurrentToolChange(Editor* editor)
{
  tools::Tool* currentTool = editor->getCurrentEditorTool();

  // If the user change from a selection tool to a non-selection tool,
  // or viceversa, we've to show or hide the transformation handles.
  // TODO Compare the ink (isSelection()) of the previous tool with
  // the new one.
  editor->invalidate();
}

bool StandbyState::checkForScroll(Editor* editor, MouseMessage* msg)
{
  UIContext* context = UIContext::instance();
  tools::Tool* currentTool = editor->getCurrentEditorTool();
  tools::Ink* clickedInk = currentTool->getInk(msg->right() ? 1: 0);

  // Start scroll loop
  if (msg->middle() || clickedInk->isScrollMovement()) { // TODO msg->middle() should be customizable
    editor->setState(EditorStatePtr(new ScrollingState()));
    editor->captureMouse();
    return true;
  }
  else
    return false;
}

bool StandbyState::onMouseDown(Editor* editor, MouseMessage* msg)
{
  if (editor->hasCapture())
    return true;

  UIContext* context = UIContext::instance();
  tools::Tool* current_tool = editor->getCurrentEditorTool();
  tools::Ink* clickedInk = current_tool->getInk(msg->right() ? 1: 0);
  DocumentLocation location;
  editor->getDocumentLocation(&location);
  Document* document = location.document();
  Sprite* sprite = location.sprite();
  Layer* layer = location.layer();

  // When an editor is clicked the current view is changed.
  context->setActiveView(editor->getDocumentView());

  // Start scroll loop
  if (checkForScroll(editor, msg))
    return true;

  // Move cel X,Y coordinates
  if (clickedInk->isCelMovement()) {
    if ((layer) &&
        (layer->type() == OBJECT_LAYER_IMAGE)) {
      // TODO you can move the `Background' with tiled mode
      if (layer->isBackground()) {
        Alert::show(PACKAGE
                    "<<You can't move the `Background' layer."
                    "||&Close");
      }
      else if (!layer->isMoveable()) {
        Alert::show(PACKAGE "<<The layer movement is locked.||&Close");
      }
      else {
        // Change to MovingCelState
        editor->setState(EditorStatePtr(new MovingCelState(editor, msg)));
      }
    }
    return true;
  }

  // Transform selected pixels
  if (document->isMaskVisible() &&
      m_decorator->getTransformHandles(editor)) {
    TransformHandles* transfHandles = m_decorator->getTransformHandles(editor);

    // Get the handle covered by the mouse.
    HandleType handle = transfHandles->getHandleAtPoint(editor,
                                                        msg->position(),
                                                        document->getTransformation());

    if (handle != NoHandle) {
      int x, y, opacity;
      Image* image = location.image(&x, &y, &opacity);
      if (image) {
        if (!layer->isWritable()) {
          Alert::show(PACKAGE "<<The layer is locked.||&Close");
          return true;
        }

        // Change to MovingPixelsState
        transformSelection(editor, msg, handle);
      }
      return true;
    }
  }

  // Move selected pixels
  if (editor->isInsideSelection() &&
      current_tool->getInk(0)->isSelection() &&
      msg->left()) {
    int x, y, opacity;
    Image* image = location.image(&x, &y, &opacity);
    if (image) {
      if (!layer->isWritable()) {
        Alert::show(PACKAGE "<<The layer is locked.||&Close");
        return true;
      }

      // Change to MovingPixelsState
      transformSelection(editor, msg, MoveHandle);
    }
    return true;
  }

  // Call the eyedropper command
  if (clickedInk->isEyedropper()) {
    Command* eyedropper_cmd =
      CommandsModule::instance()->getCommandByName(CommandId::Eyedropper);

    Params params;
    params.set("target", msg->right() ? "background": "foreground");

    UIContext::instance()->executeCommand(eyedropper_cmd, &params);
    return true;
  }

  // Start the Tool-Loop
  if (layer) {
    tools::ToolLoop* toolLoop = create_tool_loop(editor, context, msg);
    if (toolLoop)
      editor->setState(EditorStatePtr(new DrawingState(toolLoop, editor, msg)));
    return true;
  }

  return true;
}

bool StandbyState::onMouseUp(Editor* editor, MouseMessage* msg)
{
  editor->releaseMouse();
  return true;
}

bool StandbyState::onMouseMove(Editor* editor, MouseMessage* msg)
{
  editor->moveDrawingCursor();
  editor->updateStatusBar();
  return true;
}

bool StandbyState::onMouseWheel(Editor* editor, MouseMessage* msg)
{
  int dz = jmouse_z(1) - jmouse_z(0);
  WHEEL_ACTION wheelAction = WHEEL_NONE;
  bool scrollBigSteps = false;

  // Without modifiers
  if (msg->keyModifiers() == kKeyNoneModifier) {
    wheelAction = WHEEL_ZOOM;
  }
  else {
#if 1                           // TODO make it configurable
    if (msg->altPressed()) {
      if (msg->shiftPressed())
        wheelAction = WHEEL_BG;
      else
        wheelAction = WHEEL_FG;
    }
    else if (msg->ctrlPressed()) {
      wheelAction = WHEEL_FRAME;
    }
#else
    if (msg->ctrlPressed())
      wheelAction = WHEEL_HSCROLL;
    else
      wheelAction = WHEEL_VSCROLL;

    if (msg->shiftPressed())
      scrollBigSteps = true;
#endif
  }

  switch (wheelAction) {

    case WHEEL_NONE:
      // Do nothing
      break;

    case WHEEL_FG:
      // if (m_state == EDITOR_STATE_STANDBY)
      {
        int newIndex = 0;
        if (ColorBar::instance()->getFgColor().getType() == app::Color::IndexType) {
          newIndex = ColorBar::instance()->getFgColor().getIndex() + dz;
          newIndex = MID(0, newIndex, 255);
        }
        ColorBar::instance()->setFgColor(app::Color::fromIndex(newIndex));
      }
      break;

    case WHEEL_BG:
      // if (m_state == EDITOR_STATE_STANDBY)
      {
        int newIndex = 0;
        if (ColorBar::instance()->getBgColor().getType() == app::Color::IndexType) {
          newIndex = ColorBar::instance()->getBgColor().getIndex() + dz;
          newIndex = MID(0, newIndex, 255);
        }
        ColorBar::instance()->setBgColor(app::Color::fromIndex(newIndex));
      }
      break;

    case WHEEL_FRAME:
      // if (m_state == EDITOR_STATE_STANDBY)
      {
        Command* command = CommandsModule::instance()->getCommandByName
          ((dz < 0) ? CommandId::GotoNextFrame:
                      CommandId::GotoPreviousFrame);
        if (command)
          UIContext::instance()->executeCommand(command, NULL);
      }
      break;

    case WHEEL_ZOOM: {
      MouseMessage* mouseMsg = static_cast<MouseMessage*>(msg);
      int zoom = MID(MIN_ZOOM, editor->getZoom()-dz, MAX_ZOOM);
      if (editor->getZoom() != zoom)
        editor->setZoomAndCenterInMouse(zoom, mouseMsg->position().x, mouseMsg->position().y);
      break;
    }

    case WHEEL_HSCROLL:
    case WHEEL_VSCROLL: {
      View* view = View::getView(editor);
      gfx::Rect vp = view->getViewportBounds();
      int dx = 0;
      int dy = 0;

      if (wheelAction == WHEEL_HSCROLL) {
        dx = dz * vp.w;
      }
      else {
        dy = dz * vp.h;
      }

      if (scrollBigSteps) {
        dx /= 2;
        dy /= 2;
      }
      else {
        dx /= 10;
        dy /= 10;
      }

      gfx::Point scroll = view->getViewScroll();

      editor->hideDrawingCursor();
      editor->setEditorScroll(scroll.x+dx, scroll.y+dy, true);
      editor->showDrawingCursor();
      break;
    }

  }

  return true;
}

bool StandbyState::onSetCursor(Editor* editor)
{
  tools::Tool* current_tool = editor->getCurrentEditorTool();

  if (current_tool) {
    tools::Ink* current_ink = current_tool->getInk(0);

    // If the current tool change selection (e.g. rectangular marquee, etc.)
    if (current_ink->isSelection()) {
      // See if the cursor is in some selection handle.
      if (m_decorator->onSetCursor(editor))
        return true;

      // Move pixels
      if (editor->isInsideSelection()) {
        EditorCustomizationDelegate* customization = editor->getCustomizationDelegate();

        editor->hideDrawingCursor();

        if (customization && customization->isCopySelectionKeyPressed())
          jmouse_set_cursor(kArrowPlusCursor);
        else
          jmouse_set_cursor(kMoveCursor);

        return true;
      }
    }
    else if (current_ink->isEyedropper()) {
      editor->hideDrawingCursor();
      jmouse_set_cursor(kEyedropperCursor);
      return true;
    }
    else if (current_ink->isScrollMovement()) {
      editor->hideDrawingCursor();
      jmouse_set_cursor(kScrollCursor);
      return true;
    }
    else if (current_ink->isCelMovement()) {
      editor->hideDrawingCursor();
      jmouse_set_cursor(kMoveCursor);
      return true;
    }
  }

  // Draw
  if (editor->canDraw()) {
    jmouse_set_cursor(kNoCursor);
    editor->showDrawingCursor();
  }
  // Forbidden
  else {
    editor->hideDrawingCursor();
    jmouse_set_cursor(kForbiddenCursor);
  }

  return true;
}

bool StandbyState::onKeyDown(Editor* editor, KeyMessage* msg)
{
  return editor->processKeysToSetZoom(msg);
}

bool StandbyState::onKeyUp(Editor* editor, KeyMessage* msg)
{
  return false;
}

bool StandbyState::onUpdateStatusBar(Editor* editor)
{
  tools::Tool* current_tool = editor->getCurrentEditorTool();
  const Sprite* sprite = editor->getSprite();
  int x, y;

  editor->screenToEditor(jmouse_x(0), jmouse_y(0), &x, &y);

  if (!sprite) {
    StatusBar::instance()->clearText();
  }
  // For eye-dropper
  else if (current_tool->getInk(0)->isEyedropper()) {
    PixelFormat format = sprite->getPixelFormat();
    uint32_t pixel = sprite->getPixel(x, y, editor->getFrame());
    app::Color color = app::Color::fromImage(format, pixel);

    int alpha = 255;
    switch (format) {
      case IMAGE_RGB: alpha = rgba_geta(pixel); break;
      case IMAGE_GRAYSCALE: alpha = graya_geta(pixel); break;
    }

    char buf[256];
    usprintf(buf, "- Pos %d %d", x, y);

    StatusBar::instance()->showColor(0, buf, color, alpha);
  }
  else {
    Mask* mask =
      (editor->getDocument()->isMaskVisible() ? 
       editor->getDocument()->getMask(): NULL);

    StatusBar::instance()->setStatusText(0,
      "Pos %d %d, Size %d %d, Frame %d [%d msecs]",
      x, y,
      (mask ? mask->getBounds().w: sprite->getWidth()),
      (mask ? mask->getBounds().h: sprite->getHeight()),
      editor->getFrame()+1,
      sprite->getFrameDuration(editor->getFrame()));
  }

  return true;
}

gfx::Transformation StandbyState::getTransformation(Editor* editor)
{
  return editor->getDocument()->getTransformation();
}

void StandbyState::transformSelection(Editor* editor, MouseMessage* msg, HandleType handle)
{
  try {
    EditorCustomizationDelegate* customization = editor->getCustomizationDelegate();
    Document* document = editor->getDocument();
    base::UniquePtr<Image> tmpImage(NewImageFromMask(editor->getDocumentLocation()));
    int x = document->getMask()->getBounds().x;
    int y = document->getMask()->getBounds().y;
    int opacity = 255;
    Sprite* sprite = editor->getSprite();
    Layer* layer = editor->getLayer();
    PixelsMovementPtr pixelsMovement(
      new PixelsMovement(UIContext::instance(),
        document, sprite, layer,
        tmpImage, x, y, opacity,
        "Transformation"));

    // If the Ctrl key is pressed start dragging a copy of the selection
    if (customization && customization->isCopySelectionKeyPressed())
      pixelsMovement->copyMask();
    else
      pixelsMovement->cutMask();

    editor->setState(EditorStatePtr(new MovingPixelsState(editor, msg, pixelsMovement, handle)));
  }
  catch (const LockedDocumentException&) {
    // Other editor is locking the document.

    // TODO steal the PixelsMovement of the other editor and use it for this one.
  }
}

//////////////////////////////////////////////////////////////////////
// Decorator

StandbyState::Decorator::Decorator(StandbyState* standbyState)
  : m_transfHandles(NULL)
  , m_standbyState(standbyState)
{
}

StandbyState::Decorator::~Decorator()
{
  delete m_transfHandles;
}

TransformHandles* StandbyState::Decorator::getTransformHandles(Editor* editor)
{
  if (!m_transfHandles)
    m_transfHandles = new TransformHandles();

  return m_transfHandles;
}

bool StandbyState::Decorator::onSetCursor(Editor* editor)
{
  if (!editor->getDocument()->isMaskVisible())
    return false;

  const gfx::Transformation transformation(m_standbyState->getTransformation(editor));
  TransformHandles* tr = getTransformHandles(editor);
  HandleType handle = tr->getHandleAtPoint(editor,
                                           gfx::Point(jmouse_x(0), jmouse_y(0)),
                                           transformation);

  CursorType newCursor = kArrowCursor;

  switch (handle) {
    case ScaleNWHandle:         newCursor = kSizeTLCursor; break;
    case ScaleNHandle:          newCursor = kSizeTCursor; break;
    case ScaleNEHandle:         newCursor = kSizeTRCursor; break;
    case ScaleWHandle:          newCursor = kSizeLCursor; break;
    case ScaleEHandle:          newCursor = kSizeRCursor; break;
    case ScaleSWHandle:         newCursor = kSizeBLCursor; break;
    case ScaleSHandle:          newCursor = kSizeBCursor; break;
    case ScaleSEHandle:         newCursor = kSizeBRCursor; break;
    case RotateNWHandle:        newCursor = kRotateTLCursor; break;
    case RotateNHandle:         newCursor = kRotateTCursor; break;
    case RotateNEHandle:        newCursor = kRotateTRCursor; break;
    case RotateWHandle:         newCursor = kRotateLCursor; break;
    case RotateEHandle:         newCursor = kRotateRCursor; break;
    case RotateSWHandle:        newCursor = kRotateBLCursor; break;
    case RotateSHandle:         newCursor = kRotateBCursor; break;
    case RotateSEHandle:        newCursor = kRotateBRCursor; break;
    case PivotHandle:           newCursor = kHandCursor; break;
    default:
      return false;
  }

  // Adjust the cursor depending the current transformation angle.
  fixed angle = ftofix(128.0 * transformation.angle() / PI);
  angle = fixadd(angle, itofix(16));
  angle &= (255<<16);
  angle >>= 16;
  angle /= 32;

  if (newCursor >= kSizeTLCursor && newCursor <= kSizeBRCursor) {
    size_t num = sizeof(rotated_size_cursors) / sizeof(rotated_size_cursors[0]);
    size_t c;
    for (c=num-1; c>0; --c)
      if (rotated_size_cursors[c] == newCursor)
        break;

    newCursor = rotated_size_cursors[(c+angle) % num];
  }
  else if (newCursor >= kRotateTLCursor && newCursor <= kRotateBRCursor) {
    size_t num = sizeof(rotated_rotate_cursors) / sizeof(rotated_rotate_cursors[0]);
    size_t c;
    for (c=num-1; c>0; --c)
      if (rotated_rotate_cursors[c] == newCursor)
        break;

    newCursor = rotated_rotate_cursors[(c+angle) % num];
  }

  // Hide the drawing cursor (just in case) and show the new system cursor.
  editor->hideDrawingCursor();
  jmouse_set_cursor(newCursor);
  return true;
}

void StandbyState::Decorator::preRenderDecorator(EditorPreRender* render)
{
  // Do nothing
}

void StandbyState::Decorator::postRenderDecorator(EditorPostRender* render)
{
  Editor* editor = render->getEditor();

  // Draw transformation handles (if the mask is visible and isn't frozen).
  if (editor->getDocument()->isMaskVisible() &&
      !editor->getDocument()->getMask()->isFrozen()) {
    // And draw only when the user has a selection tool as active tool.
    tools::Tool* currentTool = editor->getCurrentEditorTool();

    if (currentTool->getInk(0)->isSelection())
      getTransformHandles(editor)->drawHandles(editor,
                                               m_standbyState->getTransformation(editor));
  }
}

} // namespace app
