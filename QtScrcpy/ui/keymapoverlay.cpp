// Mirrorff — fork of QtScrcpy v4.2.1 (Apache-2.0)
#include "keymapoverlay.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

static QString nodeLabelText(const KeymapNode &node)
{
    if (!node.comment.isEmpty()) {
        return node.comment;
    }
    if (!node.key.isEmpty()) {
        return KeymapModel::keyDisplayName(node.key);
    }
    return node.type;
}

KeymapOverlay::KeymapOverlay(QWidget *parent)
    : QWidget(parent)
    , m_model(0)
    , m_editMode(false)
    , m_showCrosshair(false)
    , m_captureIndex(-1)
    , m_captureDirection(-1)
    , m_flashOn(false)
    , m_dragKind(DragNone)
    , m_dragNode(-1)
    , m_dragTap(-1)
    , m_dragDirection(-1)
    , m_undoPushed(false)
    , m_hasPreview(false)
{
    setFocusPolicy(Qt::StrongFocus);
    setVisible(false); // play mode: hidden until edit mode is switched on
    m_flashTimer.setInterval(350);
    connect(&m_flashTimer, &QTimer::timeout, this, &KeymapOverlay::onFlashTimeout);
}

void KeymapOverlay::setModel(KeymapModel *model)
{
    if (m_model) {
        disconnect(m_model, 0, this, 0);
    }
    m_model = model;
    if (m_model) {
        connect(m_model, &KeymapModel::modelChanged, this, &KeymapOverlay::onModelChanged);
        connect(m_model, &KeymapModel::selectionChanged, this, &KeymapOverlay::onSelectionChanged);
    }
    update();
}

void KeymapOverlay::setEditMode(bool on)
{
    if (on == m_editMode) {
        return;
    }
    m_editMode = on;
    if (!on) {
        endCapture();
    }
    // In play mode the overlay is hidden so gameplay input flows through
    // untouched; in edit mode it is visible and accepts mouse events.
    setVisible(on);
    update();
}

bool KeymapOverlay::editMode() const
{
    return m_editMode;
}

void KeymapOverlay::setShowCrosshair(bool on)
{
    if (on == m_showCrosshair) {
        return;
    }
    m_showCrosshair = on;
    update();
}

bool KeymapOverlay::showCrosshair() const
{
    return m_showCrosshair;
}

void KeymapOverlay::onModelChanged()
{
    update();
}

void KeymapOverlay::onSelectionChanged(int)
{
    update();
}

void KeymapOverlay::onFlashTimeout()
{
    m_flashOn = !m_flashOn;
    update();
}

QPointF KeymapOverlay::relToPx(const QPointF &relPos) const
{
    return QPointF(relPos.x() * width(), relPos.y() * height());
}

QPointF KeymapOverlay::pxToRel(const QPointF &pxPos) const
{
    if (width() <= 0 || height() <= 0) {
        return QPointF();
    }
    return QPointF(pxPos.x() / width(), pxPos.y() / height());
}

QPointF KeymapOverlay::wheelDirectionPos(const KeymapNode &node, int direction) const
{
    switch (direction) {
    case 0:
        return QPointF(node.pos.x() - node.leftOffset, node.pos.y());
    case 1:
        return QPointF(node.pos.x() + node.rightOffset, node.pos.y());
    case 2:
        return QPointF(node.pos.x(), node.pos.y() - node.upOffset);
    default:
        return QPointF(node.pos.x(), node.pos.y() + node.downOffset);
    }
}

QRectF KeymapOverlay::nodeRect(const KeymapNode &node) const
{
    int btnSize = KeymapModel::buttonPixelSize(size());
    double half = btnSize / 2.0;
    if (node.type == "KMT_STEER_WHEEL") {
        double x0 = (node.pos.x() - node.leftOffset) * width();
        double x1 = (node.pos.x() + node.rightOffset) * width();
        double y0 = (node.pos.y() - node.upOffset) * height();
        double y1 = (node.pos.y() + node.downOffset) * height();
        return QRectF(QPointF(x0, y0), QPointF(x1, y1)).normalized();
    }
    if (node.type == "KMT_CLICK_MULTI") {
        QRectF rect;
        for (int t = 0; t < node.clickNodes.size(); ++t) {
            QPointF px = relToPx(node.clickNodes.at(t).second);
            QRectF tapRect(px.x() - half, px.y() - half, btnSize, btnSize);
            rect = rect.isNull() ? tapRect : rect.united(tapRect);
        }
        return rect;
    }
    QPointF px = relToPx(node.pos);
    return QRectF(px.x() - half, px.y() - half, btnSize, btnSize);
}

