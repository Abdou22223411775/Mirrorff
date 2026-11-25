// Mirrorff — fork of QtScrcpy v4.2.1 (Apache-2.0)
#include "keymapio.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QMetaEnum>
#include <QtMath>

// ---------------------------------------------------------------------------
// Local helpers
// ---------------------------------------------------------------------------

static QJsonObject posToJson(const QPointF &pos)
{
    QJsonObject obj;
    obj.insert("x", pos.x());
    obj.insert("y", pos.y());
    return obj;
}

static QJsonObject nodeToJson(const KeymapNode &node)
{
    QJsonObject obj;
    obj.insert("type", node.type);
    obj.insert("comment", node.comment);
    obj.insert("size", node.size);
    if (node.type == "KMT_CLICK" || node.type == "KMT_CLICK_TWICE") {
        obj.insert("key", node.key);
        obj.insert("pos", posToJson(node.pos));
        obj.insert("switchMap", node.switchMap);
        if (node.androidKey != 0) {
            obj.insert("androidKey", node.androidKey);
        }
    } else if (node.type == "KMT_CLICK_MULTI") {
        obj.insert("key", node.key);
        obj.insert("switchMap", node.switchMap);
        QJsonArray taps;
        for (int i = 0; i < node.clickNodes.size(); ++i) {
            QJsonObject tap;
            tap.insert("delay", node.clickNodes.at(i).first);
            tap.insert("pos", posToJson(node.clickNodes.at(i).second));
            taps.append(tap);
        }
        obj.insert("clickNodes", taps);
    } else if (node.type == "KMT_STEER_WHEEL") {
        obj.insert("centerPos", posToJson(node.pos));
        obj.insert("leftKey", node.leftKey);
        obj.insert("rightKey", node.rightKey);
        obj.insert("upKey", node.upKey);
        obj.insert("downKey", node.downKey);
        obj.insert("leftOffset", node.leftOffset);
        obj.insert("rightOffset", node.rightOffset);
        obj.insert("upOffset", node.upOffset);
        obj.insert("downOffset", node.downOffset);
    } else if (node.type == "KMT_DRAG") {
        obj.insert("key", node.key);
        obj.insert("startPos", posToJson(node.pos));
        obj.insert("endPos", posToJson(node.endPos));
        obj.insert("startDelay", static_cast<int>(node.startDelay));
        obj.insert("dragSpeed", static_cast<double>(node.dragSpeed));
    } else if (node.type == "KMT_ANDROID_KEY") {
        obj.insert("key", node.key);
        obj.insert("androidKey", node.androidKey);
    }
    return obj;
}

static bool positionsOverlap(const QRectF &a, const QPointF &centreA, const QRectF &b, const QPointF &centreB)
{
    if (a.intersects(b)) {
        return true;
    }
    double dx = centreA.x() - centreB.x();
    double dy = centreA.y() - centreB.y();
    return qSqrt(dx * dx + dy * dy) < 48.0;
}

// ---------------------------------------------------------------------------
// KeymapModel
// ---------------------------------------------------------------------------

KeymapModel::KeymapModel(QObject *parent) : QObject(parent), m_selectedIndex(-1) {}

QPointF KeymapModel::readPos(const QJsonObject &obj, const QString &name, bool *ok)
{
    if (ok) {
        *ok = false;
    }
    if (!obj.contains(name) || !obj.value(name).isObject()) {
        return QPointF();
    }
    QJsonObject pos = obj.value(name).toObject();
    if (!pos.contains("x") || !pos.value("x").isDouble() || !pos.contains("y") || !pos.value("y").isDouble()) {
        return QPointF();
    }
    if (ok) {
        *ok = true;
    }
    return QPointF(pos.value("x").toDouble(), pos.value("y").toDouble());
}

bool KeymapModel::isValidKeyName(const QString &name)
{
    if (name.isEmpty() || name == "Key_unknown") {
        return false;
    }
    QByteArray latin = name.toLatin1();
    if (QMetaEnum::fromType<Qt::Key>().keyToValue(latin.constData()) != -1) {
        return true;
    }
    if (QMetaEnum::fromType<Qt::MouseButtons>().keyToValue(latin.constData()) != -1) {
        return true;
    }
    return false;
}

QString KeymapModel::nodeLabel(const KeymapNode &node)
{
    if (!node.comment.isEmpty()) {
        return node.comment;
    }
    if (!node.key.isEmpty()) {
        return keyDisplayName(node.key);
    }
    if (node.type == "KMT_STEER_WHEEL") {
        return QString::fromUtf8("Roue");
    }
    return node.type;
}

