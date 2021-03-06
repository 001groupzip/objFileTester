// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2007-2014 Barend Gehrels, Amsterdam, the Netherlands.
// Copyright (c) 2014 Adam Wulkiewicz, Lodz, Poland.

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_SELECT_RINGS_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_SELECT_RINGS_HPP


#include <map>

#include <boost/range.hpp>

#include <boost/geometry/core/tags.hpp>

#include <boost/geometry/algorithms/area.hpp>
#include <boost/geometry/algorithms/within.hpp>
#include <boost/geometry/algorithms/detail/interior_iterator.hpp>
#include <boost/geometry/algorithms/detail/ring_identifier.hpp>
#include <boost/geometry/algorithms/detail/overlay/ring_properties.hpp>
#include <boost/geometry/algorithms/detail/overlay/overlay_type.hpp>


namespace boost { namespace geometry
{


#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace overlay
{

struct ring_turn_info
{
    bool has_normal_turn;
    bool within_other;

    ring_turn_info()
        : has_normal_turn(false)
        , within_other(false)
    {}
};

namespace dispatch
{

    template <typename Tag, typename Geometry>
    struct select_rings
    {};

    template <typename Box>
    struct select_rings<box_tag, Box>
    {
        template <typename Geometry, typename RingPropertyMap>
        static inline void apply(Box const& box, Geometry const& ,
                ring_identifier const& id, RingPropertyMap& ring_properties)
        {
            ring_properties[id] = typename RingPropertyMap::mapped_type(box);
        }

        template <typename RingPropertyMap>
        static inline void apply(Box const& box,
                ring_identifier const& id, RingPropertyMap& ring_properties)
        {
            ring_properties[id] = typename RingPropertyMap::mapped_type(box);
        }
    };

    template <typename Ring>
    struct select_rings<ring_tag, Ring>
    {
        template <typename Geometry, typename RingPropertyMap>
        static inline void apply(Ring const& ring, Geometry const& ,
                    ring_identifier const& id, RingPropertyMap& ring_properties)
        {
            if (boost::size(ring) > 0)
            {
                ring_properties[id] = typename RingPropertyMap::mapped_type(ring);
            }
        }

        template <typename RingPropertyMap>
        static inline void apply(Ring const& ring,
                    ring_identifier const& id, RingPropertyMap& ring_properties)
        {
            if (boost::size(ring) > 0)
            {
                ring_properties[id] = typename RingPropertyMap::mapped_type(ring);
            }
        }
    };


    template <typename Polygon>
    struct select_rings<polygon_tag, Polygon>
    {
        template <typename Geometry, typename RingPropertyMap>
        static inline void apply(Polygon const& polygon, Geometry const& geometry,
                    ring_identifier id, RingPropertyMap& ring_properties)
        {
            typedef typename geometry::ring_type<Polygon>::type ring_type;
            typedef select_rings<ring_tag, ring_type> per_ring;

            per_ring::apply(exterior_ring(polygon), geometry, id, ring_properties);

            typename interior_return_type<Polygon const>::type
                rings = interior_rings(polygon);
            for (typename detail::interior_iterator<Polygon const>::type
                    it = boost::begin(rings); it != boost::end(rings); ++it)
            {
                id.ring_index++;
                per_ring::apply(*it, geometry, id, ring_properties);
            }
        }

        template <typename RingPropertyMap>
        static inline void apply(Polygon const& polygon,
                ring_identifier id, RingPropertyMap& ring_properties)
        {
            typedef typename geometry::ring_type<Polygon>::type ring_type;
            typedef select_rings<ring_tag, ring_type> per_ring;

            per_ring::apply(exterior_ring(polygon), id, ring_properties);

            typename interior_return_type<Polygon const>::type
                rings = interior_rings(polygon);
            for (typename detail::interior_iterator<Polygon const>::type
                    it = boost::begin(rings); it != boost::end(rings); ++it)
            {
                id.ring_index++;
                per_ring::apply(*it, id, ring_properties);
            }
        }
    };

    template <typename Multi>
    struct select_rings<multi_polygon_tag, Multi>
    {
        template <typename Geometry, typename RingPropertyMap>
        static inline void apply(Multi const& multi, Geometry const& geometry,
                    ring_identifier id, RingPropertyMap& ring_properties)
        {
            typedef typename boost::range_iterator
                <
                    Multi const
                >::type iterator_type;

            typedef select_rings<polygon_tag, typename boost::range_value<Multi>::type> per_polygon;

            id.multi_index = 0;
            for (iterator_type it = boost::begin(multi); it != boost::end(multi); ++it)
            {
                id.ring_index = -1;
                per_polygon::apply(*it, geometry, id, ring_properties);
                id.multi_index++;
            }
        }
    };

} // namespace dispatch


template<overlay_type OverlayType>
struct decide
{};

template<>
struct decide<overlay_union>
{
    static bool include(ring_identifier const& , ring_turn_info const& info)
    {
        return ! info.within_other;
    }

    static bool reversed(ring_identifier const& , ring_turn_info const& )
    {
        return false;
    }
};

template<>
struct decide<overlay_difference>
{
    static bool include(ring_identifier const& id, ring_turn_info const& info)
    {
        // Difference: A - B

        // If this is A (source_index=0), then the ring is inside B
        // If this is B (source_index=1), then the ring is NOT inside A

        // If this is A and the ring is within the other geometry,
        // then we should NOT include it.
        // If this is B then we SHOULD include it.

        return id.source_index == 0
            ? ! info.within_other
            : info.within_other;
    }

    static bool reversed(ring_identifier const& id, ring_turn_info const& info)
    {
        // Difference: A - B
        // If this is B, and the ring is included, it should be reversed afterwards

        return id.source_index == 1 && include(id, info);
    }
};

template<>
struct decide<overlay_intersection>
{
    static bool include(ring_identifier const& , ring_turn_info const& info)
    {
        return info.within_other;
    }

    static bool reversed(ring_identifier const& , ring_turn_info const& )
    {
        return false;
    }
};


template
<
    overlay_type OverlayType,
    typename Geometry1,
    typename Geometry2,
    typename TurnInfoMap,
    typename RingPropertyMap
>
inline void update_ring_selection(Geometry1 const& geometry1,
            Geometry2 const& geometry2,
            TurnInfoMap const& turn_info_map,
            RingPropertyMap const& all_ring_properties,
            RingPropertyMap& selected_ring_properties)
{
    selected_ring_properties.clear();

    for (typename RingPropertyMap::const_iterator it = boost::begin(all_ring_properties);
        it != boost::end(all_ring_properties);
        ++it)
    {
        ring_identifier const& id = it->first;

        ring_turn_info info;

        typename TurnInfoMap::const_iterator tcit = turn_info_map.find(id);
        if (tcit != turn_info_map.end())
        {
            info = tcit->second; // Copy by value
        }

        if (info.has_normal_turn)
        {
            // There are normal turns on this ring. It should be traversed, we
            // don't include the original ring
            continue;
        }

        // Check if the ring is within the other geometry, by taking
        // a point lying on the ring
        switch(id.source_index)
        {
            case 0 :
                info.within_other = geometry::within(it->second.point, geometry2);
                break;
            case 1 :
                info.within_other = geometry::within(it->second.point, geometry1);
                break;
        }

        if (decide<OverlayType>::include(id, info))
        {
            typename RingPropertyMap::mapped_type properties = it->second; // Copy by value
            properties.reversed = decide<OverlayType>::reversed(id, info);
            selected_ring_properties[id] = properties;
        }
    }
}


/*!
\brief The function select_rings select rings based on the overlay-type (union,intersection)
*/
template
<
    overlay_type OverlayType,
    typename Geometry1,
    typename Geometry2,
    typename RingTurnInfoMap,
    typename RingPropertyMap
>
inline void select_rings(Geometry1 const& geometry1, Geometry2 const& geometry2,
            RingTurnInfoMap const& turn_info_per_ring,
            RingPropertyMap& selected_ring_properties)
{
    typedef typename geometry::tag<Geometry1>::type tag1;
    typedef typename geometry::tag<Geometry2>::type tag2;

    RingPropertyMap all_ring_properties;
    dispatch::select_rings<tag1, Geometry1>::apply(geometry1, geometry2,
                ring_identifier(0, -1, -1), all_ring_properties);
    dispatch::select_rings<tag2, Geometry2>::apply(geometry2, geometry1,
                ring_identifier(1, -1, -1), all_ring_properties);

    update_ring_selection<OverlayType>(geometry1, geometry2, turn_info_per_ring,
                all_ring_properties, selected_ring_properties);
}

template
<
    overlay_type OverlayType,
    typename Geometry,
    typename RingTurnInfoMap,
    typename RingPropertyMap
>
inline void select_rings(Geometry const& geometry,
            RingTurnInfoMap const& turn_info_per_ring,
            RingPropertyMap& selected_ring_properties)
{
    typedef typename geometry::tag<Geometry>::type tag;

    RingPropertyMap all_ring_properties;
    dispatch::select_rings<tag, Geometry>::apply(geometry,
                ring_identifier(0, -1, -1), all_ring_properties);

    update_ring_selection<OverlayType>(geometry, geometry, turn_info_per_ring,
                all_ring_properties, selected_ring_properties);
}


}} // namespace detail::overlay
#endif // DOXYGEN_NO_DETAIL


}} // namespace boost::geometry


#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_OVERLAY_SELECT_RINGS_HPP