QRectF KeymapOverlay::androidChipRect(int nodeIndex) const
{
    int ordinal = 0;
    if (m_model) {
        for (int i = 0; i < nodeIndex && i < m_model->nodes().size(); ++i) {
            if (m_model->nodes().at(i).type == "KMT_ANDROID_KEY") {
                ++ordinal;
            }
        }
    }
    return QRectF(8, 8 + ordinal * 30, 170, 26);
}

QString KeymapOverlay::firstFreeKey(const QStringList &extraUsed) const
{
    QStringList candidates;
    for (int i = 2; i <= 12; ++i) {
        candidates.append(QString("Key_F%1").arg(i));
    }
    for (int i = 0; i < 26; ++i) {
        candidates.append(QString("Key_%1").arg(QChar('A' + i)));
    }
    for (int i = 1; i <= 9; ++i) {
        candidates.append(QString("Key_%1").arg(i));
    }
    candidates.append("Key_0");
    QStringList used = extraUsed;
    if (m_model) {
        used.append(m_model->switchKey());
        for (int i = 0; i < m_model->nodes().size(); ++i) {
            const KeymapNode &node = m_model->nodes().at(i);
            if (node.type == "KMT_STEER_WHEEL") {
                used.append(node.leftKey);
                used.append(node.rightKey);
                used.append(node.upKey);
                used.append(node.downKey);
            } else if (!node.key.isEmpty()) {
                used.append(node.key);
            }
        }
        if (m_model->mouseMoveMap().present && m_model->mouseMoveMap().smallEyes) {
            used.append(m_model->mouseMoveMap().smallEyesKey);
        }
    }
    for (int i = 0; i < candidates.size(); ++i) {
        if (!used.contains(candidates.at(i))) {
            return candidates.at(i);
        }
    }
    return "Key_F2";
}

bool KeymapOverlay::keyInUse(const QString &key, int excludeNode, int excludeDirection) const
{
    if (!m_model || key.isEmpty()) {
        return false;
    }
    if (m_model->switchKey() == key) {
        return true;
    }
    for (int i = 0; i < m_model->nodes().size(); ++i) {
        const KeymapNode &node = m_model->nodes().at(i);
        if (i == excludeNode) {
            if (node.type == "KMT_STEER_WHEEL" && excludeDirection >= 0) {
                if (excludeDirection != 0 && node.leftKey == key) {
                    return true;
                }
                if (excludeDirection != 1 && node.rightKey == key) {
                    return true;
                }
                if (excludeDirection != 2 && node.upKey == key) {
                    return true;
                }
                if (excludeDirection != 3 && node.downKey == key) {
                    return true;
                }
            }
            continue;
        }
        if (node.type == "KMT_STEER_WHEEL") {
            if (node.leftKey == key || node.rightKey == key || node.upKey == key || node.downKey == key) {
                return true;
            }
        } else if (node.key == key) {
            return true;
        }
    }
    MouseMoveMap map = m_model->mouseMoveMap();
    if (map.present && map.smallEyes && map.smallEyesKey == key) {
        return true;
    }
    return false;
}

void KeymapOverlay::beginCapture(int nodeIndex, int direction)
{
    if (!m_model || nodeIndex < 0 || nodeIndex >= m_model->nodes().size()) {
        return;
    }
    m_captureIndex = nodeIndex;
    m_captureDirection = direction;
    m_flashOn = true;
    m_flashTimer.start();
    m_model->setSelectedIndex(nodeIndex);
    QString label = nodeLabelText(m_model->nodes().at(nodeIndex));
    if (direction >= 0) {
        QString dirName;
        if (direction == 0) {
            dirName = QString::fromUtf8("gauche");
        } else if (direction == 1) {
            dirName = QString::fromUtf8("droite");
        } else if (direction == 2) {
            dirName = QString::fromUtf8("haut");
        } else {
            dirName = QString::fromUtf8("bas");
        }
        emit statusMessage(QString::fromUtf8("Appuyez sur une touche pour « %1 » (%2)… (Échap pour annuler)")
                               .arg(label, dirName));
    } else {
        emit statusMessage(QString::fromUtf8("Appuyez sur une touche pour « %1 »… (Échap pour annuler)").arg(label));
    }
    update();
}

void KeymapOverlay::endCapture()
{
    m_captureIndex = -1;
    m_captureDirection = -1;
    m_flashTimer.stop();
    m_flashOn = false;
    update();
}

void KeymapOverlay::revertDrag()
{
    if (!m_model) {
        return;
    }
    // The drop was rejected by validation: undo the drag. pushUndo()
    // afterwards clears the redo stack, so the rejected arrangement cannot
    // come back through redo.
    m_model->undo();
    m_model->pushUndo();
    update();
}

