#include "Layout2D.h"

#include <QHash>
#include <QLineF>
#include <QSet>
#include <QtMath>
#include <algorithm>

namespace Layout2D {
namespace {

constexpr double Pi = 3.14159265358979323846;

double normalizeAngle(double a) {
    while (a > Pi) a -= 2 * Pi;
    while (a <= -Pi) a += 2 * Pi;
    return a;
}

QVector<QVector<int>> adjacencyOf(int n, const QVector<Edge> &edges) {
    QVector<QVector<int>> adj(n);
    for (const Edge &e : edges) {
        if (e.a == e.b || e.a < 0 || e.b < 0 || e.a >= n || e.b >= n) continue;
        adj[e.a].append(e.b);
        adj[e.b].append(e.a);
    }
    return adj;
}

QVector<QVector<int>> smallestRings(int n, const QVector<Edge> &edges, const QVector<QVector<int>> &adj) {
    QVector<QVector<int>> candidates;
    QSet<QString> seenKeys;
    QHash<quint64, int> edgeIndex;
    const auto pairKey = [](int a, int b) { return (quint64(qMin(a, b)) << 32) | quint64(qMax(a, b)); };
    int edgeCount = 0;
    for (const Edge &e : edges) {
        if (e.a == e.b) continue;
        if (!edgeIndex.contains(pairKey(e.a, e.b))) edgeIndex.insert(pairKey(e.a, e.b), edgeCount++);
    }
    // Number of independent rings = E - V + components.
    QVector<int> comp(n, -1);
    int components = 0;
    for (int i = 0; i < n; ++i) {
        if (comp[i] >= 0) continue;
        QVector<int> queue{i};
        comp[i] = components;
        for (int h = 0; h < queue.size(); ++h)
            for (const int nb : adj[queue[h]])
                if (comp[nb] < 0) { comp[nb] = components; queue.append(nb); }
        ++components;
    }
    const int needed = edgeCount - n + components;
    if (needed <= 0) return {};

    for (auto it = edgeIndex.cbegin(); it != edgeIndex.cend(); ++it) {
        const int a = int(it.key() >> 32), b = int(it.key() & 0xffffffffu);
        QVector<int> parent(n, -2);
        QVector<int> queue{a};
        parent[a] = -1;
        bool found = false;
        for (int h = 0; h < queue.size() && !found; ++h) {
            const int cur = queue[h];
            for (const int next : adj[cur]) {
                if ((cur == a && next == b) || (cur == b && next == a)) continue;
                if (parent[next] != -2) continue;
                parent[next] = cur;
                if (next == b) { found = true; break; }
                queue.append(next);
            }
        }
        if (!found) continue;
        QVector<int> path;
        for (int cur = b; cur != -1; cur = parent[cur]) path.prepend(cur);
        QVector<int> sorted = path;
        std::sort(sorted.begin(), sorted.end());
        QString key;
        for (const int v : sorted) key += QString::number(v) + ',';
        if (seenKeys.contains(key)) continue;
        seenKeys.insert(key);
        candidates.append(path);
    }
    std::stable_sort(candidates.begin(), candidates.end(), [](const QVector<int> &x, const QVector<int> &y) {
        return x.size() < y.size();
    });

    // Keep rings that are independent over GF(2) in edge space.
    QVector<QVector<char>> basis;
    QVector<int> pivots;
    QVector<QVector<int>> rings;
    for (const QVector<int> &ring : candidates) {
        QVector<char> vec(edgeCount, 0);
        for (int i = 0; i < ring.size(); ++i)
            vec[edgeIndex.value(pairKey(ring[i], ring[(i + 1) % ring.size()]))] ^= 1;
        for (int b = 0; b < basis.size(); ++b)
            if (vec[pivots[b]]) for (int k = 0; k < edgeCount; ++k) vec[k] ^= basis[b][k];
        int pivot = -1;
        for (int k = 0; k < edgeCount; ++k) if (vec[k]) { pivot = k; break; }
        if (pivot < 0) continue;
        basis.append(vec);
        pivots.append(pivot);
        rings.append(ring);
        if (rings.size() == needed) break;
    }
    return rings;
}

class Placer {
public:
    Placer(int n, const QVector<Edge> &edges, double length)
        : m_n(n), m_L(length), m_adj(adjacencyOf(n, edges)) {
        m_pos.resize(n);
        m_placed.fill(0, n);
        m_queued.fill(0, n);
        m_parent.fill(-1, n);
        m_turn.fill(0, n);
        m_comp.fill(-1, n);
        m_sp.fill(0, n);
        QVector<int> doubles(n, 0);
        for (const Edge &e : edges) {
            if (e.a == e.b) continue;
            if (e.order == 3) { m_sp[e.a] = 1; m_sp[e.b] = 1; }
            if (e.order == 2) { ++doubles[e.a]; ++doubles[e.b]; }
        }
        for (int i = 0; i < n; ++i) if (doubles[i] >= 2) m_sp[i] = 1;
        m_rings = smallestRings(n, edges, m_adj);
        m_atomRings.resize(n);
        for (int r = 0; r < m_rings.size(); ++r)
            for (const int a : m_rings[r]) m_atomRings[a].append(r);
        // Ring systems: rings sharing an atom.
        m_ringSystem.fill(-1, m_rings.size());
        for (int r = 0; r < m_rings.size(); ++r) {
            if (m_ringSystem[r] >= 0) continue;
            const int id = m_systems.size();
            m_systems.append(QVector<int>());
            QVector<int> stack{r};
            m_ringSystem[r] = id;
            while (!stack.isEmpty()) {
                const int cur = stack.takeLast();
                m_systems[id].append(cur);
                for (const int atom : m_rings[cur])
                    for (const int other : m_atomRings[atom])
                        if (m_ringSystem[other] < 0) { m_ringSystem[other] = id; stack.append(other); }
            }
        }
        m_systemPlaced.fill(0, m_systems.size());
        m_ringDone.fill(0, m_rings.size());
        m_ringCenter.resize(m_rings.size());
        m_ringHasCenter.fill(0, m_rings.size());
    }

