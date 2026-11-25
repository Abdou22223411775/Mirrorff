// Mirrorff — fork of QtScrcpy v4.2.1 (Apache-2.0)
#ifndef KEYMAPIO_H
#define KEYMAPIO_H

#include <QJsonObject>
#include <QObject>
#include <QPair>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>

class QKeyEvent;

// One gameplay button of a keymap file. Field names match the keymap JSON
// format read by the input converter (see keymap.cpp in the core):
// positions are relative 0..1, origin top-left. "comment" and "size" are
// ignored by the converter but preserved round-trip by this model.
struct KeymapNode
{
    QString type; // "KMT_CLICK", "KMT_CLICK_TWICE", "KMT_CLICK_MULTI",
                  // "KMT_STEER_WHEEL", "KMT_DRAG", "KMT_ANDROID_KEY"
    QString key;  // Qt::Key enum name ("Key_F1", "Key_Space", ...) or mouse
                  // button name ("LeftButton", "RightButton")
    QPointF pos;  // click position, steer wheel centre (centerPos), drag
                  // startPos; unused for multi (see clickNodes) and for
                  // android-key nodes (no screen position)
    bool switchMap = false;
    QString comment;
    int size = 60;
    // KMT_CLICK_MULTI tap points: (delay in ms, relative position).
    QVector<QPair<int, QPointF> > clickNodes;
    // KMT_STEER_WHEEL
    QString leftKey;
    QString rightKey;
    QString upKey;
    QString downKey;
    double leftOffset = 0;
    double rightOffset = 0;
    double upOffset = 0;
    double downOffset = 0;
    // KMT_DRAG
    QPointF endPos;
    quint32 startDelay = 0;
    float dragSpeed = 1.0f;
    // KMT_ANDROID_KEY (and optional extra on KMT_CLICK / KMT_CLICK_TWICE)
    double androidKey = 0;
};

// Mouse-look ("mouseMoveMap") block of a keymap file.
struct MouseMoveMap
{
    bool present = false;
    QPointF startPos;
    double speedRatioX = 1;
    double speedRatioY = 1;
    bool smallEyes = false;
    QString smallEyesKey;
    QPointF smallEyesPos;
};

// Editor model for a keymap file. No UI, no file I/O: the parent form owns
// reading/writing the file and talking to the device, this class only owns
// the parsed document, validation and undo/redo.
//
// Undo/redo are JSON-string snapshots (cap 50). Every mutating method pushes
// the pre-change JSON first (pushUndo) and clears the redo stack, except
// setFileName (the file name is not part of the JSON document) and
// loadFromJson (a fresh document clears both stacks).
// Direct mutation through the non-const nodes() accessor is allowed; callers
// must call pushUndo() before mutating and modelChanged() after, so the
// undo stack and the views stay in sync.
class KeymapModel : public QObject
{
    Q_OBJECT
public:
    explicit KeymapModel(QObject *parent = 0);

    bool loadFromJson(const QString &json, QString *error);
    QString toJson() const;

    QString switchKey() const;
    void setSwitchKey(const QString &k);
    MouseMoveMap mouseMoveMap() const;
    void setMouseMoveMap(const MouseMoveMap &m);

    const QVector<KeymapNode> &nodes() const;
    QVector<KeymapNode> &nodes();
    void setNodes(const QVector<KeymapNode> &n);

    QString fileName() const;
    void setFileName(const QString &n);

    // Overlap validation and nudging are aspect-independent: the keymap
    // is a relative (0..1) layout, so its geometry is always checked on
    // the fixed design canvas (2048x921, the screenshot the keymap was
    // built from), never on the live video size. Validating on a
    // portrait video failed the shipped keymap: the button size scales
    // with widget height while the horizontal spacing scales with
    // width, so fire/jump rects intersected below w/h ~= 0.52 and
    // every drag release on a portrait video was rejected.
    // The 48 px minimum centre separation keeps its exact meaning, in
    // design-canvas pixels.
    bool validate(QString *error) const;
    bool hasDuplicateKey(QString *dupKey) const;
    void nudgeOverlaps();

    // Rewrites a keymap JSON string, converting any legacy multi-tap
    // array into "clickNodes" (the converter ignores the legacy name).
    // Returns the input unchanged when it cannot be parsed.
    static QString convertDelayClickNodes(const QString &json);
    // Qt::Key enum name of the pressed key ("Key_F1", "Key_Shift", ...).
    // Modifier-only keys that are valid gameplay keys (Shift, Control,
    // Alt, CapsLock, Meta) return their names, so pressing one alone
    // during rebind assigns it. Returns an empty string for unknown keys
    // and for modifier-only keys that cannot be gameplay keys (AltGr,
    // NumLock, ScrollLock, Super, Hyper).
    static QString keyNameFromKeyEvent(QKeyEvent *event);
    // Assignable modifier-only keys: (Qt::Key enum name, display name).
    // Shift, Control, Alt, CapsLock, Meta — the keys a player may want
    // on a button (e.g. sprint = Shift) but cannot type as a plain key.
    static QVector<QPair<QString, QString> > modifierKeys();
    // Short label for a key name: "Key_F1" -> "F1", "Key_Ampersand" -> "&",
    // "LeftButton" -> "LMB", ...
    static QString keyDisplayName(const QString &keyName);
    static bool isMouseButtonKey(const QString &keyName);
    // 60 px at 1080p height, scaled to the video height (never below 28 px).
    static int buttonPixelSize(const QSize &videoSize);

    void pushUndo();
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;

    int selectedIndex() const;
    void setSelectedIndex(int i);

signals:
    void modelChanged();
    void selectionChanged(int index);

private:
    // Geometry of one touchable button, in design-canvas pixels. Used
    // by validate() and nudgeOverlaps(). tapIndex is -1 for a single
    // button (pos, steer wheel centre, drag startPos) and >= 0 for a
    // multi tap point.
    struct ButtonGeom
    {
        QRectF rect;
        QPointF centre;
        QString label;
        int nodeIndex;
        int tapIndex;
    };

    bool parseKeymapJson(const QString &json, QString *error, QString *switchKeyOut,
                         MouseMoveMap *mouseMoveMapOut, QVector<KeymapNode> *nodesOut) const;
    // Restores a snapshot produced by toJson() without touching the undo
    // stacks and without nudging (used by undo()/redo()).
    bool restoreFromJson(const QString &json, QString *error);
    QVector<ButtonGeom> buttonGeometries(const QVector<KeymapNode> &nodes, const QSize &videoSize) const;
    QVector<QPair<QString, QString> > collectKeys() const; // (key, label)

    static QPointF readPos(const QJsonObject &obj, const QString &name, bool *ok);
    static bool isValidKeyName(const QString &name);
    static QString nodeLabel(const KeymapNode &node);

    QString m_switchKey;
    MouseMoveMap m_mouseMoveMap;
    QVector<KeymapNode> m_nodes;
    QString m_fileName;
    int m_selectedIndex;
    QStringList m_undoStack;
    QStringList m_redoStack;
};

#endif // KEYMAPIO_H