void KeymapOverlay::addClickAt(const QPointF &relPos)
{
    if (!m_model) {
        return;
    }
    m_model->pushUndo();
    KeymapNode node;
    node.type = "KMT_CLICK";
    node.key = firstFreeKey(QStringList());
    node.pos = QPointF(qBound(0.0, relPos.x(), 1.0), qBound(0.0, relPos.y(), 1.0));
    node.switchMap = false;
    node.size = 60;
    m_model->nodes().append(node);
    m_model->setSelectedIndex(m_model->nodes().size() - 1);
    m_model->modelChanged();
    emit statusMessage(QString::fromUtf8("Bouton ajouté : %1").arg(KeymapModel::keyDisplayName(node.key)));
}

void KeymapOverlay::addMultiAt(const QPointF &relPos)
{
    if (!m_model) {
        return;
    }
    m_model->pushUndo();
    QPointF first(qBound(0.0, relPos.x(), 1.0), qBound(0.0, relPos.y(), 1.0));
    QPointF second(qBound(0.0, relPos.x() + 0.05, 1.0), qBound(0.0, relPos.y(), 1.0));
    KeymapNode node;
    node.type = "KMT_CLICK_MULTI";
    node.key = firstFreeKey(QStringList());
    node.switchMap = false;
    node.size = 60;
    node.clickNodes.append(qMakePair(0, first));
    node.clickNodes.append(qMakePair(60, second));
    m_model->nodes().append(node);
    m_model->setSelectedIndex(m_model->nodes().size() - 1);
    m_model->modelChanged();
    emit statusMessage(QString::fromUtf8("Bouton multi ajouté : %1").arg(KeymapModel::keyDisplayName(node.key)));
}

void KeymapOverlay::addWheelAt(const QPointF &relPos)
{
    if (!m_model) {
        return;
    }
    m_model->pushUndo();
    QStringList extra;
    KeymapNode node;
    node.type = "KMT_STEER_WHEEL";
    node.pos = QPointF(qBound(0.0, relPos.x(), 1.0), qBound(0.0, relPos.y(), 1.0));
    node.leftKey = firstFreeKey(extra);
    extra.append(node.leftKey);
    node.rightKey = firstFreeKey(extra);
    extra.append(node.rightKey);
    node.upKey = firstFreeKey(extra);
    extra.append(node.upKey);
    node.downKey = firstFreeKey(extra);
    node.leftOffset = 0.06;
    node.rightOffset = 0.06;
    node.upOffset = 0.11;
    node.downOffset = 0.11;
    node.size = 60;
    m_model->nodes().append(node);
    m_model->setSelectedIndex(m_model->nodes().size() - 1);
    m_model->modelChanged();
    emit statusMessage(QString::fromUtf8("Roue ajoutée"));
}

void KeymapOverlay::deleteSelected()
{
    if (!m_model) {
        return;
    }
    int index = m_model->selectedIndex();
    if (index < 0 || index >= m_model->nodes().size()) {
        return;
    }
    m_model->pushUndo();
    m_model->nodes().removeAt(index);
    m_model->setSelectedIndex(-1);
    m_model->modelChanged();
    emit statusMessage(QString::fromUtf8("Bouton supprimé"));
}

void KeymapOverlay::duplicateSelected()
{
    if (!m_model) {
        return;
    }
    int index = m_model->selectedIndex();
    if (index < 0 || index >= m_model->nodes().size()) {
        return;
    }
    m_model->pushUndo();
    KeymapNode node = m_model->nodes().at(index);
    QPointF shift(0.03, 0.03);
    if (node.type == "KMT_CLICK_MULTI") {
        for (int t = 0; t < node.clickNodes.size(); ++t) {
            QPointF p = node.clickNodes.at(t).second + shift;
            p.setX(qBound(0.0, p.x(), 1.0));
            p.setY(qBound(0.0, p.y(), 1.0));
            node.clickNodes[t].second = p;
        }
        node.key = firstFreeKey(QStringList());
    } else if (node.type == "KMT_STEER_WHEEL") {
        node.pos.setX(qBound(0.0, node.pos.x() + shift.x(), 1.0));
        node.pos.setY(qBound(0.0, node.pos.y() + shift.y(), 1.0));
        QStringList extra;
        node.leftKey = firstFreeKey(extra);
        extra.append(node.leftKey);
        node.rightKey = firstFreeKey(extra);
        extra.append(node.rightKey);
        node.upKey = firstFreeKey(extra);
        extra.append(node.upKey);
        node.downKey = firstFreeKey(extra);
    } else if (node.type == "KMT_DRAG") {
        node.pos.setX(qBound(0.0, node.pos.x() + shift.x(), 1.0));
        node.pos.setY(qBound(0.0, node.pos.y() + shift.y(), 1.0));
        node.endPos.setX(qBound(0.0, node.endPos.x() + shift.x(), 1.0));
        node.endPos.setY(qBound(0.0, node.endPos.y() + shift.y(), 1.0));
        node.key = firstFreeKey(QStringList());
    } else if (node.type == "KMT_ANDROID_KEY") {
        node.key = firstFreeKey(QStringList());
    } else {
        node.pos.setX(qBound(0.0, node.pos.x() + shift.x(), 1.0));
        node.pos.setY(qBound(0.0, node.pos.y() + shift.y(), 1.0));
        node.key = firstFreeKey(QStringList());
    }
    m_model->nodes().append(node);
    m_model->setSelectedIndex(m_model->nodes().size() - 1);
    m_model->modelChanged();
    emit statusMessage(QString::fromUtf8("Bouton dupliqué"));
}

