/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2013 CERN
 * @author Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */


#include <assert.h>                               // for assert
#include <cmath>
#include <limits.h>                               // for INT_MAX

#include <geometry/seg.h>                         // for SEG
#include <geometry/shape.h>
#include <geometry/shape_arc.h>
#include <geometry/shape_line_chain.h>
#include <geometry/shape_circle.h>
#include <geometry/shape_rect.h>
#include <geometry/shape_segment.h>
#include <geometry/shape_simple.h>
#include <geometry/shape_compound.h>
#include <math/vector2d.h>

typedef VECTOR2I::extended_type ecoord;

static inline bool Collide( const SHAPE_CIRCLE& aA, const SHAPE_CIRCLE& aB, int aClearance,
                            int* aActual, VECTOR2I* aLocation, VECTOR2I* aMTV )
{
    ecoord min_dist = aClearance + aA.GetRadius() + aB.GetRadius();
    ecoord min_dist_sq = min_dist * min_dist;

    const VECTOR2I delta = aB.GetCenter() - aA.GetCenter();
    ecoord dist_sq = delta.SquaredEuclideanNorm();

    if( dist_sq < min_dist_sq )
    {
        if( aActual )
            *aActual = std::max( 0, (int) sqrt( dist_sq ) - aA.GetRadius() - aB.GetRadius() );

        if( aLocation )
            *aLocation = ( aA.GetCenter() + aB.GetCenter() ) / 2;

        if( aMTV )
            *aMTV = delta.Resize( min_dist - sqrt( dist_sq ) + 3 );  // fixme: apparent rounding error

        return true;
    }

    return false;
}


static inline bool Collide( const SHAPE_RECT& aA, const SHAPE_CIRCLE& aB, int aClearance,
                            int* aActual, VECTOR2I* aLocation, VECTOR2I* aMTV )
{
    const VECTOR2I c = aB.GetCenter();
    const VECTOR2I p0 = aA.GetPosition();
    const VECTOR2I size = aA.GetSize();
    const int r = aB.GetRadius();
    const int min_dist = aClearance + r;
    const ecoord min_dist_sq = SEG::Square( min_dist );

    const VECTOR2I vts[] =
    {
        VECTOR2I( p0.x,          p0.y ),
        VECTOR2I( p0.x,          p0.y + size.y ),
        VECTOR2I( p0.x + size.x, p0.y + size.y ),
        VECTOR2I( p0.x + size.x, p0.y ),
        VECTOR2I( p0.x,          p0.y )
    };

    ecoord nearest_side_dist_sq = VECTOR2I::ECOORD_MAX;
    VECTOR2I nearest;

    bool inside = c.x >= p0.x && c.x <= ( p0.x + size.x )
                  && c.y >= p0.y && c.y <= ( p0.y + size.y );

    // If we're not looking for MTV or actual, short-circuit once we find a hard collision
    if( inside && !aActual && !aLocation && !aMTV )
        return true;

    for( int i = 0; i < 4; i++ )
    {
        const SEG side( vts[i], vts[ i + 1] );

        VECTOR2I pn = side.NearestPoint( c );
        ecoord side_dist_sq = ( pn - c ).SquaredEuclideanNorm();

        if( side_dist_sq < nearest_side_dist_sq )
        {
            nearest = pn;
            nearest_side_dist_sq = side_dist_sq;

            // If we're not looking for actual or MTV, short-circuit once we find any collision
            if( ( nearest_side_dist_sq == 0 || !aActual ) && !aMTV )
                break;
        }
    }

    if( inside || nearest_side_dist_sq < min_dist_sq )
    {
        if( aLocation )
            *aLocation = nearest;

        if( aActual )
            *aActual = std::max( 0, (int) sqrt( nearest_side_dist_sq ) - r );

        if( aMTV )
        {
            VECTOR2I delta = c - nearest;

            if( inside )
                *aMTV = -delta.Resize( abs( min_dist + 1 + sqrt( nearest_side_dist_sq ) ) + 1 );
            else
                *aMTV = delta.Resize( abs( min_dist + 1 - sqrt( nearest_side_dist_sq ) ) + 1 );
        }

        return true;
    }

    return false;
}


