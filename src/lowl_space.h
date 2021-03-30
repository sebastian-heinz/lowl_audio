#ifndef LOWL_SPACE_H
#define LOWL_SPACE_H

#include "lowl_space_id.h"

#include <string>

namespace Lowl {
    class Space {

    private:
        size_t p_memory_mb;
        std::string p_tmp_path;

    public:
        void play(SpaceId p_id);

        void stop(SpaceId p_id);

        SpaceId add_audio(const std::string &p_path);

        void load();

        Space();

        ~Space() = default;
    };
}

#endif