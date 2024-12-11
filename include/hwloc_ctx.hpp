#pragma once

#include <hwloc.h>

#include <stdexcept>

namespace rg
{
    class HwlocTopology
    {
    public:
        static hwloc_topology_t const& getInstance()
        {
            static hwloc_topology_t topology = initializeTopology();
            return topology;
        }

    private:
        static hwloc_topology_t initializeTopology()
        {
            hwloc_topology_t topology;
            if(hwloc_topology_init(&topology) != 0)
            {
                throw std::runtime_error("Failed to initialize hwloc topology");
            }
            if(hwloc_topology_load(topology) != 0)
            {
                hwloc_topology_destroy(topology);
                throw std::runtime_error("Failed to load hwloc topology");
            }
            return topology;
        }
    };
} // namespace rg