void KeymapOverlay::undo()
{
    if (m_model) {
        m_model->undo();
    }
}

void KeymapOverlay::redo()
{
    if (m_model) {
        m_model->redo();
    }
}

bool KeymapOverlay::canUndo() const
{
    return m_model && m_model->canUndo();
}

bool KeymapOverlay::canRedo() const
{
    return m_model && m_model->canRedo();
}

bool KeymapOverlay::handleKeyPress(QKeyEvent *event)
{
    if (!event || !m_model) {
        return false;
    }
    if (m_captureIndex >= 0 && m_captureIndex < m_model->nodes().size()) {
        if (event->key() == Qt::Key_Escape) {
            endCapture();
            emit statusMessage(QString::fromUtf8("Capture annulée"));
            return true;
        }
        QString name = KeymapModel::keyNameFromKeyEvent(event);
        if (name.isEmpty()) {
            return true; // unknown key: stay in capture, ignore the key
        }
        if (keyInUse(name, m_captureIndex, m_captureDirection)) {
            endCapture();
            emit statusMessage(QString::fromUtf8("La touche « %1 » est déjà utilisée")
                                   .arg(KeymapModel::keyDisplayName(name)));
            return true;
        }
        m_model->pushUndo();
        KeymapNode &node = m_model->nodes()[m_captureIndex];
        if (m_captureDirection >= 0) {
            switch (m_captureDirection) {
            case 0:
                node.leftKey = name;
                break;
            case 1:
                node.rightKey = name;
                break;
            case 2:
                node.upKey = name;
                break;
            default:
                node.downKey = name;
                break;
            }
        } else {
            node.key = name;
        }
        QString display = KeymapModel::keyDisplayName(name);
        endCapture();
        // modelChanged is a public signal: emitting it here tells the other
        // views (sidebar) that the model changed through direct editing.
        m_model->modelChanged();
        emit statusMessage(QString::fromUtf8("Touche définie : %1").arg(display));
        return true;
    }
    if (event->key() == Qt::Key_Escape) {
        if (m_model->selectedIndex() >= 0) {
            m_model->setSelectedIndex(-1);
            return true;
        }
        return false;
    }
    return false;
}

void KeymapOverlay::keyPressEvent(QKeyEvent *event)
{
    if (!handleKeyPress(event)) {
        event->ignore(); // let the parent form see the key
    }
}

