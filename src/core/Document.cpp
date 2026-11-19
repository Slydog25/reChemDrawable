#include "Document.h"

#include "Molecule.h"
#include "MolfileWriter.h"
#include "ReactionLayer.h"
#include "SvgWriter.h"
#include "ThumbnailWriter.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QStandardPaths>

Document::Document(Molecule *molecule, ReactionLayer *reactions, const QString &directory, QObject *parent)
    : QObject(parent), m_molecule(molecule), m_reactions(reactions),
      m_directory(directory.isEmpty() ? defaultDirectory() : directory) {
    connect(m_molecule, &Molecule::changed, this, &Document::markDirty);
    connect(m_reactions, &ReactionLayer::changed, this, &Document::markDirty);
    m_autosaveTimer.setSingleShot(true);
    m_autosaveTimer.setInterval(DefaultAutosaveDelayMs);
    connect(&m_autosaveTimer, &QTimer::timeout, this, &Document::runAutosave);
}

bool Document::autosavePending() const { return m_autosaveTimer.isActive(); }
int Document::autosaveDelayMs() const { return m_autosaveTimer.interval(); }
void Document::setAutosaveDelayMs(int milliseconds) {
    m_autosaveTimer.setInterval(qMax(0, milliseconds));
    emit autosavePendingChanged();
}

QString Document::thumbnailPathFor(const QString &documentPath) {
    const QFileInfo info(documentPath);
    return info.absoluteDir().filePath(QStringLiteral(".thumbs/") + info.completeBaseName() + QStringLiteral(".png"));
}

void Document::writeThumbnail(const QString &documentPath) const {
    QImage image = ThumbnailWriter::render(*m_molecule, m_reactions->items());
    image.setText(QStringLiteral("formula"), m_molecule->formula());
    const QString path = thumbnailPathFor(documentPath);
    if (QDir().mkpath(QFileInfo(path).absolutePath())) image.save(path, "PNG");
}

void Document::scheduleAutosave() {
    if (!m_hasFile || m_loading) return;
    const bool wasPending = m_autosaveTimer.isActive();
    m_autosaveTimer.start(); // restarting is the debounce
    if (!wasPending) emit autosavePendingChanged();
}

void Document::cancelAutosave() {
    if (!m_autosaveTimer.isActive()) return;
    m_autosaveTimer.stop();
    emit autosavePendingChanged();
}

void Document::runAutosave() {
    emit autosavePendingChanged();
    if (!m_hasFile || !m_dirty || m_path.isEmpty()) return;
    if (!m_reactions->saveToFile(m_path)) {
        emit message(QStringLiteral("Autosave failed."));
        return;
    }
    writeThumbnail(m_path);
    setClean(m_path);
    emit autosaved();
    emit message(QStringLiteral("Saved."));
}

bool Document::flushAutosave() {
    if (!m_autosaveTimer.isActive()) return false;
    m_autosaveTimer.stop();
    runAutosave();
    return !m_dirty;
}

bool Document::deleteDocument(const QString &path) {
    const QFileInfo info(path);
    const QString suffix = info.suffix();
    if ((suffix != QStringLiteral("rcd") && suffix != QStringLiteral("mol")) ||
        info.absoluteDir() != QDir(m_directory) || !info.exists())
        return false;
    if (!QFile::remove(info.absoluteFilePath())) {
        emit message(QStringLiteral("Could not delete %1.").arg(info.fileName()));
        return false;
    }
    QFile::remove(thumbnailPathFor(path));
    if (!m_path.isEmpty() && QFileInfo(m_path).absoluteFilePath() == info.absoluteFilePath()) {
        // Keep what is on the canvas, now as an unsaved untitled drawing.
        cancelAutosave();
        m_path.clear();
        m_hasFile = false;
        m_dirty = true;
        emit stateChanged();
    }
    emit message(QStringLiteral("Deleted %1.").arg(info.fileName()));
    return true;
}

QString Document::defaultDirectory() {
    QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (documents.isEmpty()) documents = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(documents).filePath(QStringLiteral("reChemDrawable"));
}

QString Document::directory() const { return m_directory; }
QString Document::currentPath() const { return m_path; }
bool Document::isDirty() const { return m_dirty; }

QString Document::title() const {
    return m_path.isEmpty() ? QStringLiteral("Untitled") : QFileInfo(m_path).completeBaseName();
}