static VECTOR2I pushoutForce( const SHAPE_CIRCLE& aA, const SEG& aB, int aClearance )
{
    VECTOR2I f( 0, 0 );

    const VECTOR2I c = aA.GetCenter();
    const VECTOR2I nearest = aB.NearestPoint( c );

    const int r = aA.GetRadius();

    int dist = ( nearest - c ).EuclideanNorm();
    int min_dist = aClearance + r;

    if( dist < min_dist )
    {
        for( int corr = 0; corr < 5; corr++ )
        {
            f = ( aA.GetCenter() - nearest ).Resize( min_dist - dist + corr );

            if( aB.Distance( c + f ) >= min_dist )
                break;
        }
    }

    return f;
}


static inline bool Collide( const SHAPE_CIRCLE& aA, const SHAPE_LINE_CHAIN_BASE& aB,
                            int aClearance, int* aActual, VECTOR2I* aLocation, VECTOR2I* aMTV )
{
    int closest_dist = INT_MAX;
    VECTOR2I nearest;

    for( int s = 0; s < aB.GetSegmentCount(); s++ )
    {
        int collision_dist = 0;
        VECTOR2I pn;

        if( aA.Collide( aB.GetSegment( s ), aClearance,
                        aActual || aLocation ? &collision_dist : nullptr,
                        aLocation ? &pn : nullptr ) )
        {
            if( collision_dist < closest_dist )
            {
                nearest = pn;
                closest_dist = collision_dist;

                if( closest_dist == 0 || !aActual )
                    break;
            }
        }
    }

    if( closest_dist < aClearance )
    {
        if( aLocation )
            *aLocation = nearest;

        if( aActual )
            *aActual = closest_dist;

        if( aMTV )
        {
            SHAPE_CIRCLE cmoved( aA );
            VECTOR2I f_total( 0, 0 );

            for( int s = 0; s < aB.GetSegmentCount(); s++ )
            {
                VECTOR2I f = pushoutForce( cmoved, aB.GetSegment( s ), aClearance );
                cmoved.SetCenter( cmoved.GetCenter() + f );
                f_total += f;
            }

            *aMTV = f_total;
        }

        return true;
    }

    return false;
}


static inline bool Collide( const SHAPE_CIRCLE& aA, const SHAPE_SEGMENT& aSeg, int aClearance,
                            int* aActual, VECTOR2I* aLocation, VECTOR2I* aMTV )
{
    if( aA.Collide( aSeg.GetSeg(), aClearance + aSeg.GetWidth() / 2, aActual, aLocation ) )
    {
        if( aMTV )
            *aMTV = -pushoutForce( aA, aSeg.GetSeg(), aClearance + aSeg.GetWidth() / 2);

        return true;
    }

    return false;
}


static inline bool Collide( const SHAPE_LINE_CHAIN_BASE& aA, const SHAPE_LINE_CHAIN_BASE& aB,
                            int aClearance, int* aActual, VECTOR2I* aLocation, VECTOR2I* aMTV )
{
    // TODO: why doesn't this handle MTV?
    // TODO: worse, why this doesn't handle closed shapes?

    int closest_dist = INT_MAX;
    VECTOR2I nearest;

    for( int i = 0; i < aB.GetSegmentCount(); i++ )
    {
        int collision_dist = 0;
        VECTOR2I pn;

        if( aA.Collide( aB.GetSegment( i ), aClearance,
                        aActual || aLocation ? &collision_dist : nullptr,
                        aLocation ? &pn : nullptr ) )
        {
            if( collision_dist < closest_dist )
            {
                nearest = pn;
                closest_dist = collision_dist;

                if( closest_dist == 0 || !aActual )
                    break;
            }
        }
    }

    if( closest_dist < aClearance )
    {
        if( aLocation )
            *aLocation = nearest;

        if( aActual )
            *aActual = closest_dist;

        return true;
    }

    return false;
}


static inline bool Collide( const SHAPE_RECT& aA, const SHAPE_LINE_CHAIN_BASE& aB, int aClearance,
                            int* aActual, VECTOR2I* aLocation, VECTOR2I* aMTV )
{
    // TODO: why doesn't this handle MTV?

    if( aB.IsClosed() && aB.PointInside( aA.Centre() ) )
    {
        if( aLocation )
            *aLocation = aA.Centre();

        if( aActual )
            *aActual = 0;

        return true;
    }

    int closest_dist = INT_MAX;
    VECTOR2I nearest;

    for( int s = 0; s < aB.GetSegmentCount(); s++ )
    {
        int collision_dist = 0;
        VECTOR2I pn;

        if( aA.Collide( aB.GetSegment( s ), aClearance,
                        aActual || aLocation ? &collision_dist : nullptr,
                        aLocation ? &pn : nullptr ) )
        {
            if( collision_dist < closest_dist )
            {
                nearest = pn;
                closest_dist = collision_dist;

                if( closest_dist == 0 || !aActual )
                    break;
            }
        }
    }

    if( closest_dist < aClearance )
    {
        if( aLocation )
            *aLocation = nearest;

        if( aActual )
            *aActual = closest_dist;

        return true;
    }

    return false;
}


