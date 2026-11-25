// Mirrorff — fork of QtScrcpy v4.2.1 (Apache-2.0)
#ifndef KEYMAPSIDEBAR_H
#define KEYMAPSIDEBAR_H

#include <QPointer>
#include <QWidget>

class QCheckBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QKeyEvent;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

class KeymapModel;
class KeymapOverlay;

// Control panel next to the video (fixed width 220 px). It never talks to
// the device or the filesystem itself: it reads the model / overlay and
// emits signals, and the parent form wires everything (save, import,
// export, audio, window actions).
class KeymapSidebar : public QWidget
{
    Q_OBJECT
public:
    explicit KeymapSidebar(QWidget *parent = 0);

    void setOverlay(KeymapOverlay *overlay);
    void setModel(KeymapModel *model);
    void setFileName(const QString &name);
    void setSwitchKey(const QString &key);
    void setEditMode(bool on);
    void setAudioRunning(bool running);
    void refreshAll();

signals:
    void editModeToggled(bool on);
    void saveRequested();
    void importRequested();
    void exportCopyRequested();
    void openKeymapFolderRequested();
    void addClickRequested();
    void addMultiRequested();
    void addWheelRequested();
    void deleteRequested();
    void duplicateRequested();
    void undoRequested();
    void redoRequested();
    void installSndcpyRequested();
    void startAudioRequested();
    void stopAudioRequested();
    void fullscreenRequested();
    void rotateRequested();
    void screenshotRequested();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onModelChanged();
    void onSelectionChanged(int index);
    void onSwitchKeyButtonClicked();
    void onSmallEyesKeyButtonClicked();

private:
    QGroupBox *makeSection(const QString &title, QWidget *content);
    void rebuildProperties();
    // Buttons, check boxes and combo boxes here are mouse-only controls:
    // they must never take keyboard focus, otherwise a focused button is
    // activated by Space (QAbstractButton) and the game's jump key never
    // reaches the device.
    void disableKeyboardFocus();
    void startNodeKeyCapture(int index);
    void cancelKeyCaptures();
    bool keyInUse(const QString &key, int excludeNode) const;

    KeymapModel *m_model;
    KeymapOverlay *m_overlay;
    bool m_editMode;
    bool m_audioRunning;

    // Key capture (click a capture button, then press a key; Escape cancels).
    bool m_capturingSwitchKey;
    bool m_capturingNodeKey;
    bool m_capturingSmallEyesKey;
    int m_captureNodeIndex;

    QScrollArea *m_scrollArea;
    QWidget *m_content;

    // Mode
    QPushButton *m_modeButton;
    QLabel *m_modeLabel;
    // Edit
    QPushButton *m_deleteButton;
    QPushButton *m_duplicateButton;
    QPushButton *m_undoButton;
    QPushButton *m_redoButton;
    // Keymap
    QPushButton *m_saveButton;
    QPushButton *m_importButton;
    QPushButton *m_exportButton;
    QPushButton *m_openFolderButton;
    QLabel *m_fileNameLabel;
    QPushButton *m_switchKeyButton;
    // Properties (rebuilt by refreshAll)
    QWidget *m_propertiesContent;
    QVBoxLayout *m_propertiesLayout;
    QPointer<QPushButton> m_nodeKeyCaptureButton;
    // Mouse-look
    QDoubleSpinBox *m_lookXSpin;
    QDoubleSpinBox *m_lookYSpin;
    QDoubleSpinBox *m_ratioXSpin;
    QDoubleSpinBox *m_ratioYSpin;
    QCheckBox *m_smallEyesCheck;
    QPushButton *m_smallEyesKeyButton;
    QDoubleSpinBox *m_smallEyesXSpin;
    QDoubleSpinBox *m_smallEyesYSpin;
    QCheckBox *m_showCrosshairCheck;
    // Audio
    QLabel *m_audioLabel;
};

#endif // KEYMAPSIDEBAR_H
