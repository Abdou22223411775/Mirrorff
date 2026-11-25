// Mirrorff — fork of QtScrcpy v4.2.1 (Apache-2.0)
#ifndef KEYMAPOVERLAY_H
#define KEYMAPOVERLAY_H

#include <QTimer>
#include <QWidget>

#include "keymapio.h"

class QContextMenuEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QPainter;

// Transparent editor drawn INSIDE the video widget (this class is a child of
// it, never a separate top-level window). In play mode it is hidden so
// gameplay input flows through untouched; in edit mode it is visible and
// accepts the mouse and the keyboard.
//
// All positions are stored relative 0..1 in the model; this widget only
// converts between relative coordinates and its own pixels (size()).
class KeymapOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit KeymapOverlay(QWidget *parent = 0);

    void setModel(KeymapModel *model);
    void setEditMode(bool on);
    bool editMode() const;
    void setShowCrosshair(bool on);
    bool showCrosshair() const;

    void addClickAt(const QPointF &relPos);
    void addMultiAt(const QPointF &relPos);
    void addWheelAt(const QPointF &relPos);
    void deleteSelected();
    void duplicateSelected();
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;

    // Returns true when the key press was consumed (rebind capture or
    // Escape). F9 is NOT handled here: the parent form owns that shortcut.
    bool handleKeyPress(QKeyEvent *event);

signals:
    void statusMessage(const QString &msg);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
    void onModelChanged();
    void onSelectionChanged(int index);
    void onFlashTimeout();
    void onDragRepaintTimeout();

private:
    enum HitPart
    {
        HitNone,
        HitNode,           // single button / drag start / android-key chip
        HitTapPoint,       // one tap point of a multi node
        HitWheelCentre,    // wheel centre (drags the whole wheel)
        HitWheelDirection, // one direction dot of a wheel
        HitMouseLook,      // mouse-look crosshair (startPos)
        HitSmallEyes       // smallEyes dot
    };
    struct Hit
    {
        HitPart part;
        int nodeIndex;
        int tapIndex;  // multi tap point, -1 when not applicable
        int direction; // wheel direction: 0 left, 1 right, 2 up, 3 down
    };
    enum DragKind
    {
        DragNone,
        DragNode,
        DragTapPoint,
        DragWheelCentre,
        DragWheelDirection,
        DragMouseLook,
        DragSmallEyes
    };

    Hit hitTest(const QPointF &relPos) const;
    QPointF relToPx(const QPointF &relPos) const;
    QPointF pxToRel(const QPointF &pxPos) const;
    QPointF wheelDirectionPos(const KeymapNode &node, int direction) const;
    QRectF nodeRect(const KeymapNode &node) const;
    QRectF androidChipRect(int nodeIndex) const;
    void paintNode(QPainter *painter, const KeymapNode &node, int index);
    QString firstFreeKey(const QStringList &extraUsed) const;
    bool keyInUse(const QString &key, int excludeNode, int excludeDirection) const;
    void beginCapture(int nodeIndex, int direction);
    void endCapture();
    void revertDrag();
    // Drag repaint throttle (see the .cpp): mouse moves arrive far faster
    // than the video widget can repaint, so repaints during a drag are
    // coalesced to ~30 Hz and limited to the dragged button's old+new
    // area instead of the whole overlay.
    void requestDragRepaint(const QRectF &dirty);
    void finishDragRepaint();

    KeymapModel *m_model;
    bool m_editMode;
    bool m_showCrosshair;

    // Rebind capture (double-click a button, then press a key).
    int m_captureIndex;
    int m_captureDirection; // -1: the node key itself, 0..3: wheel direction
    QTimer m_flashTimer;
    bool m_flashOn;

    // Drag state. The undo snapshot is pushed once, on the first mouse
    // move that actually changes something (m_undoPushed); a drag that
    // only selects leaves the undo stack alone.
    DragKind m_dragKind;
    int m_dragNode;
    int m_dragTap;
    int m_dragDirection;
    QPointF m_grabOffset; // press point minus dragged centre, relative
    bool m_undoPushed;
    // Live preview for mouse-look / smallEyes drags: the model has no
    // mutable accessor for mouseMoveMap, so the dragged position is shown
    // from here and only written to the model on release.
    bool m_hasPreview;
    QPointF m_previewMouseLookPos;
    QPointF m_previewSmallEyesPos;

    // Drag repaint throttle: a full-overlay update() on every mouse move
    // forces a full repaint of the parent QOpenGLWidget (paintGL + child
    // compositing) and starves the decoder's frame uploads, freezing the
    // video. Repaints during a drag are therefore rate-limited to one
    // every 33 ms (~30 Hz) and clipped to the union of the dragged
    // button's old and new rectangles.
    QTimer m_dragRepaintTimer;
    bool m_dragRepaintPending;
    QRectF m_dragDirtyRect;
};

#endif // KEYMAPOVERLAY_H
