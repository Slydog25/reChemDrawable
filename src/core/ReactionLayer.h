#pragma once

#include <QObject>
#include <QVector>
#include <QVariantList>

class Molecule;

class ReactionLayer : public QObject {
    Q_OBJECT

public:
    explicit ReactionLayer(Molecule *molecule, QObject *parent = nullptr);

    Q_INVOKABLE QVariantList items() const;
    Q_INVOKABLE int addArrow(double startX, double startY, double endX, double endY);
    Q_INVOKABLE int addLabel(const QString &text, double x, double y);
    Q_INVOKABLE int itemAt(double x, double y, double tolerance = 16.0) const;
    Q_INVOKABLE bool translateItem(int itemId, double dx, double dy);
    Q_INVOKABLE bool removeItem(int itemId);
    Q_INVOKABLE bool saveToFile(const QString &path);
    Q_INVOKABLE bool loadFromFile(const QString &path);
    Q_INVOKABLE bool undo();
    Q_INVOKABLE bool redo();
    Q_INVOKABLE void clear();
    // Removes every annotation and discards undo history (used when a document
    // without annotations is opened). Unlike clear(), this cannot be undone.
    void reset();

    static constexpr int MaxUndoSteps = 200;

signals:
    void changed();
    void message(const QString &text);
    // Emitted once for every new undoable edit (not for undo/redo themselves).
    void committed();
    // Emitted when the undo/redo stacks are discarded, e.g. after loading a scheme.
    void historyReset();

private:
    struct Item {
        int id = -1;
        QString type;
        double x1 = 0.0;
        double y1 = 0.0;
        double x2 = 0.0;
        double y2 = 0.0;
        QString text;
    };
    struct Snapshot {
        QVector<Item> items;
        int nextId = 1;
    };

    Snapshot snapshot() const;
    void commit(const Snapshot &before);

    Molecule *m_molecule;
    QVector<Item> m_items;
    QVector<Snapshot> m_undo;
    QVector<Snapshot> m_redo;
    int m_nextId = 1;
};
