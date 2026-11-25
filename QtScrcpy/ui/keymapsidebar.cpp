// Mirrorff — fork of QtScrcpy v4.2.1 (Apache-2.0)
#include "keymapsidebar.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include "keymapio.h"
#include "keymapoverlay.h"

static void clearLayout(QLayout *layout)
{
    QLayoutItem *item;
    while ((item = layout->takeAt(0)) != 0) {
        if (item->widget()) {
            delete item->widget();
        } else if (item->layout()) {
            clearLayout(item->layout());
            delete item->layout();
        }
        delete item;
    }
}

KeymapSidebar::KeymapSidebar(QWidget *parent)
    : QWidget(parent)
    , m_model(0)
    , m_overlay(0)
    , m_editMode(false)
    , m_audioRunning(false)
    , m_capturingSwitchKey(false)
    , m_capturingNodeKey(false)
    , m_capturingSmallEyesKey(false)
    , m_captureNodeIndex(-1)
    , m_scrollArea(0)
    , m_content(0)
    , m_modeButton(0)
    , m_modeLabel(0)
    , m_deleteButton(0)
    , m_duplicateButton(0)
    , m_undoButton(0)
    , m_redoButton(0)
    , m_saveButton(0)
    , m_importButton(0)
    , m_exportButton(0)
    , m_openFolderButton(0)
    , m_fileNameLabel(0)
    , m_switchKeyButton(0)
    , m_propertiesContent(0)
    , m_propertiesLayout(0)
    , m_lookXSpin(0)
    , m_lookYSpin(0)
    , m_ratioXSpin(0)
    , m_ratioYSpin(0)
    , m_smallEyesCheck(0)
    , m_smallEyesKeyButton(0)
    , m_smallEyesXSpin(0)
    , m_smallEyesYSpin(0)
    , m_showCrosshairCheck(0)
    , m_audioLabel(0)
{
    setFixedWidth(220);
    setObjectName("keymapSidebar");
    setFocusPolicy(Qt::StrongFocus);
    // Small local stylesheet for section titles only; the rest inherits
    // the application stylesheet.
    setStyleSheet("QGroupBox::title { font-weight: bold; }");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    mainLayout->addWidget(m_scrollArea);

    m_content = new QWidget;
    QVBoxLayout *contentLayout = new QVBoxLayout(m_content);
    contentLayout->setContentsMargins(2, 2, 2, 2);
    m_scrollArea->setWidget(m_content);

    // 1. Mode
    {
        QWidget *sectionContent = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout(sectionContent);
        layout->setContentsMargins(2, 2, 2, 2);
        m_modeButton = new QPushButton(QString::fromUtf8("Éditer (F9)"));
        m_modeButton->setCheckable(true);
        m_modeButton->setToolTip(QString::fromUtf8("تبديل التعديل / اللعب"));
        m_modeLabel = new QLabel(QString::fromUtf8("Mode : Jeu"));
        layout->addWidget(m_modeButton);
        layout->addWidget(m_modeLabel);
        connect(m_modeButton, &QPushButton::toggled, this, &KeymapSidebar::editModeToggled);
        contentLayout->addWidget(makeSection(QString::fromUtf8("Mode"), sectionContent));
    }

    // 2. Ajouter
    {
        QWidget *sectionContent = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout(sectionContent);
        layout->setContentsMargins(2, 2, 2, 2);
        QPushButton *addClickButton = new QPushButton(QString::fromUtf8("+ Clic"));
        addClickButton->setToolTip(QString::fromUtf8("إضافة زر نقرة"));
        QPushButton *addMultiButton = new QPushButton(QString::fromUtf8("+ Multi"));
        addMultiButton->setToolTip(QString::fromUtf8("إضافة زر متعدد النقرات"));
        QPushButton *addWheelButton = new QPushButton(QString::fromUtf8("+ Roue"));
        addWheelButton->setToolTip(QString::fromUtf8("إضافة عصا التحكم"));
        layout->addWidget(addClickButton);
        layout->addWidget(addMultiButton);
        layout->addWidget(addWheelButton);
        connect(addClickButton, &QPushButton::clicked, this, &KeymapSidebar::addClickRequested);
        connect(addMultiButton, &QPushButton::clicked, this, &KeymapSidebar::addMultiRequested);
        connect(addWheelButton, &QPushButton::clicked, this, &KeymapSidebar::addWheelRequested);
        contentLayout->addWidget(makeSection(QString::fromUtf8("Ajouter"), sectionContent));
    }

    // 3. Éditer
    {
        QWidget *sectionContent = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout(sectionContent);
        layout->setContentsMargins(2, 2, 2, 2);
        m_deleteButton = new QPushButton(QString::fromUtf8("Supprimer"));
        m_duplicateButton = new QPushButton(QString::fromUtf8("Dupliquer"));
        m_undoButton = new QPushButton(QString::fromUtf8("Annuler (Ctrl+Z)"));
        m_redoButton = new QPushButton(QString::fromUtf8("Rétablir (Ctrl+Y)"));
        layout->addWidget(m_deleteButton);
        layout->addWidget(m_duplicateButton);
        layout->addWidget(m_undoButton);
        layout->addWidget(m_redoButton);
        connect(m_deleteButton, &QPushButton::clicked, this, &KeymapSidebar::deleteRequested);
        connect(m_duplicateButton, &QPushButton::clicked, this, &KeymapSidebar::duplicateRequested);
        connect(m_undoButton, &QPushButton::clicked, this, &KeymapSidebar::undoRequested);
        connect(m_redoButton, &QPushButton::clicked, this, &KeymapSidebar::redoRequested);
        contentLayout->addWidget(makeSection(QString::fromUtf8("Éditer"), sectionContent));
    }

    // 4. Keymap
    {
        QWidget *sectionContent = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout(sectionContent);
        layout->setContentsMargins(2, 2, 2, 2);
        m_saveButton = new QPushButton(QString::fromUtf8("Enregistrer & Lier"));
        QFont saveFont = m_saveButton->font();
        saveFont.setBold(true);
        m_saveButton->setFont(saveFont);
        m_saveButton->setToolTip(QString::fromUtf8("حفظ و ربط الملف"));
        m_importButton = new QPushButton(QString::fromUtf8("Importer…"));
        m_exportButton = new QPushButton(QString::fromUtf8("Exporter une copie…"));
        m_openFolderButton = new QPushButton(QString::fromUtf8("Ouvrir le dossier"));
        m_fileNameLabel = new QLabel("—");
        m_fileNameLabel->setWordWrap(true);
        QHBoxLayout *switchKeyRow = new QHBoxLayout;
        QLabel *switchKeyLabel = new QLabel("switchKey");
        m_switchKeyButton = new QPushButton("—");
        m_switchKeyButton->setToolTip(QString::fromUtf8("cliquer puis appuyer sur une touche"));
        switchKeyRow->addWidget(switchKeyLabel);
        switchKeyRow->addWidget(m_switchKeyButton);
        layout->addWidget(m_saveButton);
        layout->addWidget(m_importButton);
        layout->addWidget(m_exportButton);
        layout->addWidget(m_openFolderButton);
        layout->addWidget(m_fileNameLabel);
        layout->addLayout(switchKeyRow);
        connect(m_saveButton, &QPushButton::clicked, this, &KeymapSidebar::saveRequested);
        connect(m_importButton, &QPushButton::clicked, this, &KeymapSidebar::importRequested);
        connect(m_exportButton, &QPushButton::clicked, this, &KeymapSidebar::exportCopyRequested);
        connect(m_openFolderButton, &QPushButton::clicked, this, &KeymapSidebar::openKeymapFolderRequested);
        connect(m_switchKeyButton, &QPushButton::clicked, this, &KeymapSidebar::onSwitchKeyButtonClicked);
        contentLayout->addWidget(makeSection(QString::fromUtf8("Keymap"), sectionContent));
    }

    // 5. Propriétés (rebuilt by refreshAll)
    {
        m_propertiesContent = new QWidget;
        m_propertiesLayout = new QVBoxLayout(m_propertiesContent);
        m_propertiesLayout->setContentsMargins(2, 2, 2, 2);
        contentLayout->addWidget(makeSection(QString::fromUtf8("Propriétés"), m_propertiesContent));
    }

    // 6. Visée souris
    {
        QWidget *sectionContent = new QWidget;
        QFormLayout *form = new QFormLayout(sectionContent);
        form->setContentsMargins(2, 2, 2, 2);
        m_lookXSpin = new QDoubleSpinBox;
        m_lookXSpin->setRange(0, 1);
        m_lookXSpin->setDecimals(4);
        m_lookXSpin->setSingleStep(0.001);
        m_lookYSpin = new QDoubleSpinBox;
        m_lookYSpin->setRange(0, 1);
        m_lookYSpin->setDecimals(4);
        m_lookYSpin->setSingleStep(0.001);
        m_ratioXSpin = new QDoubleSpinBox;
        m_ratioXSpin->setRange(0.1, 10);
        m_ratioXSpin->setDecimals(2);
        m_ratioXSpin->setSingleStep(0.1);
        m_ratioYSpin = new QDoubleSpinBox;
        m_ratioYSpin->setRange(0.1, 10);
        m_ratioYSpin->setDecimals(2);
        m_ratioYSpin->setSingleStep(0.1);
        m_smallEyesCheck = new QCheckBox("smallEyes");
        m_smallEyesCheck->setToolTip(QString::fromUtf8("النقر بالعين الصغيرة"));
        m_smallEyesKeyButton = new QPushButton(QString::fromUtf8("⌨"));
        m_smallEyesKeyButton->setToolTip(QString::fromUtf8("cliquer puis appuyer sur une touche"));
        m_smallEyesXSpin = new QDoubleSpinBox;
        m_smallEyesXSpin->setRange(0, 1);
        m_smallEyesXSpin->setDecimals(4);
        m_smallEyesXSpin->setSingleStep(0.001);
        m_smallEyesYSpin = new QDoubleSpinBox;
        m_smallEyesYSpin->setRange(0, 1);
        m_smallEyesYSpin->setDecimals(4);
        m_smallEyesYSpin->setSingleStep(0.001);
        m_showCrosshairCheck = new QCheckBox(QString::fromUtf8("Afficher la visée"));
        form->addRow("X", m_lookXSpin);
        form->addRow("Y", m_lookYSpin);
        form->addRow(QString::fromUtf8("Vitesse X"), m_ratioXSpin);
        form->addRow(QString::fromUtf8("Vitesse Y"), m_ratioYSpin);
        QHBoxLayout *smallEyesRow = new QHBoxLayout;
        smallEyesRow->addWidget(m_smallEyesCheck);
        smallEyesRow->addWidget(m_smallEyesKeyButton);
        form->addRow("smallEyes", smallEyesRow);
        form->addRow(QString::fromUtf8("smallEyes X"), m_smallEyesXSpin);
        form->addRow(QString::fromUtf8("smallEyes Y"), m_smallEyesYSpin);
        form->addRow(m_showCrosshairCheck);
        connect(m_lookXSpin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this,
                [this](double value) {
                    if (!m_model || !m_model->mouseMoveMap().present) {
                        return;
                    }
                    MouseMoveMap map = m_model->mouseMoveMap();
                    map.startPos.setX(value);
                    m_model->setMouseMoveMap(map);
                });
        connect(m_lookYSpin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this,
                [this](double value) {
                    if (!m_model || !m_model->mouseMoveMap().present) {
                        return;
                    }
                    MouseMoveMap map = m_model->mouseMoveMap();
                    map.startPos.setY(value);
                    m_model->setMouseMoveMap(map);
                });
        connect(m_ratioXSpin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this,
                [this](double value) {
                    if (!m_model || !m_model->mouseMoveMap().present) {
                        return;
                    }
                    MouseMoveMap map = m_model->mouseMoveMap();
                    map.speedRatioX = value;
                    m_model->setMouseMoveMap(map);
                });
        connect(m_ratioYSpin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this,
                [this](double value) {
                    if (!m_model || !m_model->mouseMoveMap().present) {
                        return;
                    }
                    MouseMoveMap map = m_model->mouseMoveMap();
                    map.speedRatioY = value;
                    m_model->setMouseMoveMap(map);
                });
        connect(m_smallEyesCheck, &QCheckBox::toggled, this, [this](bool checked) {
            if (!m_model || !m_model->mouseMoveMap().present) {
                return;
            }
            MouseMoveMap map = m_model->mouseMoveMap();
            map.smallEyes = checked;
            if (checked && map.smallEyesKey.isEmpty()) {
                map.smallEyesKey = "Key_Alt";
                map.smallEyesPos = QPointF(0.5, 0.4);
            }
            m_model->setMouseMoveMap(map);
        });
        connect(m_smallEyesXSpin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
                this, [this](double value) {
                    if (!m_model || !m_model->mouseMoveMap().present) {
                        return;
                    }
                    MouseMoveMap map = m_model->mouseMoveMap();
                    map.smallEyesPos.setX(value);
                    m_model->setMouseMoveMap(map);
                });
        connect(m_smallEyesYSpin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged),
                this, [this](double value) {
                    if (!m_model || !m_model->mouseMoveMap().present) {
                        return;
                    }
                    MouseMoveMap map = m_model->mouseMoveMap();
                    map.smallEyesPos.setY(value);
                    m_model->setMouseMoveMap(map);
                });
        connect(m_showCrosshairCheck, &QCheckBox::toggled, this, [this](bool checked) {
            if (m_overlay) {
                m_overlay->setShowCrosshair(checked);
            }
        });
        connect(m_smallEyesKeyButton, &QPushButton::clicked, this,
                &KeymapSidebar::onSmallEyesKeyButtonClicked);
        contentLayout->addWidget(makeSection(QString::fromUtf8("Visée souris"), sectionContent));
    }

    // 7. Audio
    {
        QWidget *sectionContent = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout(sectionContent);
        layout->setContentsMargins(2, 2, 2, 2);
        QPushButton *installButton = new QPushButton(QString::fromUtf8("Installer Sndcpy"));
        QPushButton *startButton = new QPushButton(QString::fromUtf8("Démarrer audio"));
        QPushButton *stopButton = new QPushButton(QString::fromUtf8("Arrêter audio"));
        m_audioLabel = new QLabel(QString::fromUtf8("Audio : arrêté"));
        layout->addWidget(installButton);
        layout->addWidget(startButton);
        layout->addWidget(stopButton);
        layout->addWidget(m_audioLabel);
        connect(installButton, &QPushButton::clicked, this, &KeymapSidebar::installSndcpyRequested);
        connect(startButton, &QPushButton::clicked, this, &KeymapSidebar::startAudioRequested);
        connect(stopButton, &QPushButton::clicked, this, &KeymapSidebar::stopAudioRequested);
        contentLayout->addWidget(makeSection(QString::fromUtf8("Audio"), sectionContent));
    }

    // 8. Fenêtre
    {
        QWidget *sectionContent = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout(sectionContent);
        layout->setContentsMargins(2, 2, 2, 2);
        QPushButton *fullscreenButton = new QPushButton(QString::fromUtf8("Plein écran"));
        QPushButton *rotateButton = new QPushButton(QString::fromUtf8("Pivoter"));
        QPushButton *screenshotButton = new QPushButton(QString::fromUtf8("Capture d'écran"));
        layout->addWidget(fullscreenButton);
        layout->addWidget(rotateButton);
        layout->addWidget(screenshotButton);
        connect(fullscreenButton, &QPushButton::clicked, this, &KeymapSidebar::fullscreenRequested);
        connect(rotateButton, &QPushButton::clicked, this, &KeymapSidebar::rotateRequested);
        connect(screenshotButton, &QPushButton::clicked, this, &KeymapSidebar::screenshotRequested);
        contentLayout->addWidget(makeSection(QString::fromUtf8("Fenêtre"), sectionContent));
    }

    contentLayout->addStretch(1);
    refreshAll();
}