bool KeymapModel::parseKeymapJson(const QString &json, QString *error, QString *switchKeyOut,
                                 MouseMoveMap *mouseMoveMapOut, QVector<KeymapNode> *nodesOut) const
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (error) {
            *error = QString::fromUtf8("JSON invalide : ") + parseError.errorString();
        }
        return false;
    }
    if (!doc.isObject()) {
        if (error) {
            *error = QString::fromUtf8("Le keymap doit être un objet JSON");
        }
        return false;
    }
    QJsonObject root = doc.object();

    // switchKey is required; without it the converter aborts the whole file.
    if (!root.contains("switchKey") || !root.value("switchKey").isString()) {
        if (error) {
            *error = QString::fromUtf8("switchKey manquante dans le keymap");
        }
        return false;
    }
    QString switchKey = root.value("switchKey").toString();
    if (!isValidKeyName(switchKey)) {
        if (error) {
            *error = QString::fromUtf8("switchKey invalide : ") + switchKey;
        }
        return false;
    }
    *switchKeyOut = switchKey;

    // mouseMoveMap (optional). Same rules as the converter: a speed ratio
    // is required, then startPos; a malformed smallEyes block is skipped.
    MouseMoveMap moveMap;
    if (root.contains("mouseMoveMap") && root.value("mouseMoveMap").isObject()) {
        QJsonObject mm = root.value("mouseMoveMap").toObject();
        bool haveRatio = false;
        if (mm.contains("speedRatio") && mm.value("speedRatio").isDouble()) {
            double ratio = mm.value("speedRatio").toDouble();
            moveMap.speedRatioX = ratio;
            moveMap.speedRatioY = ratio / 2.25; // same rule as the converter
            haveRatio = true;
        }
        if (mm.contains("speedRatioX") && mm.value("speedRatioX").isDouble()) {
            moveMap.speedRatioX = mm.value("speedRatioX").toDouble();
            haveRatio = true;
        }
        if (mm.contains("speedRatioY") && mm.value("speedRatioY").isDouble()) {
            moveMap.speedRatioY = mm.value("speedRatioY").toDouble();
            haveRatio = true;
        }
        if (!haveRatio) {
            if (error) {
                *error = QString::fromUtf8("speedRatio manquant dans mouseMoveMap");
            }
            return false;
        }
        bool posOk = false;
        moveMap.startPos = readPos(mm, "startPos", &posOk);
        if (!posOk) {
            if (error) {
                *error = QString::fromUtf8("startPos manquant dans mouseMoveMap");
            }
            return false;
        }
        moveMap.present = true;
        if (mm.contains("smallEyes") && mm.value("smallEyes").isObject()) {
            QJsonObject se = mm.value("smallEyes").toObject();
            bool enabled = true;
            if (se.contains("enabled") && se.value("enabled").isBool()) {
                enabled = se.value("enabled").toBool();
            }
            if (enabled && se.value("type").toString() == "KMT_CLICK" && se.contains("key")
                && se.value("key").isString() && isValidKeyName(se.value("key").toString())) {
                bool sePosOk = false;
                QPointF sePos = readPos(se, "pos", &sePosOk);
                if (sePosOk) {
                    moveMap.smallEyes = true;
                    moveMap.smallEyesKey = se.value("key").toString();
                    moveMap.smallEyesPos = sePos;
                }
            }
        }
    }
    *mouseMoveMapOut = moveMap;

    // keyMapNodes: invalid nodes are skipped, like the converter does.
    QVector<KeymapNode> nodes;
    if (root.contains("keyMapNodes") && root.value("keyMapNodes").isArray()) {
        QJsonArray arr = root.value("keyMapNodes").toArray();
        for (int i = 0; i < arr.size(); ++i) {
            if (!arr.at(i).isObject()) {
                continue;
            }
            QJsonObject obj = arr.at(i).toObject();
            if (!obj.contains("type") || !obj.value("type").isString()) {
                continue;
            }
            QString type = obj.value("type").toString();
            KeymapNode node;
            node.type = type;
            node.comment = obj.value("comment").toString();
            if (obj.contains("size") && obj.value("size").isDouble()) {
                node.size = obj.value("size").toInt(60);
            }
            bool ok = false;
            if (type == "KMT_CLICK" || type == "KMT_CLICK_TWICE") {
                if (!obj.contains("key") || !obj.value("key").isString()) {
                    continue;
                }
                if (!isValidKeyName(obj.value("key").toString())) {
                    continue;
                }
                bool posOk = false;
                QPointF pos = readPos(obj, "pos", &posOk);
                if (!posOk) {
                    continue;
                }
                // switchMap is required for KMT_CLICK (converter rule).
                if (type == "KMT_CLICK" && (!obj.contains("switchMap") || !obj.value("switchMap").isBool())) {
                    continue;
                }
                node.key = obj.value("key").toString();
                node.pos = pos;
                node.switchMap = obj.value("switchMap").toBool(false);
                if (obj.contains("androidKey") && obj.value("androidKey").isDouble()) {
                    node.androidKey = obj.value("androidKey").toDouble();
                }
                ok = true;
            } else if (type == "KMT_CLICK_MULTI") {
                if (!obj.contains("key") || !obj.value("key").isString()) {
                    continue;
                }
                if (!isValidKeyName(obj.value("key").toString())) {
                    continue;
                }
                QJsonArray taps;
                if (obj.contains("clickNodes") && obj.value("clickNodes").isArray()) {
                    taps = obj.value("clickNodes").toArray();
                } else if (obj.contains("delayClickNodes") && obj.value("delayClickNodes").isArray()) {
                    taps = obj.value("delayClickNodes").toArray(); // converted on import
                } else {
                    continue;
                }
                if (taps.isEmpty()) {
                    continue;
                }
                node.key = obj.value("key").toString();
                node.switchMap = obj.value("switchMap").toBool(false);
                bool tapsOk = true;
                for (int t = 0; t < taps.size() && t < 50; ++t) {
                    if (!taps.at(t).isObject()) {
                        tapsOk = false;
                        break;
                    }
                    QJsonObject tap = taps.at(t).toObject();
                    bool posOk = false;
                    QPointF pos = readPos(tap, "pos", &posOk);
                    if (!posOk || !tap.contains("delay") || !tap.value("delay").isDouble()) {
                        tapsOk = false;
                        break;
                    }
                    node.clickNodes.append(qMakePair(tap.value("delay").toInt(), pos));
                }
                if (!tapsOk || node.clickNodes.isEmpty()) {
                    continue;
                }
                ok = true;
            } else if (type == "KMT_STEER_WHEEL") {
                QString leftKey = obj.value("leftKey").toString();
                QString rightKey = obj.value("rightKey").toString();
                QString upKey = obj.value("upKey").toString();
                QString downKey = obj.value("downKey").toString();
                if (!isValidKeyName(leftKey) || !isValidKeyName(rightKey) || !isValidKeyName(upKey)
                    || !isValidKeyName(downKey)) {
                    continue;
                }
                bool posOk = false;
                QPointF centre = readPos(obj, "centerPos", &posOk);
                if (!posOk) {
                    continue;
                }
                if (!obj.value("leftOffset").isDouble() || !obj.value("rightOffset").isDouble()
                    || !obj.value("upOffset").isDouble() || !obj.value("downOffset").isDouble()) {
                    continue;
                }
                node.leftKey = leftKey;
                node.rightKey = rightKey;
                node.upKey = upKey;
                node.downKey = downKey;
                node.pos = centre;
                node.leftOffset = obj.value("leftOffset").toDouble();
                node.rightOffset = obj.value("rightOffset").toDouble();
                node.upOffset = obj.value("upOffset").toDouble();
                node.downOffset = obj.value("downOffset").toDouble();
                ok = true;
            } else if (type == "KMT_DRAG") {
                if (!obj.contains("key") || !obj.value("key").isString()) {
                    continue;
                }
                if (!isValidKeyName(obj.value("key").toString())) {
                    continue;
                }
                bool startOk = false;
                bool endOk = false;
                QPointF start = readPos(obj, "startPos", &startOk);
                QPointF end = readPos(obj, "endPos", &endOk);
                if (!startOk || !endOk) {
                    continue;
                }
                node.key = obj.value("key").toString();
                node.pos = start;
                node.endPos = end;
                if (obj.contains("startDelay") && obj.value("startDelay").isDouble()) {
                    node.startDelay = static_cast<quint32>(obj.value("startDelay").toDouble());
                }
                if (obj.contains("dragSpeed") && obj.value("dragSpeed").isDouble()) {
                    node.dragSpeed = static_cast<float>(obj.value("dragSpeed").toDouble());
                }
                ok = true;
            } else if (type == "KMT_ANDROID_KEY") {
                if (!obj.contains("key") || !obj.value("key").isString()) {
                    continue;
                }
                if (!isValidKeyName(obj.value("key").toString())) {
                    continue;
                }
                if (!obj.contains("androidKey") || !obj.value("androidKey").isDouble()) {
                    continue;
                }
                node.key = obj.value("key").toString();
                node.androidKey = obj.value("androidKey").toDouble();
                ok = true;
            } else {
                continue; // unknown type (including KMT_MOUSE_MOVE as a node)
            }
            if (ok) {
                nodes.append(node);
            }
        }
    }
    *nodesOut = nodes;
    return true;
}