QVariantList Document::documents() const {
    QVariantList result;
    const QFileInfoList files = QDir(m_directory).entryInfoList({QStringLiteral("*.rcd"), QStringLiteral("*.mol")},
                                                                 QDir::Files, QDir::Time);
    for (const QFileInfo &file : files) {
        const QString thumbnail = thumbnailPathFor(file.absoluteFilePath());
        const bool hasThumbnail = QFileInfo::exists(thumbnail);
        QString formula;
        if (hasThumbnail) formula = QImageReader(thumbnail).text(QStringLiteral("formula"));
        result.append(QVariantMap{
            {QStringLiteral("thumbnail"), hasThumbnail ? thumbnail : QString()},
            {QStringLiteral("formula"), formula},
            {QStringLiteral("name"), file.completeBaseName()},
            {QStringLiteral("path"), file.absoluteFilePath()},
            {QStringLiteral("kind"), file.suffix() == QStringLiteral("mol") ? QStringLiteral("molfile") : QStringLiteral("scheme")},
            {QStringLiteral("modified"), file.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm"))}});
    }
    return result;
}

bool Document::save() {
    if (!QDir().mkpath(m_directory)) {
        emit message(QStringLiteral("Could not create %1.").arg(m_directory));
        return false;
    }
    const QString path = m_path.isEmpty()
        ? uniquePath(QStringLiteral("drawing-") + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmm")), QStringLiteral("rcd"))
        : m_path;
    if (!m_reactions->saveToFile(path)) {
        emit message(QStringLiteral("Could not save %1.").arg(path));
        return false;
    }
    writeThumbnail(path);
    m_hasFile = true;
    setClean(path);
    emit message(QStringLiteral("Saved %1.").arg(QFileInfo(path).fileName()));
    return true;
}

bool Document::open(const QString &path) {
    const QFileInfo info(path);
    bool loaded = false;
    QString newPath;
    m_loading = true;
    if (info.suffix() == QStringLiteral("rcd")) {
        loaded = m_reactions->loadFromFile(path);
        newPath = path;
    } else if (info.suffix() == QStringLiteral("mol")) {
        loaded = m_molecule->loadFromFile(path);
        if (loaded) {
            // A .mol has no annotations; drop the old ones. Saving creates a
            // separate .rcd so the original Molfile is never overwritten.
            m_reactions->reset();
            newPath = uniquePath(info.completeBaseName(), QStringLiteral("rcd"));
        }
    } else {
        emit message(QStringLiteral("Only .rcd and .mol files can be opened."));
    }
    m_loading = false;
    if (!loaded) return false;
    setClean(newPath);
    m_hasFile = info.suffix() == QStringLiteral("rcd");
    // A Molfile's .rcd does not exist yet, so the drawing still needs saving.
    if (!QFileInfo::exists(newPath)) markDirty();
    emit message(QStringLiteral("Opened %1.").arg(info.fileName()));
    return true;
}

void Document::newDocument() {
    m_loading = true;
    m_molecule->replaceContent({}, {});
    m_reactions->reset();
    m_loading = false;
    m_hasFile = false;
    setClean(QString());
}

QString Document::exportMolfile() {
    const QString path = exportPath(QStringLiteral("mol"));
    return writeExport(path, MolfileWriter::writeV2000(*m_molecule, title()).toUtf8()) ? path : QString();
}

QString Document::exportSvg() {
    const QString path = exportPath(QStringLiteral("svg"));
    return writeExport(path, SvgWriter::write(*m_molecule, m_reactions->items()).toUtf8()) ? path : QString();
}

QString Document::exportSmiles() {
    const QString smiles = m_molecule->smiles();
    if (smiles.isEmpty()) {
        emit message(QStringLiteral("Draw a structure before exporting SMILES."));
        return QString();
    }
    const QString path = exportPath(QStringLiteral("smi"));
    if (!writeExport(path, (smiles + QLatin1Char(' ') + title() + QLatin1Char('\n')).toUtf8())) return QString();
    emit message(QStringLiteral("SMILES: %1").arg(smiles));
    return path;
}

QString Document::saveRecoveryCopy() {
    if (!m_dirty || !QDir().mkpath(m_directory)) return QString();
    const QString path = uniquePath(QStringLiteral("recovered-") + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")),
                                    QStringLiteral("rcd"));
    if (!m_reactions->saveToFile(path)) return QString();
    writeThumbnail(path);
    return path;
}

void Document::discardChanges() {
    cancelAutosave();
    if (!m_dirty) return;
    m_dirty = false;
    emit stateChanged();
}

void Document::markDirty() {
    if (m_loading) return;
    scheduleAutosave();
    if (m_dirty) return;
    m_dirty = true;
    emit stateChanged();
}

void Document::setClean(const QString &path) {
    m_path = path;
    m_dirty = false;
    cancelAutosave();
    emit stateChanged();
}

QString Document::uniquePath(const QString &baseName, const QString &extension) const {
    const QDir dir(m_directory);
    QString candidate = dir.filePath(baseName + QLatin1Char('.') + extension);
    for (int suffix = 2; QFileInfo::exists(candidate); ++suffix)
        candidate = dir.filePath(QStringLiteral("%1-%2.%3").arg(baseName).arg(suffix).arg(extension));
    return candidate;
}

QString Document::exportPath(const QString &extension) const {
    const QString base = m_path.isEmpty() ? QStringLiteral("untitled") : QFileInfo(m_path).completeBaseName();
    return QDir(m_directory).filePath(QStringLiteral("exports/%1.%2").arg(base, extension));
}

bool Document::writeExport(const QString &path, const QByteArray &contents) {
    QFile file(path);
    if (!QDir().mkpath(QFileInfo(path).absolutePath()) || !file.open(QIODevice::WriteOnly)) {
        emit message(QStringLiteral("Could not write %1.").arg(path));
        return false;
    }
    file.write(contents);
    emit message(QStringLiteral("Exported %1.").arg(QDir(m_directory).relativeFilePath(path)));
    return true;
}