QGroupBox *KeymapSidebar::makeSection(const QString &title, QWidget *content)
{
    QGroupBox *box = new QGroupBox(title);
    box->setCheckable(true);
    box->setChecked(true);
    QVBoxLayout *boxLayout = new QVBoxLayout(box);
    boxLayout->setContentsMargins(4, 12, 4, 4);
    boxLayout->addWidget(content);
    connect(box, &QGroupBox::toggled, content, &QWidget::setVisible);
    return box;
}

void KeymapSidebar::setOverlay(KeymapOverlay *overlay)
{
    m_overlay = overlay;
    refreshAll();
}

void KeymapSidebar::setModel(KeymapModel *model)
{
    if (m_model) {
        disconnect(m_model, 0, this, 0);
    }
    m_model = model;
    if (m_model) {
        connect(m_model, &KeymapModel::modelChanged, this, &KeymapSidebar::onModelChanged);
        connect(m_model, &KeymapModel::selectionChanged, this, &KeymapSidebar::onSelectionChanged);
    }
    refreshAll();
}

void KeymapSidebar::setFileName(const QString &name)
{
    if (m_model) {
        m_model->setFileName(name);
    }
    if (m_fileNameLabel) {
        m_fileNameLabel->setText(name.isEmpty() ? "—" : name);
    }
}

