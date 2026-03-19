#include "ot_engine.h"

Op OTEngine::transform(Op a, const Op& b) {
    if (a.isNoop() || b.isNoop())
        return a;

    if (a.type == Op::Insert) {
        if (b.type == Op::Insert) {
            // tie-break concurrent same-position inserts by author id
            if (b.pos < a.pos || (b.pos == a.pos && b.author < a.author))
                a.pos += b.text.length();
        } else {
            // a=Insert, b=Delete
            if (b.pos < a.pos) {
                int bEnd = b.pos + b.len;
                a.pos    = (bEnd <= a.pos) ? (a.pos - b.len) : b.pos;
            }
        }
    } else {
        // a=Delete
        if (b.type == Op::Insert) {
            if (b.pos <= a.pos)
                a.pos += b.text.length();
            else if (b.pos < a.pos + a.len)
                a.len += b.text.length();
        } else {
            // a=Delete, b=Delete
            int aEnd = a.pos + a.len;
            int bEnd = b.pos + b.len;

            if (bEnd <= a.pos) {
                a.pos -= b.len;
            } else if (b.pos < aEnd) {
                if (b.pos >= a.pos && bEnd <= aEnd) {
                    a.len -= b.len;
                } else if (b.pos < a.pos && bEnd >= aEnd) {
                    a.len = 0;
                    a.pos = b.pos;
                } else if (b.pos < a.pos) {
                    a.len = aEnd - bEnd;
                    a.pos = b.pos;
                } else {
                    a.len = b.pos - a.pos;
                }
            }
        }
    }
    return a;
}

Op OTEngine::transformAgainstHistory(Op op, int fromRev) const {
    int startIdx = fromRev - m_baseRevision;
    if (startIdx < 0) startIdx = 0;

    for (int i = startIdx; i < (int)m_history.size(); i++) {
        op = transform(op, m_history[i]);
        if (op.isNoop())
            return Op{};
    }
    op.revision = revision();
    return op;
}

void OTEngine::commitOp(const Op& op) {
    if (!op.isNoop())
        m_history.append(op);
}

void OTEngine::setBaseRevision(int rev) {
    m_baseRevision = rev;
    m_history.clear();
}

void OTEngine::reset() {
    m_baseRevision = 0;
    m_history.clear();
}