bool KeymapModel::restoreFromJson(const QString &json, QString *error)
{
    QString switchKey;
    MouseMoveMap moveMap;
    QVector<KeymapNode> nodes;
    if (!parseKeymapJson(json, error, &switchKey, &moveMap, &nodes)) {
        return false;
    }
    m_switchKey = switchKey;
    m_mouseMoveMap = moveMap;
    m_nodes = nodes;
    if (m_selectedIndex >= m_nodes.size()) {
        m_selectedIndex = -1;
        emit selectionChanged(-1);
    }
    emit modelChanged();
    return true;
}

bool KeymapModel::loadFromJson(const QString &json, QString *error)
{
    QString switchKey;
    MouseMoveMap moveMap;
    QVector<KeymapNode> nodes;
    if (!parseKeymapJson(json, error, &switchKey, &moveMap, &nodes)) {
        return false;
    }
    m_switchKey = switchKey;
    m_mouseMoveMap = moveMap;
    m_nodes = nodes;
    int oldSelection = m_selectedIndex;
    m_selectedIndex = -1;
    // A fresh document: fix overlaps against a 1080p reference, then clear
    // the undo history (nudgeOverlaps may push one entry first).
    nudgeOverlaps(QSize(1920, 1080));
    m_undoStack.clear();
    m_redoStack.clear();
    emit modelChanged();
    if (oldSelection != -1) {
        emit selectionChanged(-1);
    }
    return true;
}