void KeymapSidebar::setSwitchKey(const QString &key)
{
    if (m_model) {
        m_model->setSwitchKey(key); // emits modelChanged, refreshAll follows
    }
}

void KeymapSidebar::setEditMode(bool on)
{
    m_editMode = on;
    refreshAll();
}

void KeymapSidebar::setAudioRunning(bool running)
{
    m_audioRunning = running;
    if (m_audioLabel) {
        m_audioLabel->setText(running ? QString::fromUtf8("Audio : en cours")
                                      : QString::fromUtf8("Audio : arrêté"));
    }
}

void KeymapSidebar::onModelChanged()
{
    refreshAll();
}

void KeymapSidebar::onSelectionChanged(int)
{
    refreshAll();
}

void KeymapSidebar::refreshAll()
{
    if (m_modeButton) {
        QSignalBlocker blocker(m_modeButton);
        m_modeButton->setChecked(m_editMode);
    }
    if (m_modeLabel) {
        m_modeLabel->setText(m_editMode ? QString::fromUtf8("Mode : Édition")
                                        : QString::fromUtf8("Mode : Jeu"));
    }
    bool hasSelection = m_model && m_model->selectedIndex() >= 0;
    if (m_deleteButton) {
        m_deleteButton->setEnabled(hasSelection);
    }
    if (m_duplicateButton) {
        m_duplicateButton->setEnabled(hasSelection);
    }
    if (m_undoButton) {
        m_undoButton->setEnabled(m_model && m_model->canUndo());
    }
    if (m_redoButton) {
        m_redoButton->setEnabled(m_model && m_model->canRedo());
    }
    if (m_fileNameLabel) {
        m_fileNameLabel->setText(m_model && !m_model->fileName().isEmpty() ? m_model->fileName() : "—");
    }
    if (m_switchKeyButton && !m_capturingSwitchKey) {
        m_switchKeyButton->setText(m_model ? KeymapModel::keyDisplayName(m_model->switchKey()) : "—");
        m_switchKeyButton->setEnabled(m_model != 0);
    }
    if (m_audioLabel) {
        m_audioLabel->setText(m_audioRunning ? QString::fromUtf8("Audio : en cours")
                                             : QString::fromUtf8("Audio : arrêté"));
    }

    // Mouse-look section (values set with blocked signals: no feedback loop).
    bool hasMap = m_model && m_model->mouseMoveMap().present;
    MouseMoveMap map = hasMap ? m_model->mouseMoveMap() : MouseMoveMap();
    QDoubleSpinBox *spins[4] = { m_lookXSpin, m_lookYSpin, m_ratioXSpin, m_ratioYSpin };
    double spinValues[4] = { map.startPos.x(), map.startPos.y(), map.speedRatioX, map.speedRatioY };
    for (int i = 0; i < 4; ++i) {
        if (spins[i]) {
            QSignalBlocker blocker(spins[i]);
            spins[i]->setValue(spinValues[i]);
            spins[i]->setEnabled(hasMap);
        }
    }
    if (m_smallEyesCheck) {
        QSignalBlocker blocker(m_smallEyesCheck);
        m_smallEyesCheck->setChecked(hasMap && map.smallEyes);
        m_smallEyesCheck->setEnabled(hasMap);
    }
    if (m_smallEyesKeyButton && !m_capturingSmallEyesKey) {
        m_smallEyesKeyButton->setText(hasMap && map.smallEyes && !map.smallEyesKey.isEmpty()
                                          ? KeymapModel::keyDisplayName(map.smallEyesKey)
                                          : QString::fromUtf8("⌨"));
        m_smallEyesKeyButton->setEnabled(hasMap && map.smallEyes);
    }
    QDoubleSpinBox *seSpins[2] = { m_smallEyesXSpin, m_smallEyesYSpin };
    double seValues[2] = { map.smallEyesPos.x(), map.smallEyesPos.y() };
    for (int i = 0; i < 2; ++i) {
        if (seSpins[i]) {
            QSignalBlocker blocker(seSpins[i]);
            seSpins[i]->setValue(seValues[i]);
            seSpins[i]->setEnabled(hasMap && map.smallEyes);
        }
    }
    if (m_showCrosshairCheck) {
        QSignalBlocker blocker(m_showCrosshairCheck);
        m_showCrosshairCheck->setChecked(m_overlay && m_overlay->showCrosshair());
    }

    // Properties: skip the rebuild while the user is editing inside it,
    // otherwise the editor under the cursor would be destroyed.
    QWidget *focus = QApplication::focusWidget();
    bool focusInProperties = m_propertiesContent && focus && m_propertiesContent->isAncestorOf(focus);
    if (!focusInProperties) {
        rebuildProperties();
    }
}