    QVector<QPointF> run() {
        // Components.
        int componentCount = 0;
        QVector<QVector<int>> members;
        for (int i = 0; i < m_n; ++i) {
            if (m_comp[i] >= 0) continue;
            members.append(QVector<int>{i});
            m_comp[i] = componentCount;
            for (int h = 0; h < members.last().size(); ++h)
                for (const int nb : m_adj[members.last()[h]])
                    if (m_comp[nb] < 0) { m_comp[nb] = componentCount; members.last().append(nb); }
            ++componentCount;
        }
        for (const QVector<int> &atoms : members) layoutComponent(atoms);
        relaxIfNeeded();

        // Side by side, vertically centred, left to right.
        QVector<QPointF> result = m_pos;
        double cursor = 0;
        for (int c = 0; c < members.size(); ++c) {
            double minX = 1e18, maxX = -1e18, minY = 1e18, maxY = -1e18;
            for (const int a : members[c]) {
                minX = qMin(minX, m_pos[a].x()); maxX = qMax(maxX, m_pos[a].x());
                minY = qMin(minY, m_pos[a].y()); maxY = qMax(maxY, m_pos[a].y());
            }
            const QPointF shift(cursor - minX, -(minY + maxY) / 2);
            for (const int a : members[c]) result[a] = m_pos[a] + shift;
            cursor += (maxX - minX) + 1.5 * m_L;
        }
        double minX = 1e18, maxX = -1e18, minY = 1e18, maxY = -1e18;
        for (const QPointF &p : result) {
            minX = qMin(minX, p.x()); maxX = qMax(maxX, p.x());
            minY = qMin(minY, p.y()); maxY = qMax(maxY, p.y());
        }
        const QPointF centre((minX + maxX) / 2, (minY + maxY) / 2);
        for (QPointF &p : result) p -= centre;
        return result;
    }