KeymapOverlay::Hit KeymapOverlay::hitTest(const QPointF &relPos) const
{
    Hit hit;
    hit.part = HitNone;
    hit.nodeIndex = -1;
    hit.tapIndex = -1;
    hit.direction = -1;
    if (!m_model || width() <= 0 || height() <= 0) {
        return hit;
    }
    QPointF pxPos = relToPx(relPos);
    // Nodes first, topmost first (reverse draw order).
    for (int i = m_model->nodes().size() - 1; i >= 0; --i) {
        const KeymapNode &node = m_model->nodes().at(i);
        if (node.type == "KMT_CLICK" || node.type == "KMT_CLICK_TWICE" || node.type == "KMT_DRAG") {
            QRectF rect = nodeRect(node).adjusted(-6, -6, 6, 6);
            if (rect.contains(pxPos)) {
                hit.part = HitNode;
                hit.nodeIndex = i;
                return hit;
            }
        } else if (node.type == "KMT_CLICK_MULTI") {
            for (int t = node.clickNodes.size() - 1; t >= 0; --t) {
                QPointF tapPx = relToPx(node.clickNodes.at(t).second);
                double dx = pxPos.x() - tapPx.x();
                double dy = pxPos.y() - tapPx.y();
                if (qSqrt(dx * dx + dy * dy) <= 14.0) {
                    hit.part = HitTapPoint;
                    hit.nodeIndex = i;
                    hit.tapIndex = t;
                    return hit;
                }
            }
        } else if (node.type == "KMT_STEER_WHEEL") {
            for (int d = 0; d < 4; ++d) {
                QPointF dotPx = relToPx(wheelDirectionPos(node, d));
                double dx = pxPos.x() - dotPx.x();
                double dy = pxPos.y() - dotPx.y();
                if (qSqrt(dx * dx + dy * dy) <= 14.0) {
                    hit.part = HitWheelDirection;
                    hit.nodeIndex = i;
                    hit.direction = d;
                    return hit;
                }
            }
            QPointF centrePx = relToPx(node.pos);
            double dx = pxPos.x() - centrePx.x();
            double dy = pxPos.y() - centrePx.y();
            if (qSqrt(dx * dx + dy * dy) <= 14.0) {
                hit.part = HitWheelCentre;
                hit.nodeIndex = i;
                return hit;
            }
            if (nodeRect(node).contains(pxPos)) {
                hit.part = HitWheelCentre;
                hit.nodeIndex = i;
                return hit;
            }
        } else if (node.type == "KMT_ANDROID_KEY") {
            if (androidChipRect(i).contains(pxPos)) {
                hit.part = HitNode;
                hit.nodeIndex = i;
                return hit;
            }
        }
    }
    // Mouse-look crosshair and smallEyes dot sit under the buttons.
    if (m_showCrosshair && m_model->mouseMoveMap().present) {
        MouseMoveMap map = m_model->mouseMoveMap();
        QPointF startPx = relToPx(map.startPos);
        double dx = pxPos.x() - startPx.x();
        double dy = pxPos.y() - startPx.y();
        if (qSqrt(dx * dx + dy * dy) <= 14.0) {
            hit.part = HitMouseLook;
            return hit;
        }
        if (map.smallEyes) {
            QPointF sePx = relToPx(map.smallEyesPos);
            dx = pxPos.x() - sePx.x();
            dy = pxPos.y() - sePx.y();
            if (qSqrt(dx * dx + dy * dy) <= 14.0) {
                hit.part = HitSmallEyes;
                return hit;
            }
        }
    }
    return hit;
}

void KeymapOverlay::mousePressEvent(QMouseEvent *event)
{
    if (!m_editMode || !m_model || event->button() != Qt::LeftButton) {
        return; // right button is handled by contextMenuEvent
    }
    setFocus();
    QPointF relPos = pxToRel(event->pos());
    Hit hit = hitTest(relPos);
    m_dragKind = DragNone;
    m_dragNode = -1;
    m_dragTap = -1;
    m_dragDirection = -1;
    m_hasPreview = false;
    m_undoPushed = false;
    if (hit.part == HitNone) {
        m_model->setSelectedIndex(-1);
        update();
        return;
    }
    if (hit.part == HitMouseLook || hit.part == HitSmallEyes) {
        m_model->setSelectedIndex(-1);
        m_dragNode = -1;
        m_dragKind = hit.part == HitMouseLook ? DragMouseLook : DragSmallEyes;
        MouseMoveMap map = m_model->mouseMoveMap();
        m_previewMouseLookPos = map.startPos;
        m_previewSmallEyesPos = map.smallEyesPos;
        m_hasPreview = false; // no change yet; set on the first move
        return;
    }
    m_model->setSelectedIndex(hit.nodeIndex);
    m_dragNode = hit.nodeIndex;
    const KeymapNode &node = m_model->nodes().at(hit.nodeIndex);
    switch (hit.part) {
    case HitNode:
        if (node.type == "KMT_ANDROID_KEY") {
            m_dragKind = DragNone; // chips have no screen position
        } else {
            m_dragKind = DragNode;
            m_grabOffset = relPos - node.pos;
        }
        break;
    case HitTapPoint:
        m_dragKind = DragTapPoint;
        m_dragTap = hit.tapIndex;
        m_grabOffset = relPos - node.clickNodes.at(hit.tapIndex).second;
        break;
    case HitWheelCentre:
        m_dragKind = DragWheelCentre;
        m_grabOffset = relPos - node.pos;
        break;
    case HitWheelDirection:
        m_dragKind = DragWheelDirection;
        m_dragDirection = hit.direction;
        break;
    default:
        break;
    }
}