void KeymapSidebar::rebuildProperties()
{
    // A rebuild destroys the editors, so any node-key capture in progress
    // ends here (its button is gone).
    m_capturingNodeKey = false;
    m_captureNodeIndex = -1;
    m_nodeKeyCaptureButton = nullptr;
    if (!m_propertiesLayout) {
        return;
    }
    clearLayout(m_propertiesLayout);

    if (!m_model || m_model->selectedIndex() < 0 || m_model->selectedIndex() >= m_model->nodes().size()) {
        m_propertiesLayout->addWidget(new QLabel(QString::fromUtf8("Aucun bouton sélectionné")));
        return;
    }
    int index = m_model->selectedIndex();
    const KeymapNode &node = m_model->nodes().at(index);

    QFormLayout *form = new QFormLayout;
    m_propertiesLayout->addLayout(form);

    // Key (read-only) + capture button.
    QLineEdit *keyEdit = new QLineEdit;
    keyEdit->setReadOnly(true);
    QPushButton *captureButton = new QPushButton(QString::fromUtf8("⌨"));
    captureButton->setToolTip(QString::fromUtf8("cliquer puis appuyer sur une touche"));
    QHBoxLayout *keyRow = new QHBoxLayout;
    keyRow->addWidget(keyEdit);
    keyRow->addWidget(captureButton);
    form->addRow(QString::fromUtf8("Touche"), keyRow);
    if (node.type == "KMT_STEER_WHEEL") {
        // A wheel has four direction keys: they are rebound by
        // double-clicking a direction dot on the video, not here.
        QStringList directions;
        directions.append(KeymapModel::keyDisplayName(node.leftKey));
        directions.append(KeymapModel::keyDisplayName(node.rightKey));
        directions.append(KeymapModel::keyDisplayName(node.upKey));
        directions.append(KeymapModel::keyDisplayName(node.downKey));
        keyEdit->setText(directions.join(" "));
        keyEdit->setToolTip(QString::fromUtf8("gauche droite haut bas"));
        captureButton->setEnabled(false);
        captureButton->setToolTip(
            QString::fromUtf8("Double-cliquer sur un point de direction dans la vidéo pour changer sa touche"));
    } else {
        keyEdit->setText(KeymapModel::keyDisplayName(node.key));
        keyEdit->setToolTip(node.key);
        m_nodeKeyCaptureButton = captureButton;
        connect(captureButton, &QPushButton::clicked, this,
                [this, index]() { startNodeKeyCapture(index); });
        // Modifier dropdown next to the key field: modifier-only keys
        // (Shift, Ctrl, ...) cannot be typed as a plain key, so offer
        // them in a list. Picking one assigns it with the same
        // duplicate-key check as a key capture.
        QComboBox *modifierCombo = new QComboBox;
        modifierCombo->setToolTip(QString::fromUtf8("choisir une touche de modification (Maj, Ctrl, Alt, Verr Maj, Méta)"));
        modifierCombo->addItem(QString::fromUtf8("Modif."), QString());
        QVector<QPair<QString, QString> > modifiers = KeymapModel::modifierKeys();
        for (int m = 0; m < modifiers.size(); ++m) {
            modifierCombo->addItem(modifiers.at(m).second, modifiers.at(m).first);
        }
        int currentModifier = 0;
        for (int m = 0; m < modifiers.size(); ++m) {
            if (modifiers.at(m).first == node.key) {
                currentModifier = m + 1;
                break;
            }
        }
        modifierCombo->setCurrentIndex(currentModifier);
        keyRow->addWidget(modifierCombo);
        connect(modifierCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), this,
                [this, index, modifierCombo](int comboIndex) {
                    if (!m_model || index < 0 || index >= m_model->nodes().size()) {
                        return;
                    }
                    QString name = modifierCombo->itemData(comboIndex).toString();
                    if (name.isEmpty()) {
                        return; // placeholder entry
                    }
                    if (name == m_model->nodes().at(index).key) {
                        return; // already assigned
                    }
                    if (keyInUse(name, index)) {
                        // Duplicate: snap the list back to the current key.
                        QSignalBlocker blocker(modifierCombo);
                        int current = 0;
                        QVector<QPair<QString, QString> > modifiers = KeymapModel::modifierKeys();
                        for (int m = 0; m < modifiers.size(); ++m) {
                            if (modifiers.at(m).first == m_model->nodes().at(index).key) {
                                current = m + 1;
                                break;
                            }
                        }
                        modifierCombo->setCurrentIndex(current);
                        return;
                    }
                    m_model->pushUndo();
                    m_model->nodes()[index].key = name;
                    m_model->modelChanged();
                });
    }

    // X / Y (first tap point for a multi node; none for android-key nodes).
    if (node.type != "KMT_ANDROID_KEY") {
        QPointF pos = node.pos;
        if (node.type == "KMT_CLICK_MULTI" && !node.clickNodes.isEmpty()) {
            pos = node.clickNodes.at(0).second;
        }
        QDoubleSpinBox *xSpin = new QDoubleSpinBox;
        xSpin->setRange(0, 1);
        xSpin->setDecimals(4);
        xSpin->setSingleStep(0.001);
        xSpin->setValue(pos.x());
        QDoubleSpinBox *ySpin = new QDoubleSpinBox;
        ySpin->setRange(0, 1);
        ySpin->setDecimals(4);
        ySpin->setSingleStep(0.001);
        ySpin->setValue(pos.y());
        form->addRow("X", xSpin);
        form->addRow("Y", ySpin);
        connect(xSpin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this,
                [this, index](double value) {
                    if (!m_model || index < 0 || index >= m_model->nodes().size()) {
                        return;
                    }
                    m_model->pushUndo();
                    KeymapNode &edited = m_model->nodes()[index];
                    if (edited.type == "KMT_CLICK_MULTI" && !edited.clickNodes.isEmpty()) {
                        edited.clickNodes[0].second.setX(value);
                    } else {
                        edited.pos.setX(value);
                    }
                    m_model->modelChanged();
                });
        connect(ySpin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this,
                [this, index](double value) {
                    if (!m_model || index < 0 || index >= m_model->nodes().size()) {
                        return;
                    }
                    m_model->pushUndo();
                    KeymapNode &edited = m_model->nodes()[index];
                    if (edited.type == "KMT_CLICK_MULTI" && !edited.clickNodes.isEmpty()) {
                        edited.clickNodes[0].second.setY(value);
                    } else {
                        edited.pos.setY(value);
                    }
                    m_model->modelChanged();
                });
    }

    // Size.
    QSpinBox *sizeSpin = new QSpinBox;
    sizeSpin->setRange(20, 120);
    sizeSpin->setValue(node.size);
    form->addRow(QString::fromUtf8("Taille"), sizeSpin);
    connect(sizeSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
            [this, index](int value) {
                if (!m_model || index < 0 || index >= m_model->nodes().size()) {
                    return;
                }
                m_model->pushUndo();
                m_model->nodes()[index].size = value;
                m_model->modelChanged();
            });

    // switchMap (only meaningful for KMT_CLICK).
    QCheckBox *switchMapCheck = new QCheckBox("switchMap");
    switchMapCheck->setChecked(node.switchMap);
    switchMapCheck->setEnabled(node.type == "KMT_CLICK");
    m_propertiesLayout->addWidget(switchMapCheck);
    connect(switchMapCheck, &QCheckBox::toggled, this, [this, index](bool checked) {
        if (!m_model || index < 0 || index >= m_model->nodes().size()) {
            return;
        }
        m_model->pushUndo();
        m_model->nodes()[index].switchMap = checked;
        m_model->modelChanged();
    });

    // Comment.
    QLineEdit *commentEdit = new QLineEdit(node.comment);
    form->addRow(QString::fromUtf8("Commentaire"), commentEdit);
    connect(commentEdit, &QLineEdit::editingFinished, this, [this, index, commentEdit]() {
        if (!m_model || index < 0 || index >= m_model->nodes().size()) {
            return;
        }
        if (commentEdit->text() == m_model->nodes().at(index).comment) {
            return;
        }
        m_model->pushUndo();
        m_model->nodes()[index].comment = commentEdit->text();
        m_model->modelChanged();
    });

    // Multi tap points (read-only display; editing delays is out of scope).
    if (node.type == "KMT_CLICK_MULTI") {
        QListWidget *tapList = new QListWidget;
        for (int t = 0; t < node.clickNodes.size(); ++t) {
            QString item = QString::fromUtf8("%1 ms — %2, %3")
                               .arg(node.clickNodes.at(t).first)
                               .arg(node.clickNodes.at(t).second.x(), 0, 'f', 3)
                               .arg(node.clickNodes.at(t).second.y(), 0, 'f', 3);
            tapList->addItem(item);
        }
        m_propertiesLayout->addWidget(new QLabel(QString::fromUtf8("Points")));
        m_propertiesLayout->addWidget(tapList);
    }
}

