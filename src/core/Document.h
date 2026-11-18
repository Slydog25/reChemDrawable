#pragma once

#include <QObject>
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

public:
    // An empty directory means Documents/reChemDrawable.
    Document(Molecule *molecule, ReactionLayer *reactions, const QString &directory = QString(),
             QObject *parent = nullptr);

    static QString defaultDirectory();

    QString directory() const;
    QString title() const;
    bool isDirty() const;

    Q_INVOKABLE QString currentPath() const;
    // Drawings in the folder, newest first: {name, path, kind ("scheme"/"molfile"), modified}.
    Q_INVOKABLE QVariantList documents() const;
    // Saves to the current file, or to a new timestamped one for a new drawing.
    Q_INVOKABLE bool save();
    Q_INVOKABLE bool open(const QString &path);
    Q_INVOKABLE void newDocument();
    // Each returns the written path, or an empty string on failure.
    Q_INVOKABLE QString exportMolfile();
    Q_INVOKABLE QString exportSvg();
    Q_INVOKABLE QString exportSmiles();

signals:
    void stateChanged();
    void message(const QString &text);

private:
    void markDirty();
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
};
