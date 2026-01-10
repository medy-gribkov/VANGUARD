#ifndef MOCK_BRUCE_IR_H
#define MOCK_BRUCE_IR_H

namespace Vanguard {

class BruceIR {
public:
    static BruceIR& getInstance() { static BruceIR i; return i; }
    void init() {}
};

}

#endif