void KeymapOverlay::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_editMode || !m_model || m_dragKind == DragNone) {
        return;
    }
    QPointF relPos = pxToRel(event->pos());
    // The undo snapshot is pushed once, on the first move that changes
    // something, so a click without a drag leaves the stack alone.
    switch (m_dragKind) {
    case DragNode:
    case DragWheelCentre: {
        if (m_dragNode < 0 || m_dragNode >= m_model->nodes().size()) {
            return;
        }
        KeymapNode &node = m_model->nodes()[m_dragNode];
        QPointF target = relPos - m_grabOffset;
        target.setX(qBound(0.0, target.x(), 1.0));
        target.setY(qBound(0.0, target.y(), 1.0));
        if (target == node.pos) {
            return;
        }
        if (!m_undoPushed) {
            m_model->pushUndo();
            m_undoPushed = true;
        }
        node.pos = target;
        update();
        break;
    }
    case DragTapPoint: {
        if (m_dragNode < 0 || m_dragNode >= m_model->nodes().size()) {
            return;
        }
        KeymapNode &node = m_model->nodes()[m_dragNode];
        if (m_dragTap < 0 || m_dragTap >= node.clickNodes.size()) {
            return;
        }
        QPointF target(qBound(0.0, relPos.x(), 1.0), qBound(0.0, relPos.y(), 1.0));
        if (target == node.clickNodes.at(m_dragTap).second) {
            return;
        }
        if (!m_undoPushed) {
            m_model->pushUndo();
            m_undoPushed = true;
        }
        node.clickNodes[m_dragTap].second = target;
        update();
        break;
    }
    case DragWheelDirection: {
        if (m_dragNode < 0 || m_dragNode >= m_model->nodes().size()) {
            return;
        }
        KeymapNode &node = m_model->nodes()[m_dragNode];
        double offset = 0;
        switch (m_dragDirection) {
        case 0:
            offset = node.pos.x() - relPos.x();
            break;
        case 1:
            offset = relPos.x() - node.pos.x();
            break;
        case 2:
            offset = node.pos.y() - relPos.y();
            break;
        default:
            offset = relPos.y() - node.pos.y();
            break;
        }
        offset = qBound(0.02, offset, 0.20);
        double current = 0;
        switch (m_dragDirection) {
        case 0:
            current = node.leftOffset;
            break;
        case 1:
            current = node.rightOffset;
            break;
        case 2:
            current = node.upOffset;
            break;
        default:
            current = node.downOffset;
            break;
        }
        if (offset == current) {
            return;
        }
        if (!m_undoPushed) {
            m_model->pushUndo();
            m_undoPushed = true;
        }
        switch (m_dragDirection) {
        case 0:
            node.leftOffset = offset;
            break;
        case 1:
            node.rightOffset = offset;
            break;
        case 2:
            node.upOffset = offset;
            break;
        default:
            node.downOffset = offset;
            break;
        }
        update();
        break;
    }
    case DragMouseLook: {
        QPointF target(qBound(0.0, relPos.x(), 1.0), qBound(0.0, relPos.y(), 1.0));
        if (target == m_previewMouseLookPos) {
            return;
        }
        m_previewMouseLookPos = target;
        m_hasPreview = true;
        update();
        break;
    }
    case DragSmallEyes: {
        QPointF target(qBound(0.0, relPos.x(), 1.0), qBound(0.0, relPos.y(), 1.0));
        if (target == m_previewSmallEyesPos) {
            return;
        }
        m_previewSmallEyesPos = target;
        m_hasPreview = true;
        update();
        break;
    }
    default:
        break;
    }
}

void KeymapOverlay::mouseReleaseEvent(QMouseEvent *)
{
    if (!m_editMode || !m_model || m_dragKind == DragNone) {
        return;
    }
    DragKind kind = m_dragKind;
    m_dragKind = DragNone;
    if (kind == DragMouseLook || kind == DragSmallEyes) {
        if (!m_hasPreview) {
            update();
            return;
        }
        m_hasPreview = false;
        MouseMoveMap map = m_model->mouseMoveMap();
        if (kind == DragMouseLook) {
            map.startPos = m_previewMouseLookPos;
        } else {
            map.smallEyesPos = m_previewSmallEyesPos;
        }
        m_model->setMouseMoveMap(map); // pushes undo, emits modelChanged
        QString error;
        if (!m_model->validate(&error, size())) {
            revertDrag();
            emit statusMessage(error);
        }
        update();
        return;
    }
    if (!m_undoPushed) {
        update(); // clicked without moving: nothing changed
        return;
    }
    m_undoPushed = false;
    QString error;
    if (m_model->validate(&error, size())) {
        m_model->modelChanged();
    } else {
        revertDrag();
        emit statusMessage(error);
    }
    update();
}

void KeymapOverlay::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (!m_editMode || !m_model || event->button() != Qt::LeftButton) {
        return;
    }
    QPointF relPos = pxToRel(event->pos());
    Hit hit = hitTest(relPos);
    if (hit.part == HitNode || hit.part == HitTapPoint) {
        // Rebind capture for that node (its key).
        beginCapture(hit.nodeIndex, -1);
        return;
    }
    if (hit.part == HitWheelDirection) {
        // Rebind capture for that direction key.
        beginCapture(hit.nodeIndex, hit.direction);
        return;
    }
    if (hit.part == HitNone) {
        addClickAt(relPos);
    }
    // Wheel centre, mouse-look and smallEyes have no rebind target here.
}

