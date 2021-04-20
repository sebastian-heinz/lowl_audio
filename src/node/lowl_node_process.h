#ifndef LOWL_NODE_H
#define LOWL_NODE_H

#include "../lowl_audio_frame.h"

#include <vector>
#include <thread>

namespace Lowl {
    class NodeProcess {

        // TODO
        // need to find all input root nodes and call process on them / walk the tree
        // potentially only input nodes who extend node process will have own processing thread
        // other nodes will be joined by earliest thread walking tree up

    public:

        NodeProcess();

        ~NodeProcess() = default;
    };
}

#endif