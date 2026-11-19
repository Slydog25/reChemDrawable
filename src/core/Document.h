#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantList>

class Molecule;
class ReactionLayer;

// The drawing as a file: where it is saved, whether it has unsaved changes, and
// the list of drawings in the app's folder. Documents are saved as .rcd (structure
// plus arrows and labels); .mol files can be opened too. Exports go to an
// "exports" subfolder so they never overwrite a document.
class Document : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY stateChanged)
    Q_PROPERTY(bool dirty READ isDirty NOTIFY stateChanged)
    Q_PROPERTY(QString directory READ directory CONSTANT)
    // True while a change is waiting to be autosaved; the UI can show a hint.
    Q_PROPERTY(bool autosavePending READ autosavePending NOTIFY autosavePendingChanged)
    Q_PROPERTY(int autosaveDelayMs READ autosaveDelayMs WRITE setAutosaveDelayMs NOTIFY autosavePendingChanged)

public:
    // An empty directory means Documents/reChemDrawable.
    Document(Molecule *molecule, ReactionLayer *reactions, const QString &directory = QString(),
             QObject *parent = nullptr);

    static QString defaultDirectory();

    QString directory() const;
    QString title() const;
    bool isDirty() const;
    bool autosavePending() const;
    int autosaveDelayMs() const;
    void setAutosaveDelayMs(int milliseconds);
    static constexpr int DefaultAutosaveDelayMs = 4000;
    static QString thumbnailPathFor(const QString &documentPath);

    Q_INVOKABLE QString currentPath() const;
    // Drawings in the folder, newest first: {name, path, kind ("scheme"/"molfile"), modified,
    // thumbnail (path of the PNG preview or ""), formula (from the preview, may be "")}.
    Q_INVOKABLE QVariantList documents() const;
    // Saves now if an autosave is pending (used before closing). Returns true if it saved.
    Q_INVOKABLE bool flushAutosave();
    // Deletes a drawing from the folder together with its thumbnail. Deleting the
    // current drawing keeps its content on the canvas as an unsaved, untitled drawing.
    Q_INVOKABLE bool deleteDocument(const QString &path);
    // Saves to the current file, or to a new timestamped one for a new drawing.
    Q_INVOKABLE bool save();
    Q_INVOKABLE bool open(const QString &path);
    Q_INVOKABLE void newDocument();
    // Each returns the written path, or an empty string on failure.
    Q_INVOKABLE QString exportMolfile();
    Q_INVOKABLE QString exportSvg();
    Q_INVOKABLE QString exportSmiles();
    // Saves unsaved work to a new "recovered-<time>.rcd" without changing the
    // current file; used when the app is closed from outside. Returns the path,
    // or an empty string when there is nothing unsaved.
    QString saveRecoveryCopy();
    // Forgets unsaved changes (the user chose Discard), so nothing is recovered.
    Q_INVOKABLE void discardChanges();

signals:
    void stateChanged();
    void autosavePendingChanged();
    void autosaved();
    void message(const QString &text);

private:
    void markDirty();
    void scheduleAutosave();
    void cancelAutosave();
    void runAutosave();
    void writeThumbnail(const QString &documentPath) const;
    void setClean(const QString &path);
    QString uniquePath(const QString &baseName, const QString &extension) const;
    QString exportPath(const QString &extension) const;
    bool writeExport(const QString &path, const QByteArray &contents);

    Molecule *m_molecule;
    ReactionLayer *m_reactions;
    QString m_directory;
    QString m_path;
    bool m_dirty = false;
    bool m_loading = false;
    // True once the drawing is backed by a real .rcd (saved or opened), so autosave may write it.
    bool m_hasFile = false;
    QTimer m_autosaveTimer;
};