    QVector<QVector<int>> rings() const { return m_rings; }

private:
    void layoutComponent(const QVector<int> &atoms) {
        // Root: a terminal atom if there is one (chains read left to right),
        // otherwise the first atom (a pure ring system).
        // Not one hanging off a fusion atom: that substituent would be fixed
        // before the neighbouring rings and could end up on top of them.
        int root = atoms.first();
        bool found = false;
        for (const int a : atoms) {
            if (m_adj[a].size() != 1) continue;
            if (!found) { root = a; found = true; }
            if (m_atomRings[m_adj[a][0]].size() < 2) { root = a; break; }
        }
        m_queue.clear();
        if (m_atomRings[root].isEmpty()) {
            setPos(root, QPointF(0, 0), -1);
            enqueue(root);
        } else {
            int ring = m_atomRings[root].first();
            for (const int r : m_atomRings[root]) if (m_rings[r].size() < m_rings[ring].size()) ring = r;
            const double radius = ringRadius(m_rings[ring].size());
            const QPointF centre(0, radius);
            setPos(root, QPointF(0, 0), -1);
            placeRing(ring, root, centre);
        }
        for (int head = 0; head < m_queue.size(); ++head) extend(m_queue[head]);
    }

    double ringRadius(int sides) const { return m_L / (2.0 * qSin(Pi / sides)); }

    void setPos(int atom, const QPointF &p, int parent) {
        m_pos[atom] = p;
        m_placed[atom] = 1;
        m_parent[atom] = parent;
    }

    void enqueue(int atom) {
        if (m_queued[atom]) return;
        m_queued[atom] = 1;
        m_queue.append(atom);
    }

    // Puts the ring on a regular polygon around `centre`, with `anchor` (already
    // placed) fixing the rotation, then builds the rest of its ring system.
    void placeRing(int ringIndex, int anchor, const QPointF &centre) {
        const QVector<int> &ring = m_rings[ringIndex];
        const int sides = ring.size();
        const double radius = ringRadius(sides);
        const int anchorIdx = ring.indexOf(anchor);
        const double angle0 = qAtan2(m_pos[anchor].y() - centre.y(), m_pos[anchor].x() - centre.x());
        for (int k = 0; k < sides; ++k) {
            const int atom = ring[(anchorIdx + k) % sides];
            if (m_placed[atom]) continue;
            const double angle = angle0 + 2 * Pi * k / sides;
            setPos(atom, centre + QPointF(qCos(angle), qSin(angle)) * radius, -1);
        }
        m_ringCenter[ringIndex] = centre;
        m_ringHasCenter[ringIndex] = 1;
        m_ringDone[ringIndex] = 1;
        const int system = m_ringSystem[ringIndex];
        m_systemPlaced[system] = 1;
        for (const int atom : ring) enqueue(atom);
        buildSystem(system);
    }

    int overlapsAt(const QPointF &p, const QVector<int> &exclude) const {
        int count = 0;
        for (int a = 0; a < m_n; ++a) {
            if (!m_placed[a] || exclude.contains(a)) continue;
            if (QLineF(p, m_pos[a]).length() < 0.6 * m_L) ++count;
        }
        return count;
    }

    void buildSystem(int system) {
        const QVector<int> &rings = m_systems[system];
        for (;;) {
            bool progress = false;
            for (const int r : rings) {
                if (m_ringDone[r]) continue;
                const QVector<int> &ring = m_rings[r];
                const int m = ring.size();
                int pair = -1;
                for (int i = 0; i < m; ++i)
                    if (m_placed[ring[i]] && m_placed[ring[(i + 1) % m]]) { pair = i; break; }
                if (pair >= 0) {
                    fuse(r, pair);
                    progress = true;
                }
            }
            if (progress) continue;
            for (const int r : rings) {
                if (m_ringDone[r]) continue;
                for (const int atom : m_rings[r]) {
                    if (!m_placed[atom]) continue;
                    spiro(r, atom);
                    progress = true;
                    break;
                }
                if (progress) break;
            }
            if (!progress) break;
        }
    }

