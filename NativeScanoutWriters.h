/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>

// Tracks the renderer's live commands for direct-scanout surfaces. One
// command's kernel fence covers earlier writes in the same Turnip FIFO.
// A writer that has not reached that FIFO blocks the exact handoff.
class NativeScanoutWriters {
    struct Writer {
        uint64_t runtime=0, command=0;
        uint32_t surface=0, fence=0;
        bool ambiguous=false;
    };
    Writer rows_[64]{};
    bool exhausted_=false;
public:
    void begin(uint64_t runtime,uint64_t command,uint32_t surface,bool ambiguous)
    {
        for(auto &row:rows_)if(!row.runtime) {
            row={runtime,command,surface,0,ambiguous};return;
        }
        exhausted_=true;
    }
    void issued(uint64_t runtime,uint32_t fence)
    {
        for(auto &row:rows_)if(row.runtime==runtime) {
            if(row.fence && row.fence!=fence)row.ambiguous=true;
            row.fence=fence;
            return;
        }
    }
    uint32_t fence(uint64_t runtime,uint64_t command,uint32_t surface) const
    {
        if(exhausted_ || !surface)return 0;
        const Writer *match=nullptr;
        for(const auto &row:rows_)if(row.runtime==runtime && row.command==command) {match=&row;break;}
        if(!match || !match->fence || match->ambiguous || match->surface!=surface)return 0;
        for(const auto &row:rows_)if(row.runtime && &row!=match) {
            if(row.ambiguous || (row.surface==surface && (!row.fence ||
                row.command>command || int32_t(match->fence-row.fence)<0)))return 0;
        }
        return match->fence;
    }
    void end(uint64_t runtime,uint64_t command)
    {
        for(auto &row:rows_)if(row.runtime==runtime && row.command==command) {row={};return;}
    }
};