void KeymapOverlay::contextMenuEvent(QContextMenuEvent *event)
{
    if (!m_editMode || !m_model) {
        return;
    }
    QPointF relPos = pxToRel(event->pos());
    Hit hit = hitTest(relPos);
    if (hit.part == HitNone || hit.part == HitMouseLook || hit.part == HitSmallEyes) {
        return;
    }
    m_model->setSelectedIndex(hit.nodeIndex);
    QMenu menu(this);
    QAction *deleteAction = menu.addAction(QString::fromUtf8("Supprimer"));
    QAction *duplicateAction = menu.addAction(QString::fromUtf8("Dupliquer"));
    QAction *removeTapAction = 0;
    if (hit.part == HitTapPoint) {
        const KeymapNode &node = m_model->nodes().at(hit.nodeIndex);
        if (node.clickNodes.size() > 1) {
            removeTapAction = menu.addAction(QString::fromUtf8("Supprimer ce point"));
        }
    }
    QAction *chosen = menu.exec(event->globalPos());
    if (!chosen) {
        return;
    }
    if (chosen == deleteAction) {
        deleteSelected();
    } else if (chosen == duplicateAction) {
        duplicateSelected();
    } else if (removeTapAction && chosen == removeTapAction) {
        m_model->pushUndo();
        m_model->nodes()[hit.nodeIndex].clickNodes.removeAt(hit.tapIndex);
        m_model->modelChanged();
        emit statusMessage(QString::fromUtf8("Point supprimé"));
    }
}

void KeymapOverlay::paintEvent(QPaintEvent *)
{
    if (!m_model || width() <= 0 || height() <= 0) {
        return;
    }
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Mouse-look crosshair first, under the buttons.
    if (m_showCrosshair && m_model->mouseMoveMap().present) {
        MouseMoveMap map = m_model->mouseMoveMap();
        QPointF startPos = m_previewMouseLookPos;
        if (!m_hasPreview || m_dragKind != DragMouseLook) {
            startPos = map.startPos;
        }
        QPointF startPx = relToPx(startPos);
        painter.setPen(QPen(QColor(0, 200, 255, 160), 1, Qt::DashLine));
        painter.drawLine(QPointF(0, startPx.y()), QPointF(width(), startPx.y()));
        painter.drawLine(QPointF(startPx.x(), 0), QPointF(startPx.x(), height()));
        painter.setPen(QPen(QColor(0, 200, 255), 2));
        painter.setBrush(QColor(0, 200, 255, 90));
        painter.drawEllipse(startPx, 10, 10);
        painter.setPen(Qt::white);
        painter.drawText(QRectF(startPx.x() - 40, startPx.y() - 28, 80, 16), Qt::AlignCenter,
                         QString::fromUtf8("visée"));
        if (map.smallEyes) {
            QPointF sePos = m_previewSmallEyesPos;
            if (!m_hasPreview || m_dragKind != DragSmallEyes) {
                sePos = map.smallEyesPos;
            }
            QPointF sePx = relToPx(sePos);
            painter.setPen(QPen(QColor(255, 165, 0), 2));
            painter.setBrush(QColor(255, 165, 0, 90));
            painter.drawEllipse(sePx, 9, 9);
            painter.setPen(Qt::white);
            painter.drawText(QRectF(sePx.x() - 24, sePx.y() - 26, 48, 16), Qt::AlignCenter,
                             KeymapModel::keyDisplayName(map.smallEyesKey));
        }
    }

    for (int i = 0; i < m_model->nodes().size(); ++i) {
        paintNode(&painter, m_model->nodes().at(i), i);
    }
}