    void fuse(int r, int pair) {
        const QVector<int> &ring = m_rings[r];
        const int m = ring.size();
        const int uAtom = ring[pair], vAtom = ring[(pair + 1) % m];
        const QPointF u = m_pos[uAtom], v = m_pos[vAtom];
        const QPointF mid = (u + v) / 2;
        QPointF edge = v - u;
        const double len = qSqrt(edge.x() * edge.x() + edge.y() * edge.y());
        if (len > 1e-9) edge /= len;
        const QPointF normal(-edge.y(), edge.x());
        const double apothem = m_L / (2.0 * qTan(Pi / m));
        const QPointF c1 = mid + normal * apothem, c2 = mid - normal * apothem;

        // Reference point: centre of a placed ring that holds the shared bond,
        // else the centroid of everything placed in this system.
        QPointF ref;
        bool haveRef = false;
        for (const int other : m_atomRings[uAtom]) {
            if (other == r || !m_ringHasCenter[other] || !m_rings[other].contains(vAtom)) continue;
            ref = m_ringCenter[other];
            haveRef = true;
            break;
        }
        if (!haveRef) {
            QPointF sum;
            int count = 0;
            for (const int rr : m_systems[m_ringSystem[r]])
                for (const int a : m_rings[rr]) if (m_placed[a]) { sum += m_pos[a]; ++count; }
            ref = count ? sum / count : mid;
        }
        const auto dist = [](const QPointF &a, const QPointF &b) { return QLineF(a, b).length(); };
        const bool firstFarther = dist(c1, ref) >= dist(c2, ref);

        double bestScore = 1e18;
        QPointF best = c1;
        for (int side = 0; side < 2; ++side) {
            const QPointF c = side == 0 ? c1 : c2;
            const double radius = ringRadius(m);
            int overlaps = 0;
            const double angU = qAtan2(u.y() - c.y(), u.x() - c.x());
            const double angV = qAtan2(v.y() - c.y(), v.x() - c.x());
            const double sign = normalizeAngle(angV - angU) > 0 ? 1.0 : -1.0;
            for (int k = 2; k < m; ++k) {
                const int atom = ring[(pair + k) % m];
                if (m_placed[atom]) continue;
                const double ang = angU + sign * 2 * Pi * k / m;
                overlaps += overlapsAt(c + QPointF(qCos(ang), qSin(ang)) * radius, ring);
            }
            const bool farther = (side == 0) == firstFarther;
            const double score = overlaps * 100 + (farther ? 0 : 1);
            if (score < bestScore) { bestScore = score; best = c; }
        }
        const double radius = ringRadius(m);
        const double angU = qAtan2(u.y() - best.y(), u.x() - best.x());
        const double angV = qAtan2(v.y() - best.y(), v.x() - best.x());
        const double sign = normalizeAngle(angV - angU) > 0 ? 1.0 : -1.0;
        for (int k = 0; k < m; ++k) {
            const int atom = ring[(pair + k) % m];
            if (m_placed[atom]) continue;
            const double ang = angU + sign * 2 * Pi * k / m;
            setPos(atom, best + QPointF(qCos(ang), qSin(ang)) * radius, -1);
            enqueue(atom);
        }
        m_ringCenter[r] = best;
        m_ringHasCenter[r] = 1;
        m_ringDone[r] = 1;
    }

    void spiro(int r, int atom) {
        QPointF sum;
        int count = 0;
        for (const int other : m_atomRings[atom])
            if (m_ringHasCenter[other]) { sum += m_ringCenter[other]; ++count; }
        QPointF dir(1, 0);
        if (count) {
            dir = m_pos[atom] - sum / count;
        } else {
            QPointF away;
            for (const int nb : m_adj[atom]) if (m_placed[nb]) away -= m_pos[nb] - m_pos[atom];
            if (away.x() != 0 || away.y() != 0) dir = away;
        }
        const double len = qSqrt(dir.x() * dir.x() + dir.y() * dir.y());
        if (len > 1e-9) dir /= len;
        const int m = m_rings[r].size();
        const QPointF centre = m_pos[atom] + dir * ringRadius(m);
        placeRing(r, atom, centre);
    }

    // --- chains and substituents ---

    double repulsion(const QPointF &q, int owner) const {
        double score = 0;
        for (int a = 0; a < m_n; ++a) {
            if (!m_placed[a] || a == owner || m_comp[a] != m_comp[owner]) continue;
            const double d = QLineF(q, m_pos[a]).length();
            if (d < 3.0 * m_L) {
                const double r = m_L / qMax(d, 0.2 * m_L);
                score += r * r;
            }
        }
        return score;
    }