void KeymapSidebar::startNodeKeyCapture(int index)
{
    if (!m_model || index < 0 || index >= m_model->nodes().size()) {
        return;
    }
    m_capturingNodeKey = true;
    m_captureNodeIndex = index;
    if (m_nodeKeyCaptureButton) {
        m_nodeKeyCaptureButton->setText("…");
    }
    setFocus();
}

void KeymapSidebar::cancelKeyCaptures()
{
    m_capturingSwitchKey = false;
    m_capturingNodeKey = false;
    m_capturingSmallEyesKey = false;
    m_captureNodeIndex = -1;
    refreshAll();
}

void KeymapSidebar::onSwitchKeyButtonClicked()
{
    if (!m_model) {
        return;
    }
    m_capturingSwitchKey = true;
    if (m_switchKeyButton) {
        m_switchKeyButton->setText("…");
    }
    setFocus();
}

void KeymapSidebar::onSmallEyesKeyButtonClicked()
{
    if (!m_model || !m_model->mouseMoveMap().present) {
        return;
    }
    m_capturingSmallEyesKey = true;
    if (m_smallEyesKeyButton) {
        m_smallEyesKeyButton->setText("…");
    }
    setFocus();
}

bool KeymapSidebar::keyInUse(const QString &key, int excludeNode) const
{
    if (!m_model || key.isEmpty()) {
        return false;
    }
    if (m_model->switchKey() == key) {
        return true;
    }
    for (int i = 0; i < m_model->nodes().size(); ++i) {
        if (i == excludeNode) {
            continue;
        }
        const KeymapNode &node = m_model->nodes().at(i);
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

void KeymapSidebar::keyPressEvent(QKeyEvent *event)
{
    if (!event) {
        return;
    }
    if (m_capturingSwitchKey) {
        if (event->key() == Qt::Key_Escape) {
            cancelKeyCaptures();
            return;
        }
        QString name = KeymapModel::keyNameFromKeyEvent(event);
        if (name.isEmpty()) {
            return; // unknown key: stay in capture
        }
        if (keyInUse(name, -1)) {
            cancelKeyCaptures(); // duplicate: keep the old switchKey
            return;
        }
        m_capturingSwitchKey = false;
        if (m_model) {
            m_model->setSwitchKey(name); // emits modelChanged, refreshAll follows
        }
        return;
    }
    if (m_capturingNodeKey) {
        if (event->key() == Qt::Key_Escape) {
            cancelKeyCaptures();
            return;
        }
        QString name = KeymapModel::keyNameFromKeyEvent(event);
        if (name.isEmpty()) {
            return;
        }
        if (keyInUse(name, m_captureNodeIndex)) {
            cancelKeyCaptures(); // duplicate: keep the old key
            return;
        }
        if (!m_model || m_captureNodeIndex < 0 || m_captureNodeIndex >= m_model->nodes().size()) {
            cancelKeyCaptures();
            return;
        }
        m_capturingNodeKey = false;
        m_model->pushUndo();
        m_model->nodes()[m_captureNodeIndex].key = name;
        m_model->modelChanged();
        return;
    }
    if (m_capturingSmallEyesKey) {
        if (event->key() == Qt::Key_Escape) {
            cancelKeyCaptures();
            return;
        }
        QString name = KeymapModel::keyNameFromKeyEvent(event);
        if (name.isEmpty()) {
            return;
        }
        if (keyInUse(name, -1)) {
            cancelKeyCaptures(); // duplicate: keep the old key
            return;
        }
        if (!m_model || !m_model->mouseMoveMap().present) {
            cancelKeyCaptures();
            return;
        }
        m_capturingSmallEyesKey = false;
        MouseMoveMap map = m_model->mouseMoveMap();
        map.smallEyesKey = name;
        m_model->setMouseMoveMap(map); // emits modelChanged, refreshAll follows
        return;
    }
    event->ignore(); // let the parent form see the key (F9 lives there)
}