static inline bool Collide( const SHAPE_RECT& aA, const SHAPE_SEGMENT& aSeg, int aClearance,
                            int* aActual, VECTOR2I* aLocation, VECTOR2I* aMTV )
{
    if( aA.Collide( aSeg.GetSeg(), aClearance + aSeg.GetWidth() / 2, aActual, aLocation ) )
    {
        if( aActual )
            *aActual = std::max( 0, *aActual - aSeg.GetWidth() / 2 );

        // TODO: why doesn't this handle MTV?

        return true;
    }

    return false;
}


static inline bool Collide( const SHAPE_SEGMENT& aA, const SHAPE_SEGMENT& aB, int aClearance,
                            int* aActual, VECTOR2I* aLocation, VECTOR2I* aMTV )
{
    if( aA.Collide( aB.GetSeg(), aClearance + aB.GetWidth() / 2, aActual, aLocation ) )
    {
        if( aActual )
            *aActual = std::max( 0, *aActual - aB.GetWidth() / 2 );

        // TODO: why doesn't this handle MTV?

        return true;
    }

    return false;
}


static inline bool Collide( const SHAPE_LINE_CHAIN_BASE& aA, const SHAPE_SEGMENT& aB,
                            int aClearance, int* aActual, VECTOR2I* aLocation, VECTOR2I* aMTV )
{
    if( aA.Collide( aB.GetSeg(), aClearance + aB.GetWidth() / 2, aActual, aLocation ) )
    {
        if( aActual )
            *aActual = std::max( 0, *aActual - aB.GetWidth() / 2 );

        // TODO: why doesn't this handle MTV?

        return true;
    }

    return false;
}


static inline bool Collide( const SHAPE_RECT& aA, const SHAPE_RECT& aB, int aClearance,
                            int* aActual, VECTOR2I* aLocation, VECTOR2I* aMTV )
{
    return Collide( aA.Outline(), aB.Outline(), aClearance, aActual, aLocation, aMTV );
}

static inline bool Collide( const SHAPE_ARC& aA, const SHAPE_RECT& aB, int aClearance,
                            int* aActual, VECTOR2I* aLocation, VECTOR2I* aMTV )
{
    const auto lc = aA.ConvertToPolyline();
    return Collide( lc, aB.Outline(), aClearance, aActual, aLocation, aMTV );
}

static inline bool Collide( const SHAPE_ARC& aA, const SHAPE_CIRCLE& aB, int aClearance,
                            int* aActual, VECTOR2I* aLocation, VECTOR2I* aMTV )
{
    const auto lc = aA.ConvertToPolyline();
    bool rv = Collide( aB, lc, aClearance, aActual, aLocation, aMTV );

    if( rv && aMTV )
        *aMTV = - *aMTV ;

    return rv;
}

static inline bool Collide( const SHAPE_ARC& aA, const SHAPE_LINE_CHAIN& aB, int aClearance,
                            int* aActual, VECTOR2I* aLocation, VECTOR2I* aMTV )
{
    const auto lc = aA.ConvertToPolyline();
    return Collide( lc, aB, aClearance, aActual, aLocation, aMTV );
}

static inline bool Collide( const SHAPE_ARC& aA, const SHAPE_SEGMENT& aB, int aClearance,
                            int* aActual, VECTOR2I* aLocation, VECTOR2I* aMTV )
{
    const auto lc = aA.ConvertToPolyline();
    return Collide( lc, aB, aClearance, aActual, aLocation, aMTV );
}

static inline bool Collide( const SHAPE_ARC& aA, const SHAPE_LINE_CHAIN_BASE& aB, int aClearance,
                            int* aActual, VECTOR2I* aLocation, VECTOR2I* aMTV )
{
    const auto lc = aA.ConvertToPolyline();

    return Collide( lc, aB, aClearance, aActual, aLocation, aMTV );
}