void KeymapOverlay::paintNode(QPainter *painter, const KeymapNode &node, int index)
{
    bool selected = m_model && m_model->selectedIndex() == index;
    bool capturing = m_captureIndex == index;
    QColor fill(30, 144, 255, 70);
    QColor border(30, 144, 255, 200);
    if (selected) {
        border = QColor(255, 215, 0);
    }
    if (capturing && m_flashOn) {
        border = QColor(255, 60, 60);
    }
    QPen pen(border, (selected || capturing) ? 3 : 2);
    QFont boldFont = painter->font();
    boldFont.setBold(true);
    QFont smallFont = painter->font();
    smallFont.setBold(false);
    smallFont.setPointSize(7);

    if (node.type == "KMT_CLICK" || node.type == "KMT_CLICK_TWICE") {
        QRectF rect = nodeRect(node);
        painter->setPen(pen);
        painter->setBrush(fill);
        painter->drawRoundedRect(rect, 8, 8);
        painter->setFont(boldFont);
        painter->setPen(Qt::white);
        painter->drawText(QRectF(rect.x(), rect.y() + 6, rect.width(), 22), Qt::AlignCenter,
                          KeymapModel::keyDisplayName(node.key));
        if (node.type == "KMT_CLICK_TWICE") {
            painter->setFont(smallFont);
            painter->drawText(QRectF(rect.x(), rect.y() + rect.height() - 18, rect.width() - 6, 14),
                              Qt::AlignRight | Qt::AlignVCenter, "x2");
        } else if (!node.comment.isEmpty()) {
            painter->setFont(smallFont);
            QString elided = painter->fontMetrics().elidedText(node.comment, Qt::ElideRight,
                                                               static_cast<int>(rect.width()) - 6);
            painter->drawText(QRectF(rect.x() + 3, rect.y() + rect.height() - 18, rect.width() - 6, 14),
                              Qt::AlignCenter, elided);
        }
        if (node.switchMap) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(255, 215, 0));
            painter->drawEllipse(rect.topRight() + QPointF(-9, 9), 4, 4);
        }
    } else if (node.type == "KMT_CLICK_MULTI") {
        if (node.clickNodes.isEmpty()) {
            return;
        }
        QPainterPath path;
        for (int t = 0; t < node.clickNodes.size(); ++t) {
            QPointF px = relToPx(node.clickNodes.at(t).second);
            if (t == 0) {
                path.moveTo(px);
            } else {
                path.lineTo(px);
            }
        }
        painter->setPen(QPen(border, 2, Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(path);
        for (int t = 0; t < node.clickNodes.size(); ++t) {
            QPointF px = relToPx(node.clickNodes.at(t).second);
            painter->setPen(pen);
            painter->setBrush(fill);
            painter->drawEllipse(px, 11, 11);
            painter->setFont(boldFont);
            painter->setPen(Qt::white);
            painter->drawText(QRectF(px.x() - 11, px.y() - 8, 22, 16), Qt::AlignCenter,
                              QString::number(t + 1));
        }
        QPointF first = relToPx(node.clickNodes.at(0).second);
        painter->setFont(boldFont);
        painter->setPen(Qt::white);
        painter->drawText(QRectF(first.x() + 14, first.y() - 10, 60, 20), Qt::AlignLeft | Qt::AlignVCenter,
                          KeymapModel::keyDisplayName(node.key));
        if (selected) {
            painter->setPen(QPen(QColor(255, 215, 0), 2, Qt::DashLine));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(nodeRect(node));
        }
    } else if (node.type == "KMT_STEER_WHEEL") {
        QPointF centrePx = relToPx(node.pos);
        double rx = qMax(node.leftOffset, node.rightOffset) * width();
        double ry = qMax(node.upOffset, node.downOffset) * height();
        painter->setPen(pen);
        painter->setBrush(fill);
        painter->drawEllipse(centrePx, rx, ry);
        painter->setPen(Qt::NoPen);
        painter->setBrush(border);
        painter->drawEllipse(centrePx, 4, 4);
        for (int d = 0; d < 4; ++d) {
            QPointF dotPx = relToPx(wheelDirectionPos(node, d));
            QString key;
            if (d == 0) {
                key = node.leftKey;
            } else if (d == 1) {
                key = node.rightKey;
            } else if (d == 2) {
                key = node.upKey;
            } else {
                key = node.downKey;
            }
            bool dotCapturing = capturing && m_captureDirection == d && m_flashOn;
            painter->setPen(pen);
            painter->setBrush(dotCapturing ? QColor(255, 60, 60, 120) : fill);
            painter->drawEllipse(dotPx, 11, 11);
            painter->setFont(boldFont);
            painter->setPen(Qt::white);
            painter->drawText(QRectF(dotPx.x() - 14, dotPx.y() - 8, 28, 16), Qt::AlignCenter,
                              KeymapModel::keyDisplayName(key));
        }
    } else if (node.type == "KMT_DRAG") {
        QPointF startPx = relToPx(node.pos);
        QPointF endPx = relToPx(node.endPos);
        painter->setPen(QPen(border, 2, Qt::SolidLine));
        painter->setBrush(fill);
        painter->drawLine(startPx, endPx);
        painter->drawEllipse(startPx, 11, 11);
        painter->drawEllipse(endPx, 7, 7);
        painter->setFont(boldFont);
        painter->setPen(Qt::white);
        painter->drawText(QRectF(startPx.x() - 20, startPx.y() - 26, 40, 16), Qt::AlignCenter,
                          KeymapModel::keyDisplayName(node.key));
    } else if (node.type == "KMT_ANDROID_KEY") {
        QRectF chip = androidChipRect(index);
        painter->setPen(pen);
        painter->setBrush(fill);
        painter->drawRoundedRect(chip, 6, 6);
        painter->setFont(boldFont);
        painter->setPen(Qt::white);
        QString text = QString::fromUtf8("%1 (%2)")
                           .arg(KeymapModel::keyDisplayName(node.key))
                           .arg(node.androidKey, 0, 'f', 0);
        painter->drawText(chip, Qt::AlignCenter, text);
    }
}