    int subtreeSize(int start, int blocked) const {
        QVector<char> seen(m_n, 0);
        seen[blocked] = 1;
        seen[start] = 1;
        QVector<int> queue{start};
        for (int h = 0; h < queue.size(); ++h)
            for (const int nb : m_adj[queue[h]])
                if (!seen[nb] && !m_placed[nb]) { seen[nb] = 1; queue.append(nb); }
        return queue.size();
    }

    void extend(int atom) {
        QVector<int> unplaced;
        QVector<double> existing;
        for (const int nb : m_adj[atom]) {
            if (m_placed[nb]) existing.append(qAtan2(m_pos[nb].y() - m_pos[atom].y(), m_pos[nb].x() - m_pos[atom].x()));
            else unplaced.append(nb);
        }
        const int k = unplaced.size();
        if (k == 0) return;
        const int p = existing.size();
        std::sort(existing.begin(), existing.end());

        QVector<QVector<double>> candidates;
        QVector<double> bias;
        const auto addSet = [&](const QVector<double> &set, double b = 0) { candidates.append(set); bias.append(b); };
        const double step120 = 2 * Pi / 3;

        if (p == 0) {
            if (k == 1) addSet({0});
            else if (m_sp[atom] && k == 2) addSet({0, Pi});
            else if (k == 2) addSet({-Pi / 3, Pi / 3});
            else {
                QVector<double> set;
                for (int i = 0; i < k; ++i) set.append(2 * Pi * i / k);
                addSet(set);
            }
        } else if (p == 1) {
            const double back = existing[0];
            if (k == 1) {
                if (m_sp[atom]) addSet({back + Pi});
                else {
                    const int parent = m_parent[atom];
                    const int previous = parent >= 0 ? m_turn[parent] : 0;
                    for (const int sign : {1, -1}) {
                        // sign > 0: the new bond turns clockwise (y down) relative to the incoming one.
                        const double angle = back + sign * step120;
                        const double turn = sign > 0 ? -1 : 1; // cross(incoming, outgoing) sign
                        addSet({angle}, (previous != 0 && int(turn) == previous) ? 0.6 : 0);
                    }
                }
            } else if (k == 2) {
                addSet({back + step120, back - step120});
            } else {
                QVector<double> set;
                for (int i = 1; i <= k; ++i) set.append(back + 2 * Pi * i / (k + 1));
                addSet(set);
            }
        } else {
            // Largest gap(s) between existing bonds.
            struct Gap { double start; double size; };
            QVector<Gap> gaps;
            for (int i = 0; i < p; ++i) {
                const double a = existing[i];
                const double b = i + 1 < p ? existing[i + 1] : existing[0] + 2 * Pi;
                gaps.append({a, b - a});
            }
            double biggest = 0;
            for (const Gap &g : gaps) biggest = qMax(biggest, g.size);
            for (const Gap &g : gaps) {
                if (g.size < biggest - 0.15) continue;
                QVector<double> set;
                for (int i = 1; i <= k; ++i) set.append(g.start + g.size * i / (k + 1));
                addSet(set);
            }
        }

        // Choose the candidate set with the least crowding.
        const auto point = [&](double angle) {
            return m_pos[atom] + QPointF(qCos(angle), qSin(angle)) * m_L;
        };
        int bestSet = 0;
        double bestScore = 1e18;
        for (int s = 0; s < candidates.size(); ++s) {
            double score = bias[s];
            for (const double angle : candidates[s]) score += repulsion(point(angle), atom);
            if (score < bestScore - 1e-9) { bestScore = score; bestSet = s; }
        }
        QVector<double> angles = candidates[bestSet];
        std::sort(angles.begin(), angles.end(), [&](double x, double y) {
            return repulsion(point(x), atom) < repulsion(point(y), atom);
        });
        std::stable_sort(unplaced.begin(), unplaced.end(), [&](int x, int y) {
            return subtreeSize(x, atom) > subtreeSize(y, atom);
        });

        if (p == 1 && k == 1 && !m_sp[atom]) {
            int previousAtom = atom;
            for (const int nb : m_adj[atom]) if (m_placed[nb]) { previousAtom = nb; break; }
            const QPointF in = m_pos[atom] - m_pos[previousAtom];
            const QPointF out(qCos(angles[0]), qSin(angles[0]));
            m_turn[atom] = (in.x() * out.y() - in.y() * out.x()) > 0 ? 1 : -1;
        }

        for (int i = 0; i < k; ++i) {
            const int nb = unplaced[i];
            if (m_placed[nb]) continue;
            const double angle = angles[i];
            const QPointF position = point(angle);
            const int system = m_atomRings[nb].isEmpty() ? -1 : m_ringSystem[m_atomRings[nb].first()];
            if (system >= 0 && !m_systemPlaced[system]) {
                int ring = m_atomRings[nb].first();
                for (const int r : m_atomRings[nb]) if (m_rings[r].size() < m_rings[ring].size()) ring = r;
                setPos(nb, position, atom);
                const QPointF centre = position + QPointF(qCos(angle), qSin(angle)) * ringRadius(m_rings[ring].size());
                placeRing(ring, nb, centre);
                m_parent[nb] = atom;
            } else {
                setPos(nb, position, atom);
                enqueue(nb);
            }
        }
    }