static inline bool Collide( const SHAPE_ARC& aA, const SHAPE_ARC& aB, int aClearance,
                            int* aActual, VECTOR2I* aLocation, VECTOR2I* aMTV )
{
    const auto lcA = aA.ConvertToPolyline();
    const auto lcB = aB.ConvertToPolyline();
    return Collide( lcA, lcB, aClearance, aActual, aLocation, aMTV );
}

template<class T_a, class T_b>
inline bool CollCase( const SHAPE* aA, const SHAPE* aB, int aClearance, int* aActual,
                      VECTOR2I* aLocation, VECTOR2I* aMTV )

{
    return Collide( *static_cast<const T_a*>( aA ), *static_cast<const T_b*>( aB ),
                    aClearance, aActual, aLocation, aMTV);
}

template<class T_a, class T_b>
inline bool CollCaseReversed ( const SHAPE* aA, const SHAPE* aB, int aClearance, int* aActual,
                               VECTOR2I* aLocation, VECTOR2I* aMTV )
{
    bool rv = Collide( *static_cast<const T_b*>( aB ), *static_cast<const T_a*>( aA ),
                       aClearance, aActual, aLocation, aMTV);

    if( rv && aMTV)
        *aMTV = -  *aMTV;

    return rv;
}


static bool collideSingleShapes( const SHAPE* aA, const SHAPE* aB, int aClearance, int* aActual,
                                 VECTOR2I* aLocation, VECTOR2I* aMTV )
{
    switch( aA->Type() )
    {
    case SH_NULL: 
        return false;

    case SH_RECT:
        switch( aB->Type() )
        {
        case SH_RECT:
            return CollCase<SHAPE_RECT, SHAPE_RECT>( aA, aB, aClearance, aActual, aLocation, aMTV );

        case SH_CIRCLE:
            return CollCase<SHAPE_RECT, SHAPE_CIRCLE>( aA, aB, aClearance, aActual, aLocation, aMTV );

        case SH_LINE_CHAIN:
            return CollCase<SHAPE_RECT, SHAPE_LINE_CHAIN>( aA, aB, aClearance, aActual, aLocation, aMTV );

        case SH_SEGMENT:
            return CollCase<SHAPE_RECT, SHAPE_SEGMENT>( aA, aB, aClearance, aActual, aLocation, aMTV );

        case SH_SIMPLE:
        case SH_POLY_SET_TRIANGLE:
            return CollCase<SHAPE_RECT, SHAPE_LINE_CHAIN_BASE>( aA, aB, aClearance, aActual, aLocation, aMTV );

        case SH_ARC:
            return CollCaseReversed<SHAPE_RECT, SHAPE_ARC>( aA, aB, aClearance, aActual, aLocation, aMTV );

        case SH_NULL:
            return false;

        default:
            break;
        }
        break;

    case SH_CIRCLE:
        switch( aB->Type() )
        {
        case SH_RECT:
            return CollCaseReversed<SHAPE_CIRCLE, SHAPE_RECT>( aA, aB, aClearance, aActual, aLocation, aMTV );

        case SH_CIRCLE:
            return CollCase<SHAPE_CIRCLE, SHAPE_CIRCLE>( aA, aB, aClearance, aActual, aLocation, aMTV );

        case SH_LINE_CHAIN:
            return CollCase<SHAPE_CIRCLE, SHAPE_LINE_CHAIN>( aA, aB, aClearance, aActual, aLocation, aMTV );

        case SH_SEGMENT:
            return CollCase<SHAPE_CIRCLE, SHAPE_SEGMENT>( aA, aB, aClearance, aActual, aLocation, aMTV );

        case SH_SIMPLE:
        case SH_POLY_SET_TRIANGLE:
            return CollCase<SHAPE_CIRCLE, SHAPE_LINE_CHAIN_BASE>( aA, aB, aClearance, aActual, aLocation, aMTV );

        case SH_ARC:
            return CollCaseReversed<SHAPE_CIRCLE, SHAPE_ARC>( aA, aB, aClearance, aActual, aLocation, aMTV );
    
        case SH_NULL:
            return false;

        default:
            break;
        }
        break;

    case SH_LINE_CHAIN:
        switch( aB->Type() )
        {
        case SH_RECT:
            return CollCase<SHAPE_RECT, SHAPE_LINE_CHAIN>( aB, aA, aClearance, aActual, aLocation, aMTV );

        case SH_CIRCLE:
            return CollCase<SHAPE_CIRCLE, SHAPE_LINE_CHAIN>( aB, aA, aClearance, aActual, aLocation, aMTV );

        case SH_LINE_CHAIN:
            return CollCase<SHAPE_LINE_CHAIN, SHAPE_LINE_CHAIN>( aA, aB, aClearance, aActual, aLocation, aMTV );

        case SH_SEGMENT:
            return CollCase<SHAPE_LINE_CHAIN, SHAPE_SEGMENT>( aA, aB, aClearance, aActual, aLocation, aMTV );

        case SH_SIMPLE:
        case SH_POLY_SET_TRIANGLE:
            return CollCase<SHAPE_LINE_CHAIN, SHAPE_LINE_CHAIN_BASE>( aA, aB, aClearance, aActual, aLocation, aMTV );

        case SH_ARC:
            return CollCaseReversed<SHAPE_LINE_CHAIN, SHAPE_ARC>( aA, aB, aClearance, aActual, aLocation, aMTV );

        case SH_NULL:
            return false;

        default:
            break;
        }
        break;

    case SH_SEGMENT:
        switch( aB->Type() )
        {
        case SH_RECT:
            return CollCase<SHAPE_RECT, SHAPE_SEGMENT>( aB, aA, aClearance, aActual, aLocation, aMTV );

        case SH_CIRCLE:
            return CollCaseReversed<SHAPE_SEGMENT, SHAPE_CIRCLE>( aA, aB, aClearance, aActual, aLocation, aMTV );

        case SH_LINE_CHAIN:
            return CollCase<SHAPE_LINE_CHAIN, SHAPE_SEGMENT>( aB, aA, aClearance, aActual, aLocation, aMTV );

        case SH_SEGMENT:
            return CollCase<SHAPE_SEGMENT, SHAPE_SEGMENT>( aA, aB, aClearance, aActual, aLocation, aMTV );

        case SH_SIMPLE:
        case SH_POLY_SET_TRIANGLE:
            return CollCase<SHAPE_LINE_CHAIN_BASE, SHAPE_SEGMENT>( aB, aA, aClearance, aActual, aLocation, aMTV );

        case SH_ARC:
            return CollCaseReversed<SHAPE_SEGMENT, SHAPE_ARC>( aA, aB, aClearance, aActual, aLocation, aMTV );

        case SH_NULL:
            return false;

        default:
            break;
        }
        break;

    case SH_SIMPLE:
    case SH_POLY_SET_TRIANGLE:
        switch( aB->Type() )
        {
        case SH_RECT:
            return CollCase<SHAPE_RECT, SHAPE_LINE_CHAIN_BASE>( aB, aA, aClearance, aActual, aLocation, aMTV );

        case SH_CIRCLE:
            return CollCase<SHAPE_CIRCLE, SHAPE_LINE_CHAIN_BASE>( aB, aA, aClearance, aActual, aLocation, aMTV );

        case SH_LINE_CHAIN:
            return CollCase<SHAPE_LINE_CHAIN, SHAPE_LINE_CHAIN_BASE>( aB, aA, aClearance, aActual, aLocation, aMTV );

        case SH_SEGMENT:
            return CollCase<SHAPE_LINE_CHAIN_BASE, SHAPE_SEGMENT>( aA, aB, aClearance, aActual, aLocation, aMTV );

        case SH_SIMPLE:
        case SH_POLY_SET_TRIANGLE:
            return CollCase<SHAPE_LINE_CHAIN_BASE, SHAPE_LINE_CHAIN_BASE>( aA, aB, aClearance, aActual, aLocation, aMTV );

        case SH_ARC:
            return CollCaseReversed<SHAPE_LINE_CHAIN_BASE, SHAPE_ARC>( aA, aB, aClearance, aActual, aLocation, aMTV );

        case SH_NULL:
            return false;

        default:
            break;
        }
        break;

    case SH_ARC:
        switch( aB->Type() )
        {
        case SH_RECT:
            return CollCase<SHAPE_ARC, SHAPE_RECT>( aA, aB, aClearance, aActual, aLocation, aMTV );

        case SH_CIRCLE:
            return CollCase<SHAPE_ARC, SHAPE_CIRCLE>( aA, aB, aClearance, aActual, aLocation, aMTV );

        case SH_LINE_CHAIN:
            return CollCase<SHAPE_ARC, SHAPE_LINE_CHAIN>( aA, aB, aClearance, aActual, aLocation, aMTV );

        case SH_SEGMENT:
            return CollCase<SHAPE_ARC, SHAPE_SEGMENT>( aA, aB, aClearance, aActual, aLocation, aMTV );

        case SH_SIMPLE:
        case SH_POLY_SET_TRIANGLE:
            return CollCase<SHAPE_ARC, SHAPE_LINE_CHAIN_BASE>( aA, aB, aClearance, aActual, aLocation, aMTV );

        case SH_ARC:
            return CollCase<SHAPE_ARC, SHAPE_ARC>( aA, aB, aClearance, aActual, aLocation, aMTV );

        case SH_NULL:
            return false;

        default:
            break;
        }
        break;

    default:
        break;
    }

    bool unsupported_collision = true;
    (void) unsupported_collision;   // make gcc quiet

    wxASSERT( unsupported_collision == false );

    return false;
}

