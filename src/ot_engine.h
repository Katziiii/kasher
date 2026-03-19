#pragma once

#include <QString>
#include <QVector>

struct Op {
    enum Type { Insert, Delete, Noop };

    Type    type     = Noop;
    int     pos      = 0;
    QString text;           // Insert only
    int     len      = 0;  // Delete only
    int     revision = 0;
    QString author;

    bool isNoop() const {
        return type == Noop || (type == Delete && len <= 0);
    }
};

class OTEngine {
public:
    static Op  transform(Op a, const Op& b);
    Op         transformAgainstHistory(Op op, int fromRev) const;
    void       commitOp(const Op& op);
    int        revision() const { return m_baseRevision + (int)m_history.size(); }
    void       setBaseRevision(int rev);
    void       reset();

private:
    int         m_baseRevision = 0;
    QVector<Op> m_history;
};
