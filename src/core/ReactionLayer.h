#pragma once

#include <QObject>
#include <QVector>
#include <QVariantList>
#include <QVariantMap>
#include <QtNumeric>

class Molecule;

class ReactionLayer : public QObject {
    Q_OBJECT

public:
    explicit ReactionLayer(Molecule *molecule, QObject *parent = nullptr);

    Q_INVOKABLE QVariantList items() const;
    Q_INVOKABLE int addArrow(double startX, double startY, double endX, double endY);
    // Adds an arrow of the given kind (forward, equilibrium, retro, resonance, dashed,
    // noreaction, curved, fishhook). Curved kinds bend through control point (cx, cy);
    // pass NaN to use a default bulge.
    Q_INVOKABLE int addArrowKind(const QString &kind, double startX, double startY, double endX, double endY,
                                 double controlX = qQNaN(), double controlY = qQNaN());
    // Adds an arrow from a pen drag path (list of {x,y} maps or QPointF). The first and
    // last points are the ends; curved kinds take their bend from the path.
    Q_INVOKABLE int addArrowFromPath(const QString &kind, const QVariantList &path);
    // Quadratic control point for a pen path: the path point farthest from the
    // start-end chord, converted so the curve passes through it. Returns {x, y}.
    Q_INVOKABLE static QVariantMap controlFromPath(const QVariantList &path);
    Q_INVOKABLE static bool isCurvedKind(const QString &kind);
    Q_INVOKABLE static QString normalizedKind(const QString &kind);
    // Compound numbers ("1", "2a") are drawn bold.
    Q_INVOKABLE static bool isCompoundNumber(const QString &text);
    Q_INVOKABLE int addLabel(const QString &text, double x, double y);
    // Like addLabel, but a tap within `tolerance` of a straight arrow snaps the label
    // centred above (or below, following the tap) the arrow's midpoint.
    Q_INVOKABLE int addLabelSnapped(const QString &text, double x, double y, double tolerance = 40.0);
    Q_INVOKABLE QVariantMap snapLabelPosition(double x, double y, double tolerance = 40.0) const;
    // Mechanism notation. A double dagger is drawn as strokes (not a font glyph, which
    // the tablet font may lack); partial charges are ordinary labels ("δ+", "δ−").
    Q_INVOKABLE int addDagger(double x, double y);
    // Square brackets around the rectangle spanned by the two corners; withDagger adds a
    // double dagger at the top right (transition-state notation).
    Q_INVOKABLE int addBracket(double x1, double y1, double x2, double y2, bool withDagger = false);
    // p-orbital: two lobes through (x, y), oriented along (dirX, dirY); one lobe is shaded when asked.
    Q_INVOKABLE int addOrbital(double x, double y, double dirX, double dirY, bool shaded = false);
    static constexpr double OrbitalLobeLength = 30.0;
    // Moves both ends of an arrow (used by the tidy command). Straight arrows only.
    Q_INVOKABLE bool setArrowEnds(int itemId, double startX, double startY, double endX, double endY);
    // Reaction balance check for every straight reaction arrow; see SchemeAnalysis::checkReactions.
    Q_INVOKABLE QVariantList checkBalance() const;
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
        QString kind = QStringLiteral("forward");
        double cx = 0.0;
        double cy = 0.0;
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