static bool collideShapes( const SHAPE* aA, const SHAPE* aB, int aClearance, int* aActual,
                           VECTOR2I* aLocation, VECTOR2I* aMTV )
{
    int currentActual = std::numeric_limits<int>::max();
    VECTOR2I currentLocation;
    VECTOR2I currentMTV(0, 0);
    bool colliding = false;

    auto canExit =
            [&]()
            {
                if( !colliding )
                    return false;

                if( aActual && currentActual > 0 )
                    return false;

                if( aMTV )
                    return false;

                return true;
            };

    auto collideCompoundSubshapes =
            [&]( const SHAPE* elemA, const SHAPE* elemB, int clearance ) -> bool
            {
                int actual = 0;
                VECTOR2I location;
                VECTOR2I mtv;

                if( collideSingleShapes( elemA, elemB, clearance,
                                         aActual || aLocation ? &actual : nullptr,
                                         aLocation ? &location : nullptr,
                                         aMTV ? &mtv : nullptr ) )
                {
                    if( actual < currentActual )
                    {
                        currentActual = actual;
                        currentLocation = location;
                    }

                    if( aMTV && mtv.SquaredEuclideanNorm() > currentMTV.SquaredEuclideanNorm() )
                    {
                        currentMTV = mtv;
                    }

                    return true;
                }

                return false;
            };

    if( aA->Type() == SH_COMPOUND && aB->Type() == SH_COMPOUND )
    {
        const SHAPE_COMPOUND* cmpA = static_cast<const SHAPE_COMPOUND*>( aA );
        const SHAPE_COMPOUND* cmpB = static_cast<const SHAPE_COMPOUND*>( aB );

        for( const SHAPE* elemA : cmpA->Shapes() )
        {
            for( const SHAPE* elemB : cmpB->Shapes() )
            {
                if( collideCompoundSubshapes( elemA, elemB, aClearance ) )
                {
                    colliding = true;

                    if( canExit() )
                        break;
                }
            }

            if( canExit() )
                break;
        }
    }
    else if( aA->Type() == SH_COMPOUND )
    {
        const SHAPE_COMPOUND* cmpA = static_cast<const SHAPE_COMPOUND*>( aA );

        for( const SHAPE* elemA : cmpA->Shapes() )
        {
            if( collideCompoundSubshapes( elemA, aB, aClearance ) )
            {
                colliding = true;

                if( canExit() )
                    break;
            }
        }
    }
    else if( aB->Type() == SH_COMPOUND )
    {
        const SHAPE_COMPOUND* cmpB = static_cast<const SHAPE_COMPOUND*>( aB );

        for( const SHAPE* elemB : cmpB->Shapes() )
        {
            if( collideCompoundSubshapes( aA, elemB, aClearance ) )
            {
                colliding = true;

                if( canExit() )
                    break;
            }
        }
    }
    else
    {
        return collideSingleShapes( aA, aB, aClearance, aActual, aLocation, aMTV );
    }
    
    if( colliding )
    {
        if( aLocation )
            *aLocation = currentLocation;

        if( aActual )
            *aActual = currentActual;

        if( aMTV )
            *aMTV = currentMTV;
    }

    return colliding;
}

bool SHAPE::Collide( const SHAPE* aShape, int aClearance, VECTOR2I* aMTV ) const
{
    return collideShapes( this, aShape, aClearance, nullptr, nullptr, aMTV );
}


bool SHAPE::Collide( const SHAPE* aShape, int aClearance, int* aActual, VECTOR2I* aLocation ) const
{
    return collideShapes( this, aShape, aClearance, aActual, aLocation, nullptr );
}