QString KeymapModel::toJson() const
{
    QJsonObject root;
    root.insert("switchKey", m_switchKey);
    if (m_mouseMoveMap.present) {
        QJsonObject mm;
        mm.insert("startPos", posToJson(m_mouseMoveMap.startPos));
        mm.insert("speedRatioX", m_mouseMoveMap.speedRatioX);
        mm.insert("speedRatioY", m_mouseMoveMap.speedRatioY);
        if (m_mouseMoveMap.smallEyes) {
            QJsonObject se;
            se.insert("type", "KMT_CLICK");
            se.insert("key", m_mouseMoveMap.smallEyesKey);
            se.insert("pos", posToJson(m_mouseMoveMap.smallEyesPos));
            se.insert("switchMap", false);
            mm.insert("smallEyes", se);
        }
        root.insert("mouseMoveMap", mm);
    }
    QJsonArray nodes;
    for (int i = 0; i < m_nodes.size(); ++i) {
        nodes.append(nodeToJson(m_nodes.at(i)));
    }
    root.insert("keyMapNodes", nodes);
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

QString KeymapModel::switchKey() const
{
    return m_switchKey;
}

void KeymapModel::setSwitchKey(const QString &k)
{
    if (k == m_switchKey) {
        return;
    }
    pushUndo();
    m_switchKey = k;
    emit modelChanged();
}

MouseMoveMap KeymapModel::mouseMoveMap() const
{
    return m_mouseMoveMap;
}

void KeymapModel::setMouseMoveMap(const MouseMoveMap &m)
{
    if (m.present == m_mouseMoveMap.present && m.startPos == m_mouseMoveMap.startPos
        && m.speedRatioX == m_mouseMoveMap.speedRatioX && m.speedRatioY == m_mouseMoveMap.speedRatioY
        && m.smallEyes == m_mouseMoveMap.smallEyes && m.smallEyesKey == m_mouseMoveMap.smallEyesKey
        && m.smallEyesPos == m_mouseMoveMap.smallEyesPos) {
        return;
    }
    pushUndo();
    m_mouseMoveMap = m;
    emit modelChanged();
}

const QVector<KeymapNode> &KeymapModel::nodes() const
{
    return m_nodes;
}

QVector<KeymapNode> &KeymapModel::nodes()
{
    return m_nodes;
}

void KeymapModel::setNodes(const QVector<KeymapNode> &n)
{
    pushUndo(); // skipped when the JSON is unchanged
    m_nodes = n;
    if (m_selectedIndex >= m_nodes.size()) {
        m_selectedIndex = -1;
        emit selectionChanged(-1);
    }
    emit modelChanged();
}

QString KeymapModel::fileName() const
{
    return m_fileName;
}

void KeymapModel::setFileName(const QString &n)
{
    if (n == m_fileName) {
        return;
    }
    // The file name is not part of the JSON document: no undo entry.
    m_fileName = n;
    emit modelChanged();
}

int KeymapModel::selectedIndex() const
{
    return m_selectedIndex;
}

void KeymapModel::setSelectedIndex(int i)
{
    if (i < -1) {
        i = -1;
    }
    if (i >= m_nodes.size()) {
        i = -1;
    }
    if (i == m_selectedIndex) {
        return;
    }
    m_selectedIndex = i;
    emit selectionChanged(i);
}

QVector<QPair<QString, QString> > KeymapModel::collectKeys() const
{
    QVector<QPair<QString, QString> > keys;
    if (!m_switchKey.isEmpty()) {
        keys.append(qMakePair(m_switchKey, QString::fromUtf8("switchKey")));
    }
    for (int i = 0; i < m_nodes.size(); ++i) {
        const KeymapNode &node = m_nodes.at(i);
        QString label = nodeLabel(node);
        if (node.type == "KMT_STEER_WHEEL") {
            if (!node.leftKey.isEmpty()) {
                keys.append(qMakePair(node.leftKey, label));
            }
            if (!node.rightKey.isEmpty()) {
                keys.append(qMakePair(node.rightKey, label));
            }
            if (!node.upKey.isEmpty()) {
                keys.append(qMakePair(node.upKey, label));
            }
            if (!node.downKey.isEmpty()) {
                keys.append(qMakePair(node.downKey, label));
            }
        } else if (!node.key.isEmpty()) {
            keys.append(qMakePair(node.key, label));
        }
    }
    if (m_mouseMoveMap.present && m_mouseMoveMap.smallEyes && !m_mouseMoveMap.smallEyesKey.isEmpty()) {
        keys.append(qMakePair(m_mouseMoveMap.smallEyesKey, QString::fromUtf8("smallEyes")));
    }
    return keys;
}

bool KeymapModel::hasDuplicateKey(QString *dupKey) const
{
    QVector<QPair<QString, QString> > keys = collectKeys();
    for (int i = 0; i < keys.size(); ++i) {
        for (int j = i + 1; j < keys.size(); ++j) {
            if (keys.at(i).first == keys.at(j).first) {
                if (dupKey) {
                    *dupKey = keys.at(i).first;
                }
                return true;
            }
        }
    }
    return false;
}

QVector<KeymapModel::ButtonGeom> KeymapModel::buttonGeometries(const QVector<KeymapNode> &nodes,
                                                               const QSize &videoSize) const
{
    QVector<ButtonGeom> geoms;
    int size = buttonPixelSize(videoSize);
    double half = size / 2.0;
    for (int i = 0; i < nodes.size(); ++i) {
        const KeymapNode &node = nodes.at(i);
        QString label = nodeLabel(node);
        if (node.type == "KMT_CLICK" || node.type == "KMT_CLICK_TWICE") {
            ButtonGeom g;
            g.centre = QPointF(node.pos.x() * videoSize.width(), node.pos.y() * videoSize.height());
            g.rect = QRectF(g.centre.x() - half, g.centre.y() - half, size, size);
            g.label = label;
            g.nodeIndex = i;
            g.tapIndex = -1;
            geoms.append(g);
        } else if (node.type == "KMT_CLICK_MULTI") {
            // Each tap point is a real touch point on screen: test them one
            // by one. The union of the tap points would be a huge empty
            // rectangle (the two taps of a multi are far apart) and would
            // wrongly collide with every button in between.
            for (int t = 0; t < node.clickNodes.size(); ++t) {
                QPointF p = node.clickNodes.at(t).second;
                ButtonGeom g;
                g.centre = QPointF(p.x() * videoSize.width(), p.y() * videoSize.height());
                g.rect = QRectF(g.centre.x() - half, g.centre.y() - half, size, size);
                g.label = label + QString::fromUtf8(" (point %1)").arg(t + 1);
                g.nodeIndex = i;
                g.tapIndex = t;
                geoms.append(g);
            }
        } else if (node.type == "KMT_STEER_WHEEL") {
            ButtonGeom g;
            g.centre = QPointF(node.pos.x() * videoSize.width(), node.pos.y() * videoSize.height());
            double x0 = (node.pos.x() - node.leftOffset) * videoSize.width();
            double x1 = (node.pos.x() + node.rightOffset) * videoSize.width();
            double y0 = (node.pos.y() - node.upOffset) * videoSize.height();
            double y1 = (node.pos.y() + node.downOffset) * videoSize.height();
            // Exact centre +/- offsets box, with no extra margin: the wheel
            // is a cross (centre + 4 direction dots), not a solid block.
            g.rect = QRectF(QPointF(x0, y0), QPointF(x1, y1)).normalized();
            g.label = label;
            g.nodeIndex = i;
            g.tapIndex = -1;
            geoms.append(g);
        } else if (node.type == "KMT_DRAG") {
            ButtonGeom g;
            g.centre = QPointF(node.pos.x() * videoSize.width(), node.pos.y() * videoSize.height());
            g.rect = QRectF(g.centre.x() - half, g.centre.y() - half, size, size);
            g.label = label;
            g.nodeIndex = i;
            g.tapIndex = -1;
            geoms.append(g);
        }
        // KMT_ANDROID_KEY has no screen position: skipped.
        // mouseMoveMap is skipped on purpose (it is not a button).
    }
    return geoms;
}

bool KeymapModel::validate(QString *error, const QSize &videoSize) const
{
    if (m_switchKey.isEmpty() || !isValidKeyName(m_switchKey)) {
        if (error) {
            *error = QString::fromUtf8("switchKey manquante ou invalide");
        }
        return false;
    }

    // Every node must have its key(s).
    for (int i = 0; i < m_nodes.size(); ++i) {
        const KeymapNode &node = m_nodes.at(i);
        QString label = nodeLabel(node);
        if (node.type == "KMT_STEER_WHEEL") {
            if (node.leftKey.isEmpty() || node.rightKey.isEmpty() || node.upKey.isEmpty()
                || node.downKey.isEmpty()) {
                if (error) {
                    *error = QString::fromUtf8("La roue « %1 » n'a pas ses 4 touches de direction").arg(label);
                }
                return false;
            }
        } else if (node.type == "KMT_CLICK_MULTI") {
            if (node.key.isEmpty()) {
                if (error) {
                    *error = QString::fromUtf8("Le bouton multi « %1 » n'a pas de touche").arg(label);
                }
                return false;
            }
            if (node.clickNodes.isEmpty()) {
                if (error) {
                    *error = QString::fromUtf8("Le bouton multi « %1 » n'a aucun point").arg(label);
                }
                return false;
            }
        } else if (node.type == "KMT_CLICK" || node.type == "KMT_CLICK_TWICE" || node.type == "KMT_DRAG"
                   || node.type == "KMT_ANDROID_KEY") {
            if (node.key.isEmpty()) {
                if (error) {
                    *error = QString::fromUtf8("Le bouton « %1 » n'a pas de touche").arg(label);
                }
                return false;
            }
        }
    }

    // Duplicate keys (gameplay keys, wheel direction keys, smallEyes key,
    // and switchKey against gameplay keys).
    QVector<QPair<QString, QString> > keys = collectKeys();
    for (int i = 0; i < keys.size(); ++i) {
        for (int j = i + 1; j < keys.size(); ++j) {
            if (keys.at(i).first == keys.at(j).first) {
                if (error) {
                    *error = QString::fromUtf8("La touche « %1 » est utilisée deux fois : par « %2 » et par « %3 »")
                                 .arg(keyDisplayName(keys.at(i).first), keys.at(i).second, keys.at(j).second);
                }
                return false;
            }
        }
    }

    // All positions inside [0,1].
    for (int i = 0; i < m_nodes.size(); ++i) {
        const KeymapNode &node = m_nodes.at(i);
        QString label = nodeLabel(node);
        QVector<QPointF> positions;
        if (node.type == "KMT_CLICK" || node.type == "KMT_CLICK_TWICE" || node.type == "KMT_STEER_WHEEL") {
            positions.append(node.pos);
        } else if (node.type == "KMT_CLICK_MULTI") {
            for (int t = 0; t < node.clickNodes.size(); ++t) {
                positions.append(node.clickNodes.at(t).second);
            }
        } else if (node.type == "KMT_DRAG") {
            positions.append(node.pos);
            positions.append(node.endPos);
        }
        for (int p = 0; p < positions.size(); ++p) {
            QPointF pos = positions.at(p);
            if (pos.x() < 0 || pos.x() > 1 || pos.y() < 0 || pos.y() > 1) {
                if (error) {
                    *error = QString::fromUtf8("La position du bouton « %1 » est hors de l'écran").arg(label);
                }
                return false;
            }
        }
    }
    if (m_mouseMoveMap.present) {
        QPointF startPos = m_mouseMoveMap.startPos;
        if (startPos.x() < 0 || startPos.x() > 1 || startPos.y() < 0 || startPos.y() > 1) {
            if (error) {
                *error = QString::fromUtf8("La position de départ de la visée souris est hors de l'écran");
            }
            return false;
        }
        if (m_mouseMoveMap.smallEyes) {
            QPointF sePos = m_mouseMoveMap.smallEyesPos;
            if (sePos.x() < 0 || sePos.x() > 1 || sePos.y() < 0 || sePos.y() > 1) {
                if (error) {
                    *error = QString::fromUtf8("La position de smallEyes est hors de l'écran");
                }
                return false;
            }
        }
    }

    // Overlaps: rectangles intersect, or centres closer than 48 px.
    QVector<ButtonGeom> geoms = buttonGeometries(m_nodes, videoSize);
    for (int i = 0; i < geoms.size(); ++i) {
        for (int j = i + 1; j < geoms.size(); ++j) {
            if (positionsOverlap(geoms.at(i).rect, geoms.at(i).centre, geoms.at(j).rect, geoms.at(j).centre)) {
                if (error) {
                    *error = QString::fromUtf8("Les boutons « %1 » et « %2 » se chevauchent")
                                 .arg(geoms.at(i).label, geoms.at(j).label);
                }
                return false;
            }
        }
    }
    return true;
}

void KeymapModel::nudgeOverlaps(const QSize &videoSize)
{
    if (videoSize.width() <= 0 || videoSize.height() <= 0) {
        return;
    }
    QVector<KeymapNode> newNodes = m_nodes;
    MouseMoveMap newMap = m_mouseMoveMap;
    bool changed = false;

    // Clamp every position into [0,1] first.
    for (int i = 0; i < newNodes.size(); ++i) {
        KeymapNode &node = newNodes[i];
        if (node.type == "KMT_CLICK" || node.type == "KMT_CLICK_TWICE" || node.type == "KMT_STEER_WHEEL") {
            QPointF clamped(qBound(0.0, node.pos.x(), 1.0), qBound(0.0, node.pos.y(), 1.0));
            if (clamped != node.pos) {
                node.pos = clamped;
                changed = true;
            }
        } else if (node.type == "KMT_CLICK_MULTI") {
            for (int t = 0; t < node.clickNodes.size(); ++t) {
                QPointF p = node.clickNodes[t].second;
                QPointF clamped(qBound(0.0, p.x(), 1.0), qBound(0.0, p.y(), 1.0));
                if (clamped != p) {
                    node.clickNodes[t].second = clamped;
                    changed = true;
                }
            }
        } else if (node.type == "KMT_DRAG") {
            QPointF startClamped(qBound(0.0, node.pos.x(), 1.0), qBound(0.0, node.pos.y(), 1.0));
            QPointF endClamped(qBound(0.0, node.endPos.x(), 1.0), qBound(0.0, node.endPos.y(), 1.0));
            if (startClamped != node.pos) {
                node.pos = startClamped;
                changed = true;
            }
            if (endClamped != node.endPos) {
                node.endPos = endClamped;
                changed = true;
            }
        }
    }
    if (newMap.present) {
        QPointF startClamped(qBound(0.0, newMap.startPos.x(), 1.0), qBound(0.0, newMap.startPos.y(), 1.0));
        if (startClamped != newMap.startPos) {
            newMap.startPos = startClamped;
            changed = true;
        }
        if (newMap.smallEyes) {
            QPointF seClamped(qBound(0.0, newMap.smallEyesPos.x(), 1.0),
                              qBound(0.0, newMap.smallEyesPos.y(), 1.0));
            if (seClamped != newMap.smallEyesPos) {
                newMap.smallEyesPos = seClamped;
                changed = true;
            }
        }
    }

    // Move any overlapping button to the nearest free cell of a 12x8 grid.
    // Buttons are placed in order; a moved button joins the placed set, so
    // later buttons avoid it too.
    QVector<ButtonGeom> geoms = buttonGeometries(newNodes, videoSize);
    QVector<ButtonGeom> placed;
    double cellW = videoSize.width() / 12.0;
    double cellH = videoSize.height() / 8.0;
    for (int g = 0; g < geoms.size(); ++g) {
        ButtonGeom geom = geoms.at(g);
        bool overlaps = false;
        for (int p = 0; p < placed.size(); ++p) {
            if (positionsOverlap(geom.rect, geom.centre, placed.at(p).rect, placed.at(p).centre)) {
                overlaps = true;
                break;
            }
        }
        if (!overlaps) {
            placed.append(geom);
            continue;
        }
        QPointF rectOffset = geom.rect.center() - geom.centre;
        QPointF bestCentre;
        bool found = false;
        double bestDistance = 0;
        for (int row = 0; row < 8; ++row) {
            for (int col = 0; col < 12; ++col) {
                QPointF cellCentre((col + 0.5) * cellW, (row + 0.5) * cellH);
                double halfW = geom.rect.width() / 2.0;
                double halfH = geom.rect.height() / 2.0;
                double cx = qBound(halfW, cellCentre.x() + rectOffset.x(), videoSize.width() - halfW);
                double cy = qBound(halfH, cellCentre.y() + rectOffset.y(), videoSize.height() - halfH);
                QPointF candidate(cx - rectOffset.x(), cy - rectOffset.y());
                QRectF candidateRect(candidate.x() + rectOffset.x() - halfW,
                                     candidate.y() + rectOffset.y() - halfH, geom.rect.width(),
                                     geom.rect.height());
                bool free = true;
                for (int p = 0; p < placed.size(); ++p) {
                    if (positionsOverlap(candidateRect, candidate, placed.at(p).rect, placed.at(p).centre)) {
                        free = false;
                        break;
                    }
                }
                if (!free) {
                    continue;
                }
                double dx = candidate.x() - geom.centre.x();
                double dy = candidate.y() - geom.centre.y();
                double distance = qSqrt(dx * dx + dy * dy);
                if (!found || distance < bestDistance) {
                    found = true;
                    bestDistance = distance;
                    bestCentre = candidate;
                }
            }
        }
        if (!found) {
            placed.append(geom); // no free cell: leave it where it is
            continue;
        }
        QPointF rel(bestCentre.x() / videoSize.width(), bestCentre.y() / videoSize.height());
        KeymapNode &node = newNodes[geom.nodeIndex];
        if (geom.tapIndex >= 0) {
            node.clickNodes[geom.tapIndex].second = rel;
        } else {
            node.pos = rel;
        }
        geom.centre = bestCentre;
        geom.rect = QRectF(bestCentre.x() + rectOffset.x() - geom.rect.width() / 2.0,
                           bestCentre.y() + rectOffset.y() - geom.rect.height() / 2.0, geom.rect.width(),
                           geom.rect.height());
        placed.append(geom);
        changed = true;
    }

    if (!changed) {
        return;
    }
    pushUndo();
    m_nodes = newNodes;
    m_mouseMoveMap = newMap;
    if (m_selectedIndex >= m_nodes.size()) {
        m_selectedIndex = -1;
        emit selectionChanged(-1);
    }
    emit modelChanged();
}

QString KeymapModel::convertDelayClickNodes(const QString &json)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return json;
    }
    QJsonObject root = doc.object();
    if (!root.contains("keyMapNodes") || !root.value("keyMapNodes").isArray()) {
        return json;
    }
    QJsonArray nodes = root.value("keyMapNodes").toArray();
    bool changed = false;
    for (int i = 0; i < nodes.size(); ++i) {
        if (!nodes.at(i).isObject()) {
            continue;
        }
        QJsonObject node = nodes.at(i).toObject();
        if (!node.contains("delayClickNodes")) {
            continue;
        }
        if (!node.contains("clickNodes")) {
            node.insert("clickNodes", node.value("delayClickNodes"));
        }
        node.remove("delayClickNodes");
        nodes.replace(i, node);
        changed = true;
    }
    if (!changed) {
        return json;
    }
    root.insert("keyMapNodes", nodes);
    doc.setObject(root);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}

