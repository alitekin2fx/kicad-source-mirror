/*
 * This program source code file is part of KICAD, a free EDA CAD application.
 *
 * Copyright (C) 2017-2019 Kicad Developers, see AUTHORS.txt for contributors.
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
#include <limits>

#include <preview_items/polygon_geom_manager.h>

#include <geometry/geometry_utils.h>


POLYGON_GEOM_MANAGER::POLYGON_GEOM_MANAGER( CLIENT& aClient ):
    m_client( aClient ),
    m_leaderMode( LEADER_MODE::DIRECT ),
    m_intersectionsAllowed( true )
{}


bool POLYGON_GEOM_MANAGER::AddPoint( const VECTOR2I& aPt )
{
    // if this is the first point, make sure the client is happy
    // for us to continue
    if( !IsPolygonInProgress() && !m_client.OnFirstPoint( *this ) )
        return false;

    if( m_leaderPts.PointCount() > 1 )
    {
        // there are enough leader points - the next
        // locked-in point is the end of the first leader
        // segment
        m_lockedPoints.Append( m_leaderPts.CPoint( 1 ) );
    }
    else
    {
        // no leader lines, directly add the cursor
        m_lockedPoints.Append( aPt );
    }

    // check for self-intersections
    if( !m_intersectionsAllowed && IsSelfIntersecting( false ) )
    {
        m_lockedPoints.Remove( m_lockedPoints.PointCount() - 1 );
        return false;
    }

    m_client.OnGeometryChange( *this );
    return true;
}


void POLYGON_GEOM_MANAGER::SetFinished()
{

    m_client.OnComplete( *this );
}


void POLYGON_GEOM_MANAGER::SetLeaderMode( LEADER_MODE aMode )
{
    m_leaderMode = aMode;
}


bool POLYGON_GEOM_MANAGER::IsSelfIntersecting( bool aIncludeLeaderPts ) const
{
    auto pts( m_lockedPoints );

    if( aIncludeLeaderPts )
    {
        for( int i = 0; i < m_leaderPts.PointCount(); ++i )
            if( m_leaderPts.CPoint( i ) != pts.CPoint( 0 ) )
                pts.Append( m_leaderPts.CPoint( i ) );
    }

    // line chain needs to be set as closed for proper checks
    pts.SetClosed( true );

    return !!pts.SelfIntersecting();
}


void POLYGON_GEOM_MANAGER::SetCursorPosition( const VECTOR2I& aPos )
{
    updateLeaderPoints( aPos );
}


bool POLYGON_GEOM_MANAGER::IsPolygonInProgress() const
{
    return m_lockedPoints.PointCount() > 0;
}


bool POLYGON_GEOM_MANAGER::NewPointClosesOutline( const VECTOR2I& aPt ) const
{
    return m_lockedPoints.PointCount() > 0 && m_lockedPoints.CPoint( 0 ) == aPt;
}


void POLYGON_GEOM_MANAGER::DeleteLastCorner()
{
    if( m_lockedPoints.PointCount() > 0 )
    {
        m_lockedPoints.Remove( m_lockedPoints.PointCount() - 1 );
    }

    // update the new last segment (was previously
    // locked in), reusing last constraints
    if( m_lockedPoints.PointCount() > 0 )
    {
        updateLeaderPoints( m_leaderPts.CLastPoint() );
    }

    m_client.OnGeometryChange( *this );
}


void POLYGON_GEOM_MANAGER::Reset()
{
    m_lockedPoints.Clear();
    m_leaderPts.Clear();

    m_client.OnGeometryChange( *this );
}


void POLYGON_GEOM_MANAGER::updateLeaderPoints( const VECTOR2I& aEndPoint, LEADER_MODE aModifier )
{
    wxCHECK( m_lockedPoints.PointCount() > 0, /*void*/ );
    const VECTOR2I& lastPt = m_lockedPoints.CLastPoint();

    if( m_leaderMode == LEADER_MODE::DEG45 || aModifier == LEADER_MODE::DEG45 )
    {
        const VECTOR2I lineVector( aEndPoint - lastPt );
        // get a restricted 45/H/V line from the last fixed point to the cursor
        auto newEnd = lastPt + GetVectorSnapped45( lineVector );
        OPT_VECTOR2I pt;

        if( m_lockedPoints.SegmentCount() >0 )
        {
            SEG first( lastPt, newEnd );
            SEG test_seg = m_lockedPoints.CSegment( 0 );

            pt = first.IntersectLines( m_lockedPoints.CSegment( 0 ) );
            int  dist = pt ? ( aEndPoint - *pt ).EuclideanNorm() : std::numeric_limits<int>::max();

            for( int i = 1; i < 8; i++ )
            {
                test_seg.B = ( test_seg.B - test_seg.A ).Rotate( M_PI_4 ) + test_seg.A;
                auto pt2 = first.IntersectLines( test_seg );
                if( pt2 )
                {
                    int dist2 = ( aEndPoint - *pt2 ).EuclideanNorm();
                    if( dist2 < dist )
                    {
                        dist = dist2;
                        pt = pt2;
                    }
                }
            }
        }

        m_leaderPts = SHAPE_LINE_CHAIN( lastPt, newEnd );

        if( pt )
        {
            // This checks for backtracking from the point to intersection
            if( SEG( lastPt, newEnd ).Collinear( SEG( newEnd, *pt ) ) )
                m_leaderPts = SHAPE_LINE_CHAIN( lastPt, *pt );
            else
                m_leaderPts.Append( *pt );
        }
    }
    else
    {
        // direct segment
        m_leaderPts = SHAPE_LINE_CHAIN( lastPt, aEndPoint );
    }

    m_client.OnGeometryChange( *this );
}