    // Spring relaxation, used only when the constructive layout left a bad bond
    // length or overlapping atoms (bridged rings, crowded fused systems).
    void relaxIfNeeded() {
        QVector<QPair<int, int>> bonds;
        QSet<quint64> bonded;
        for (int a = 0; a < m_n; ++a)
            for (const int b : m_adj[a])
                if (a < b) { bonds.append({a, b}); bonded.insert((quint64(a) << 32) | quint64(b)); }
        const auto bad = [&]() {
            for (const auto &b : bonds)
                if (qAbs(QLineF(m_pos[b.first], m_pos[b.second]).length() - m_L) > 0.04 * m_L) return true;
            for (int a = 0; a < m_n; ++a)
                for (int b = a + 1; b < m_n; ++b)
                    if (m_comp[a] == m_comp[b] && QLineF(m_pos[a], m_pos[b]).length() < 0.6 * m_L) return true;
            return false;
        };
        if (!bad()) return;
        for (int iteration = 0; iteration < 600; ++iteration) {
            QVector<QPointF> force(m_n);
            for (const auto &b : bonds) {
                QPointF d = m_pos[b.second] - m_pos[b.first];
                const double len = qMax(1e-6, qSqrt(d.x() * d.x() + d.y() * d.y()));
                const QPointF f = d / len * (len - m_L) * 0.5;
                force[b.first] += f;
                force[b.second] -= f;
            }
            for (int a = 0; a < m_n; ++a)
                for (int b = a + 1; b < m_n; ++b) {
                    if (m_comp[a] != m_comp[b]) continue;
                    QPointF d = m_pos[b] - m_pos[a];
                    double len = qSqrt(d.x() * d.x() + d.y() * d.y());
                    const double target = bonded.contains((quint64(a) << 32) | quint64(b)) ? 0 : 1.5 * m_L;
                    if (target == 0 || len >= target) continue;
                    if (len < 1e-6) { d = QPointF(0.01 * (a + 1), 0.01 * (b + 1)); len = 0.02; }
                    const QPointF f = d / len * (target - len) * 0.15;
                    force[a] -= f;
                    force[b] += f;
                }
            for (int a = 0; a < m_n; ++a) m_pos[a] += force[a];
        }
    }

    int m_n;
    double m_L;
    QVector<QVector<int>> m_adj;
    QVector<QPointF> m_pos;
    QVector<char> m_placed, m_queued, m_sp;
    QVector<int> m_parent, m_turn, m_comp;
    QVector<int> m_queue;
    QVector<QVector<int>> m_rings;
    QVector<QVector<int>> m_atomRings;
    QVector<int> m_ringSystem;
    QVector<QVector<int>> m_systems;
    QVector<char> m_systemPlaced, m_ringDone, m_ringHasCenter;
    QVector<QPointF> m_ringCenter;
};

} // namespace

QVector<QVector<int>> findRings(int atomCount, const QVector<Edge> &edges) {
    return smallestRings(atomCount, edges, adjacencyOf(atomCount, edges));
}

QVector<QPointF> place(int atomCount, const QVector<Edge> &edges, double bondLength) {
    if (atomCount <= 0) return {};
    return Placer(atomCount, edges, bondLength).run();
}

} // namespace Layout2D