QString KeymapModel::keyNameFromKeyEvent(QKeyEvent *event)
{
    if (!event) {
        return QString();
    }
    int key = event->key();
    if (key == 0 || key == Qt::Key_unknown) {
        return QString();
    }
    // Modifier-only keys: no rebind target on their own.
    switch (key) {
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Alt:
    case Qt::Key_Meta:
    case Qt::Key_AltGr:
    case Qt::Key_CapsLock:
    case Qt::Key_NumLock:
    case Qt::Key_ScrollLock:
    case Qt::Key_Super_L:
    case Qt::Key_Super_R:
    case Qt::Key_Hyper_L:
    case Qt::Key_Hyper_R:
        return QString();
    default:
        break;
    }
    const char *name = QMetaEnum::fromType<Qt::Key>().valueToKey(key);
    if (!name) {
        return QString();
    }
    return QString::fromLatin1(name);
}

QString KeymapModel::keyDisplayName(const QString &keyName)
{
    QString name = keyName;
    if (name.startsWith("Key_")) {
        name = name.mid(4);
    }
    if (name == "Ampersand") {
        return "&";
    }
    if (name == "Eacute") {
        return QString(QChar(0x00E9));
    }
    if (name == "QuoteDbl") {
        return "\"";
    }
    if (name == "Apostrophe") {
        return "'";
    }
    if (name == "ParenLeft") {
        return "(";
    }
    if (name == "Control") {
        return "Ctrl";
    }
    if (name == "CapsLock") {
        return "Caps";
    }
    if (name == "LeftButton") {
        return "LMB";
    }
    if (name == "RightButton") {
        return "RMB";
    }
    if (name == "MiddleButton") {
        return "MMB";
    }
    return name;
}

bool KeymapModel::isMouseButtonKey(const QString &keyName)
{
    return keyName == "LeftButton" || keyName == "RightButton" || keyName == "MiddleButton";
}

int KeymapModel::buttonPixelSize(const QSize &videoSize)
{
    if (videoSize.height() <= 0) {
        return 60;
    }
    return qMax(28, qRound(60.0 * videoSize.height() / 1080.0));
}

void KeymapModel::pushUndo()
{
    QString json = toJson();
    // A new edit invalidates redo even when the snapshot itself is skipped
    // as a duplicate (the model may already sit on the top of the stack
    // after an undo, and the redo entry must still go away).
    m_redoStack.clear();
    if (!m_undoStack.isEmpty() && m_undoStack.last() == json) {
        return; // nothing changed since the last snapshot
    }
    m_undoStack.append(json);
    if (m_undoStack.size() > 50) {
        m_undoStack.removeFirst();
    }
}

void KeymapModel::undo()
{
    if (m_undoStack.isEmpty()) {
        return;
    }
    QString current = toJson();
    QString previous = m_undoStack.takeLast();
    if (m_redoStack.isEmpty() || m_redoStack.last() != current) {
        m_redoStack.append(current);
        if (m_redoStack.size() > 50) {
            m_redoStack.removeFirst();
        }
    }
    QString error;
    if (!restoreFromJson(previous, &error)) {
        m_undoStack.append(previous); // snapshot should always parse
    }
}

void KeymapModel::redo()
{
    if (m_redoStack.isEmpty()) {
        return;
    }
    QString current = toJson();
    QString next = m_redoStack.takeLast();
    if (m_undoStack.isEmpty() || m_undoStack.last() != current) {
        m_undoStack.append(current);
        if (m_undoStack.size() > 50) {
            m_undoStack.removeFirst();
        }
    }
    QString error;
    if (!restoreFromJson(next, &error)) {
        m_redoStack.append(next); // snapshot should always parse
    }
}

bool KeymapModel::canUndo() const
{
    return !m_undoStack.isEmpty();
}

bool KeymapModel::canRedo() const
{
    return !m_redoStack.isEmpty();
}
