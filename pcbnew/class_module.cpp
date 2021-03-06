/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2017 Jean-Pierre Charras, jp.charras at wanadoo.fr
 * Copyright (C) 2015 SoftPLC Corporation, Dick Hollenbeck <dick@softplc.com>
 * Copyright (C) 2015 Wayne Stambaugh <stambaughw@gmail.com>
 * Copyright (C) 1992-2020 KiCad Developers, see AUTHORS.txt for contributors.
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

#include <confirm.h>
#include <refdes_utils.h>
#include <bitmaps.h>
#include <unordered_set>
#include <kicad_string.h>
#include <pcb_edit_frame.h>
#include <class_board.h>
#include <fp_shape.h>
#include <class_module.h>
#include <view/view.h>
#include <geometry/shape_null.h>
#include <i18n_utility.h>
#include <convert_drawsegment_list_to_polygon.h>


MODULE::MODULE( BOARD* parent ) :
    BOARD_ITEM_CONTAINER( (BOARD_ITEM*) parent, PCB_MODULE_T ),
    m_initial_comments( 0 )
{
    m_Attributs    = 0;
    m_Layer        = F_Cu;
    m_Orient       = 0;
    m_ModuleStatus = MODULE_PADS_LOCKED;
    m_arflag = 0;
    m_CntRot90 = m_CntRot180 = 0;
    m_Link     = 0;
    m_LastEditTime  = 0;
    m_LocalClearance = 0;
    m_LocalSolderMaskMargin  = 0;
    m_LocalSolderPasteMargin = 0;
    m_LocalSolderPasteMarginRatio = 0.0;
    m_ZoneConnection              = ZONE_CONNECTION::INHERITED; // Use zone setting by default
    m_ThermalWidth = 0;     // Use zone setting by default
    m_ThermalGap = 0;       // Use zone setting by default

    // These are special and mandatory text fields
    m_Reference = new FP_TEXT( this, FP_TEXT::TEXT_is_REFERENCE );
    m_Value = new FP_TEXT( this, FP_TEXT::TEXT_is_VALUE );

    m_3D_Drawings.clear();
}


MODULE::MODULE( const MODULE& aFootprint ) :
    BOARD_ITEM_CONTAINER( aFootprint )
{
    m_Pos = aFootprint.m_Pos;
    m_fpid = aFootprint.m_fpid;
    m_Attributs = aFootprint.m_Attributs;
    m_ModuleStatus = aFootprint.m_ModuleStatus;
    m_Orient = aFootprint.m_Orient;
    m_BoundaryBox = aFootprint.m_BoundaryBox;
    m_CntRot90 = aFootprint.m_CntRot90;
    m_CntRot180 = aFootprint.m_CntRot180;
    m_LastEditTime = aFootprint.m_LastEditTime;
    m_Link = aFootprint.m_Link;
    m_Path = aFootprint.m_Path;

    m_LocalClearance = aFootprint.m_LocalClearance;
    m_LocalSolderMaskMargin = aFootprint.m_LocalSolderMaskMargin;
    m_LocalSolderPasteMargin = aFootprint.m_LocalSolderPasteMargin;
    m_LocalSolderPasteMarginRatio = aFootprint.m_LocalSolderPasteMarginRatio;
    m_ZoneConnection = aFootprint.m_ZoneConnection;
    m_ThermalWidth = aFootprint.m_ThermalWidth;
    m_ThermalGap = aFootprint.m_ThermalGap;

    // Copy reference and value.
    m_Reference = new FP_TEXT( *aFootprint.m_Reference );
    m_Reference->SetParent( this );
    m_Value = new FP_TEXT( *aFootprint.m_Value );
    m_Value->SetParent( this );

    std::map<BOARD_ITEM*, BOARD_ITEM*> ptrMap;

    // Copy pads
    for( D_PAD* pad : aFootprint.Pads() )
    {
        D_PAD* newPad = static_cast<D_PAD*>( pad->Clone() );
        ptrMap[ pad ] = newPad;
        Add( newPad );
    }

    // Copy zones
    for( MODULE_ZONE_CONTAINER* zone : aFootprint.Zones() )
    {
        MODULE_ZONE_CONTAINER* newZone = static_cast<MODULE_ZONE_CONTAINER*>( zone->Clone() );
        ptrMap[ zone ] = newZone;
        Add( newZone );

        // Ensure the net info is OK and especially uses the net info list
        // living in the current board
        // Needed when copying a fp from fp editor that has its own board
        // Must be NETINFO_LIST::ORPHANED_ITEM for a keepout that has no net.
        newZone->SetNetCode( -1 );
    }

    // Copy drawings
    for( BOARD_ITEM* item : aFootprint.GraphicalItems() )
    {
        BOARD_ITEM* newItem = static_cast<BOARD_ITEM*>( item->Clone() );
        ptrMap[ item ] = newItem;
        Add( newItem );
    }

    // Copy groups
    for( PCB_GROUP* group : aFootprint.Groups() )
    {
        PCB_GROUP* newGroup = static_cast<PCB_GROUP*>( group->Clone() );
        ptrMap[ group ] = newGroup;
        Add( newGroup );
    }

    // Rebuild groups
    for( PCB_GROUP* group : aFootprint.Groups() )
    {
        PCB_GROUP* newGroup = static_cast<PCB_GROUP*>( ptrMap[ group ] );

        const_cast<std::unordered_set<BOARD_ITEM*>*>( &newGroup->GetItems() )->clear();

        for( BOARD_ITEM* member : group->GetItems() )
            newGroup->AddItem( ptrMap[ member ] );
    }

    // Copy auxiliary data: 3D_Drawings info
    m_3D_Drawings = aFootprint.m_3D_Drawings;

    m_Doc         = aFootprint.m_Doc;
    m_KeyWord     = aFootprint.m_KeyWord;
    m_properties  = aFootprint.m_properties;

    m_arflag = 0;

    // Ensure auxiliary data is up to date
    CalculateBoundingBox();

    m_initial_comments = aFootprint.m_initial_comments ?
                         new wxArrayString( *aFootprint.m_initial_comments ) : nullptr;
}


MODULE::MODULE( MODULE&& aFootprint ) :
    BOARD_ITEM_CONTAINER( aFootprint )
{
    *this = std::move( aFootprint );
}


MODULE::~MODULE()
{
    // Clean up the owned elements
    delete m_Reference;
    delete m_Value;
    delete m_initial_comments;

    for( D_PAD* p : m_pads )
        delete p;

    m_pads.clear();

    for( MODULE_ZONE_CONTAINER* zone : m_fp_zones )
        delete zone;

    m_fp_zones.clear();

    for( PCB_GROUP* group : m_fp_groups )
        delete group;

    m_fp_groups.clear();

    for( BOARD_ITEM* d : m_drawings )
        delete d;

    m_drawings.clear();
}


MODULE& MODULE::operator=( MODULE&& aOther )
{
    BOARD_ITEM::operator=( aOther );

    m_Pos           = aOther.m_Pos;
    m_fpid          = aOther.m_fpid;
    m_Attributs     = aOther.m_Attributs;
    m_ModuleStatus  = aOther.m_ModuleStatus;
    m_Orient        = aOther.m_Orient;
    m_BoundaryBox   = aOther.m_BoundaryBox;
    m_CntRot90      = aOther.m_CntRot90;
    m_CntRot180     = aOther.m_CntRot180;
    m_LastEditTime  = aOther.m_LastEditTime;
    m_Link          = aOther.m_Link;
    m_Path          = aOther.m_Path;

    m_LocalClearance                = aOther.m_LocalClearance;
    m_LocalSolderMaskMargin         = aOther.m_LocalSolderMaskMargin;
    m_LocalSolderPasteMargin        = aOther.m_LocalSolderPasteMargin;
    m_LocalSolderPasteMarginRatio   = aOther.m_LocalSolderPasteMarginRatio;
    m_ZoneConnection                = aOther.m_ZoneConnection;
    m_ThermalWidth                  = aOther.m_ThermalWidth;
    m_ThermalGap                    = aOther.m_ThermalGap;

    // Move reference and value
    m_Reference = aOther.m_Reference;
    m_Reference->SetParent( this );
    m_Value = aOther.m_Value;
    m_Value->SetParent( this );


    // Move the pads
    m_pads.clear();

    for( D_PAD* pad : aOther.Pads() )
        Add( pad );

    aOther.Pads().clear();

    // Move the zones
    m_fp_zones.clear();

    for( MODULE_ZONE_CONTAINER* item : aOther.Zones() )
    {
        Add( item );

        // Ensure the net info is OK and especially uses the net info list
        // living in the current board
        // Needed when copying a fp from fp editor that has its own board
        // Must be NETINFO_LIST::ORPHANED_ITEM for a keepout that has no net.
        item->SetNetCode( -1 );
    }

    aOther.Zones().clear();

    // Move the drawings
    m_drawings.clear();

    for( BOARD_ITEM* item : aOther.GraphicalItems() )
        Add( item );

    aOther.GraphicalItems().clear();

    // Move the groups
    m_fp_groups.clear();

    for( PCB_GROUP* group : aOther.Groups() )
        Add( group );

    aOther.Groups().clear();

    // Copy auxiliary data: 3D_Drawings info
    m_3D_Drawings.clear();
    m_3D_Drawings = aOther.m_3D_Drawings;
    m_Doc         = aOther.m_Doc;
    m_KeyWord     = aOther.m_KeyWord;
    m_properties  = aOther.m_properties;

    // Ensure auxiliary data is up to date
    CalculateBoundingBox();

    m_initial_comments = aOther.m_initial_comments;

    // Clear the other item's containers since this is a move
    aOther.Pads().clear();
    aOther.Zones().clear();
    aOther.GraphicalItems().clear();
    aOther.m_Value            = nullptr;
    aOther.m_Reference        = nullptr;
    aOther.m_initial_comments = nullptr;

    return *this;
}


MODULE& MODULE::operator=( const MODULE& aOther )
{
    BOARD_ITEM::operator=( aOther );

    m_Pos           = aOther.m_Pos;
    m_fpid          = aOther.m_fpid;
    m_Attributs     = aOther.m_Attributs;
    m_ModuleStatus  = aOther.m_ModuleStatus;
    m_Orient        = aOther.m_Orient;
    m_BoundaryBox   = aOther.m_BoundaryBox;
    m_CntRot90      = aOther.m_CntRot90;
    m_CntRot180     = aOther.m_CntRot180;
    m_LastEditTime  = aOther.m_LastEditTime;
    m_Link          = aOther.m_Link;
    m_Path          = aOther.m_Path;

    m_LocalClearance                = aOther.m_LocalClearance;
    m_LocalSolderMaskMargin         = aOther.m_LocalSolderMaskMargin;
    m_LocalSolderPasteMargin        = aOther.m_LocalSolderPasteMargin;
    m_LocalSolderPasteMarginRatio   = aOther.m_LocalSolderPasteMarginRatio;
    m_ZoneConnection                = aOther.m_ZoneConnection;
    m_ThermalWidth                  = aOther.m_ThermalWidth;
    m_ThermalGap                    = aOther.m_ThermalGap;

    // Copy reference and value
    *m_Reference = *aOther.m_Reference;
    m_Reference->SetParent( this );
    *m_Value = *aOther.m_Value;
    m_Value->SetParent( this );

    std::map<BOARD_ITEM*, BOARD_ITEM*> ptrMap;

    // Copy pads
    m_pads.clear();

    for( D_PAD* pad : aOther.Pads() )
    {
        D_PAD* newPad = new D_PAD( *pad );
        ptrMap[ pad ] = newPad;
        Add( newPad );
    }

    // Copy zones
    m_fp_zones.clear();

    for( MODULE_ZONE_CONTAINER* zone : aOther.Zones() )
    {
        MODULE_ZONE_CONTAINER* newZone = static_cast<MODULE_ZONE_CONTAINER*>( zone->Clone() );
        ptrMap[ zone ] = newZone;
        Add( newZone );

        // Ensure the net info is OK and especially uses the net info list
        // living in the current board
        // Needed when copying a fp from fp editor that has its own board
        // Must be NETINFO_LIST::ORPHANED_ITEM for a keepout that has no net.
        newZone->SetNetCode( -1 );
    }

    // Copy drawings
    m_drawings.clear();

    for( BOARD_ITEM* item : aOther.GraphicalItems() )
    {
        BOARD_ITEM* newItem = static_cast<BOARD_ITEM*>( item->Clone() );
        ptrMap[ item ] = newItem;
        Add( newItem );
    }

    // Copy groups
    m_fp_groups.clear();

    for( PCB_GROUP* group : aOther.Groups() )
    {
        PCB_GROUP* newGroup = static_cast<PCB_GROUP*>( group->Clone() );
        const_cast<std::unordered_set<BOARD_ITEM*>*>( &newGroup->GetItems() )->clear();

        for( BOARD_ITEM* member : group->GetItems() )
            newGroup->AddItem( ptrMap[ member ] );

        Add( newGroup );
    }

    // Copy auxiliary data: 3D_Drawings info
    m_3D_Drawings.clear();
    m_3D_Drawings = aOther.m_3D_Drawings;
    m_Doc         = aOther.m_Doc;
    m_KeyWord     = aOther.m_KeyWord;
    m_properties  = aOther.m_properties;

    // Ensure auxiliary data is up to date
    CalculateBoundingBox();

    m_initial_comments = aOther.m_initial_comments ?
                            new wxArrayString( *aOther.m_initial_comments ) : nullptr;

    return *this;
}


void MODULE::GetContextualTextVars( wxArrayString* aVars ) const
{
    aVars->push_back( wxT( "REFERENCE" ) );
    aVars->push_back( wxT( "VALUE" ) );
    aVars->push_back( wxT( "LAYER" ) );
}


bool MODULE::ResolveTextVar( wxString* token, int aDepth ) const
{
    if( token->IsSameAs( wxT( "REFERENCE" ) ) )
    {
        *token = m_Reference->GetShownText( aDepth + 1 );
        return true;
    }
    else if( token->IsSameAs( wxT( "VALUE" ) ) )
    {
        *token = m_Value->GetShownText( aDepth + 1 );
        return true;
    }
    else if( token->IsSameAs( wxT( "LAYER" ) ) )
    {
        *token = GetLayerName();
        return true;
    }
    else if( m_properties.count( *token ) )
    {
        *token = m_properties.at( *token );
        return true;
    }

    return false;
}


void MODULE::ClearAllNets()
{
    // Force the ORPHANED dummy net info for all pads.
    // ORPHANED dummy net does not depend on a board
    for( D_PAD* pad : m_pads )
        pad->SetNetCode( NETINFO_LIST::ORPHANED );
}


void MODULE::Add( BOARD_ITEM* aBoardItem, ADD_MODE aMode )
{
    switch( aBoardItem->Type() )
    {
    case PCB_FP_TEXT_T:
        // Only user text can be added this way.
        assert( static_cast<FP_TEXT*>( aBoardItem )->GetType() == FP_TEXT::TEXT_is_DIVERS );
        KI_FALLTHROUGH;

    case PCB_FP_SHAPE_T:
        if( aMode == ADD_MODE::APPEND )
            m_drawings.push_back( aBoardItem );
        else
            m_drawings.push_front( aBoardItem );
        break;

    case PCB_PAD_T:
        if( aMode == ADD_MODE::APPEND )
            m_pads.push_back( static_cast<D_PAD*>( aBoardItem ) );
        else
            m_pads.push_front( static_cast<D_PAD*>( aBoardItem ) );
        break;

    case PCB_FP_ZONE_AREA_T:
        if( aMode == ADD_MODE::APPEND )
            m_fp_zones.push_back( static_cast<MODULE_ZONE_CONTAINER*>( aBoardItem ) );
        else
            m_fp_zones.insert( m_fp_zones.begin(), static_cast<MODULE_ZONE_CONTAINER*>( aBoardItem ) );
        break;

    case PCB_GROUP_T:
        if( aMode == ADD_MODE::APPEND )
            m_fp_groups.push_back( static_cast<PCB_GROUP*>( aBoardItem ) );
        else
            m_fp_groups.insert( m_fp_groups.begin(), static_cast<PCB_GROUP*>( aBoardItem ) );
        break;

    default:
    {
        wxString msg;
        msg.Printf( wxT( "MODULE::Add() needs work: BOARD_ITEM type (%d) not handled" ),
                    aBoardItem->Type() );
        wxFAIL_MSG( msg );

        return;
    }
    }

    aBoardItem->ClearEditFlags();
    aBoardItem->SetParent( this );
}


void MODULE::Remove( BOARD_ITEM* aBoardItem )
{
    switch( aBoardItem->Type() )
    {
    case PCB_FP_TEXT_T:
        // Only user text can be removed this way.
        wxCHECK_RET(
                static_cast<FP_TEXT*>( aBoardItem )->GetType() == FP_TEXT::TEXT_is_DIVERS,
                "Please report this bug: Invalid remove operation on required text" );
        KI_FALLTHROUGH;

    case PCB_FP_SHAPE_T:
        for( auto it = m_drawings.begin(); it != m_drawings.end(); ++it )
        {
            if( *it == aBoardItem )
            {
                m_drawings.erase( it );
                break;
            }
        }

        break;

    case PCB_PAD_T:
        for( auto it = m_pads.begin(); it != m_pads.end(); ++it )
        {
            if( *it == static_cast<D_PAD*>( aBoardItem ) )
            {
                m_pads.erase( it );
                break;
            }
        }

        break;

    case PCB_FP_ZONE_AREA_T:
        for( auto it = m_fp_zones.begin(); it != m_fp_zones.end(); ++it )
        {
            if( *it == static_cast<MODULE_ZONE_CONTAINER*>( aBoardItem ) )
            {
                m_fp_zones.erase( it );
                break;
            }
        }

        break;

    case PCB_GROUP_T:
        for( auto it = m_fp_groups.begin(); it != m_fp_groups.end(); ++it )
        {
            if( *it == static_cast<PCB_GROUP*>( aBoardItem ) )
            {
                m_fp_groups.erase( it );
                break;
            }
        }

        break;

    default:
    {
        wxString msg;
        msg.Printf( wxT( "MODULE::Remove() needs work: BOARD_ITEM type (%d) not handled" ),
                    aBoardItem->Type() );
        wxFAIL_MSG( msg );
    }
    }
}


void MODULE::CalculateBoundingBox()
{
    m_BoundaryBox = GetFootprintRect();
}


double MODULE::GetArea( int aPadding ) const
{
    double w = std::abs( static_cast<double>( m_BoundaryBox.GetWidth() ) ) + aPadding;
    double h = std::abs( static_cast<double>( m_BoundaryBox.GetHeight() ) ) + aPadding;
    return w * h;
}


EDA_RECT MODULE::GetFootprintRect() const
{
    EDA_RECT area;

    area.SetOrigin( m_Pos );
    area.SetEnd( m_Pos );
    area.Inflate( Millimeter2iu( 0.25 ) );   // Give a min size to the area

    for( BOARD_ITEM* item : m_drawings )
    {
        if( item->Type() == PCB_FP_SHAPE_T )
            area.Merge( item->GetBoundingBox() );
    }

    for( D_PAD* pad : m_pads )
        area.Merge( pad->GetBoundingBox() );

    for( MODULE_ZONE_CONTAINER* zone : m_fp_zones )
        area.Merge( zone->GetBoundingBox() );

    // Groups do not contribute to the rect, only their members

    return area;
}


EDA_RECT MODULE::GetFpPadsLocalBbox() const
{
    EDA_RECT area;

    // We want the bounding box of the footprint pads at rot 0, not flipped
    // Create such a image:
    MODULE dummy( *this );

    dummy.SetPosition( wxPoint( 0, 0 ) );

    if( dummy.IsFlipped() )
        dummy.Flip( wxPoint( 0, 0 ) , false );

    if( dummy.GetOrientation() )
        dummy.SetOrientation( 0 );

    for( D_PAD* pad : dummy.Pads() )
        area.Merge( pad->GetBoundingBox() );

    return area;
}


const EDA_RECT MODULE::GetBoundingBox() const
{
    return GetBoundingBox( true );
}


const EDA_RECT MODULE::GetBoundingBox( bool aIncludeInvisibleText ) const
{
    EDA_RECT area = GetFootprintRect();

    // Add in items not collected by GetFootprintRect():
    for( BOARD_ITEM* item : m_drawings )
    {
        if( item->Type() != PCB_FP_SHAPE_T )
            area.Merge( item->GetBoundingBox() );
    }

    // This can be further optimized when aIncludeInvisibleText is true, but currently
    // leaving this as is until it's determined there is a noticeable speed hit.
    bool   valueLayerIsVisible = true;
    bool   refLayerIsVisible   = true;
    BOARD* board               = GetBoard();

    if( board )
    {
        // The first "&&" conditional handles the user turning layers off as well as layers
        // not being present in the current PCB stackup.  Values, references, and all
        // footprint text can also be turned off via the GAL meta-layers, so the 2nd and
        // 3rd "&&" conditionals handle that.
        valueLayerIsVisible = board->IsLayerVisible( m_Value->GetLayer() )
                              && board->IsElementVisible( LAYER_MOD_VALUES )
                              && board->IsElementVisible( LAYER_MOD_TEXT_FR );

        refLayerIsVisible = board->IsLayerVisible( m_Reference->GetLayer() )
                            && board->IsElementVisible( LAYER_MOD_REFERENCES )
                            && board->IsElementVisible( LAYER_MOD_TEXT_FR );
    }


    if( ( m_Value->IsVisible() && valueLayerIsVisible ) || aIncludeInvisibleText )
        area.Merge( m_Value->GetBoundingBox() );

    if( ( m_Reference->IsVisible() && refLayerIsVisible ) || aIncludeInvisibleText )
        area.Merge( m_Reference->GetBoundingBox() );

    return area;
}


/**
 * This is a bit hacky right now for performance reasons.
 *
 * We assume that most footprints will have features aligned to the axes in
 * the zero-rotation state.  Therefore, if the footprint is rotated, we
 * temporarily rotate back to zero, get the bounding box (excluding reference
 * and value text) and then rotate the resulting poly back to the correct
 * orientation.
 *
 * This is more accurate than using the AABB when most footprints are rotated
 * off of the axes, but less accurate than computing some kind of bounding hull.
 * We should consider doing that instead at some point in the future if we can
 * use a performant algorithm and cache the result to avoid extra computing.
 */
SHAPE_POLY_SET MODULE::GetBoundingPoly() const
{
    SHAPE_POLY_SET poly;

    double orientation = GetOrientationRadians();

    MODULE temp = *this;
    temp.SetOrientation( 0.0 );
    BOX2I area = temp.GetFootprintRect();

    poly.NewOutline();

    VECTOR2I p = area.GetPosition();
    poly.Append( p );
    p.x = area.GetRight();
    poly.Append( p );
    p.y = area.GetBottom();
    poly.Append( p );
    p.x = area.GetX();
    poly.Append( p );

    BOARD* board = GetBoard();
    if( board )
    {
        int biggest_clearance = board->GetDesignSettings().GetBiggestClearanceValue();
        poly.Inflate( biggest_clearance, 4 );
    }

    poly.Inflate( Millimeter2iu( 0.01 ), 4 );
    poly.Rotate( -orientation, m_Pos );

    return poly;
}


void MODULE::GetMsgPanelInfo( EDA_DRAW_FRAME* aFrame, std::vector<MSG_PANEL_ITEM>& aList )
{
    wxString msg, msg2;

    aList.emplace_back( m_Reference->GetShownText(), m_Value->GetShownText(), DARKCYAN );

    if( aFrame->IsType( FRAME_FOOTPRINT_VIEWER )
        || aFrame->IsType( FRAME_FOOTPRINT_VIEWER_MODAL )
        || aFrame->IsType( FRAME_FOOTPRINT_EDITOR ) )
    {
        wxDateTime date( static_cast<time_t>( m_LastEditTime ) );

        // Date format: see http://www.cplusplus.com/reference/ctime/strftime
        if( m_LastEditTime && date.IsValid() )
            msg = date.Format( wxT( "%b %d, %Y" ) ); // Abbreviated_month_name Day, Year
        else
            msg = _( "Unknown" );

        aList.emplace_back( _( "Last Change" ), msg, BROWN );
    }
    else if( aFrame->IsType( FRAME_PCB_EDITOR ) )
    {
        aList.emplace_back( _( "Board Side" ), IsFlipped() ? _( "Back (Flipped)" )
                                                           : _( "Front" ), RED );
    }

    auto addToken = []( wxString* aStr, const wxString& aAttr )
                    {
                        if( !aStr->IsEmpty() )
                            *aStr += wxT( ", " );

                        *aStr += aAttr;
                    };

    wxString status;
    wxString attrs;

    if( IsLocked() )
        addToken( &status, _( "locked" ) );

    if( m_ModuleStatus & MODULE_is_PLACED )
        addToken( &status, _( "autoplaced" ) );

    if( m_Attributs & MOD_BOARD_ONLY )
        addToken( &attrs, _( "not in schematic" ) );

    if( m_Attributs & MOD_EXCLUDE_FROM_POS_FILES )
        addToken( &attrs, _( "exclude from pos files" ) );

    if( m_Attributs & MOD_EXCLUDE_FROM_BOM )
        addToken( &attrs, _( "exclude from BOM" ) );

    aList.emplace_back( _( "Status: " ) + status, _( "Attributes:" ) + wxS( " " ) + attrs, BROWN );

    msg.Printf( "%.2f", GetOrientationDegrees() );
    aList.emplace_back( _( "Rotation" ), msg, BROWN );

    msg.Printf( _( "Footprint: %s" ), m_fpid.Format().c_str() );
    msg2.Printf( _( "3D-Shape: %s" ),
                 m_3D_Drawings.empty() ? _( "none" ) : m_3D_Drawings.front().m_Filename );
    aList.emplace_back( msg, msg2, BLUE );

    msg.Printf( _( "Doc: %s" ), m_Doc );
    msg2.Printf( _( "Keywords: %s" ), m_KeyWord );
    aList.emplace_back( msg, msg2, BLACK );
}


bool MODULE::HitTest( const wxPoint& aPosition, int aAccuracy ) const
{
    EDA_RECT rect = m_BoundaryBox;//.GetBoundingBoxRotated( GetPosition(), m_Orient );
    return rect.Inflate( aAccuracy ).Contains( aPosition );
}


bool MODULE::HitTestAccurate( const wxPoint& aPosition, int aAccuracy ) const
{
    return GetBoundingPoly().Collide( aPosition, aAccuracy );
}


bool MODULE::HitTest( const EDA_RECT& aRect, bool aContained, int aAccuracy ) const
{
    EDA_RECT arect = aRect;
    arect.Inflate( aAccuracy );

    if( aContained )
        return arect.Contains( m_BoundaryBox );
    else
    {
        // If the rect does not intersect the bounding box, skip any tests
        if( !aRect.Intersects( GetBoundingBox() ) )
            return false;

        // Determine if any elements in the MODULE intersect the rect
        for( D_PAD* pad : m_pads )
        {
            if( pad->HitTest( arect, false, 0 ) )
                return true;
        }

        for( MODULE_ZONE_CONTAINER* zone : m_fp_zones )
        {
            if( zone->HitTest( arect, false, 0 ) )
                return true;
        }

        for( BOARD_ITEM* item : m_drawings )
        {
            if( item->HitTest( arect, false, 0 ) )
                return true;
        }

        // Groups are not hit-tested; only their members

        // No items were hit
        return false;
    }
}


D_PAD* MODULE::FindPadByName( const wxString& aPadName ) const
{
    for( D_PAD* pad : m_pads )
    {
        if( pad->GetName() == aPadName )
            return pad;
    }

    return NULL;
}


D_PAD* MODULE::GetPad( const wxPoint& aPosition, LSET aLayerMask )
{
    for( D_PAD* pad : m_pads )
    {
        // ... and on the correct layer.
        if( !( pad->GetLayerSet() & aLayerMask ).any() )
            continue;

        if( pad->HitTest( aPosition ) )
            return pad;
    }

    return NULL;
}


D_PAD* MODULE::GetTopLeftPad()
{
    D_PAD* topLeftPad = GetFirstPad();

    for( D_PAD* p : m_pads )
    {
        wxPoint pnt = p->GetPosition(); // GetPosition() returns the center of the pad

        if( ( pnt.x < topLeftPad->GetPosition().x ) ||
            ( topLeftPad->GetPosition().x == pnt.x && pnt.y < topLeftPad->GetPosition().y ) )
        {
            topLeftPad = p;
        }
    }

    return topLeftPad;
}


unsigned MODULE::GetPadCount( INCLUDE_NPTH_T aIncludeNPTH ) const
{
    if( aIncludeNPTH )
        return m_pads.size();

    unsigned cnt = 0;

    for( D_PAD* pad : m_pads )
    {
        if( pad->GetAttribute() == PAD_ATTRIB_NPTH )
            continue;

        cnt++;
    }

    return cnt;
}


unsigned MODULE::GetUniquePadCount( INCLUDE_NPTH_T aIncludeNPTH ) const
{
    std::set<wxString> usedNames;

    // Create a set of used pad numbers
    for( D_PAD* pad : m_pads )
    {
        // Skip pads not on copper layers (used to build complex
        // solder paste shapes for instance)
        if( ( pad->GetLayerSet() & LSET::AllCuMask() ).none() )
            continue;

        // Skip pads with no name, because they are usually "mechanical"
        // pads, not "electrical" pads
        if( pad->GetName().IsEmpty() )
            continue;

        if( !aIncludeNPTH )
        {
            // skip NPTH
            if( pad->GetAttribute() == PAD_ATTRIB_NPTH )
            {
                continue;
            }
        }

        usedNames.insert( pad->GetName() );
    }

    return usedNames.size();
}


void MODULE::Add3DModel( MODULE_3D_SETTINGS* a3DModel )
{
    if( NULL == a3DModel )
        return;

    if( !a3DModel->m_Filename.empty() )
        m_3D_Drawings.push_back( *a3DModel );

    delete a3DModel;
}


// see class_module.h
SEARCH_RESULT MODULE::Visit( INSPECTOR inspector, void* testData, const KICAD_T scanTypes[] )
{
    KICAD_T        stype;
    SEARCH_RESULT  result = SEARCH_RESULT::CONTINUE;
    const KICAD_T* p    = scanTypes;
    bool           done = false;

#if 0 && defined(DEBUG)
    std::cout << GetClass().mb_str() << ' ';
#endif

    while( !done )
    {
        stype = *p;

        switch( stype )
        {
        case PCB_MODULE_T:
            result = inspector( this, testData );  // inspect me
            ++p;
            break;

        case PCB_PAD_T:
            result = IterateForward<D_PAD*>( m_pads, inspector, testData, p );
            ++p;
            break;

        case PCB_FP_ZONE_AREA_T:
            result = IterateForward<MODULE_ZONE_CONTAINER*>( m_fp_zones, inspector, testData, p );
            ++p;
            break;

        case PCB_FP_TEXT_T:
            result = inspector( m_Reference, testData );

            if( result == SEARCH_RESULT::QUIT )
                break;

            result = inspector( m_Value, testData );

            if( result == SEARCH_RESULT::QUIT )
                break;

            // Intentionally fall through since m_Drawings can hold TYPETEXTMODULE also
            KI_FALLTHROUGH;

        case PCB_FP_SHAPE_T:
            result = IterateForward<BOARD_ITEM*>( m_drawings, inspector, testData, p );

            // skip over any types handled in the above call.
            for( ; ; )
            {
                switch( stype = *++p )
                {
                case PCB_FP_TEXT_T:
                case PCB_FP_SHAPE_T:
                    continue;

                default:
                    ;
                }

                break;
            }

            break;

        case PCB_GROUP_T:
            result = IterateForward<PCB_GROUP*>( m_fp_groups, inspector, testData, p );
            ++p;
            break;

        default:
            done = true;
            break;
        }

        if( result == SEARCH_RESULT::QUIT )
            break;
    }

    return result;
}


wxString MODULE::GetSelectMenuText( EDA_UNITS aUnits ) const
{
    wxString reference = GetReference();

    if( reference.IsEmpty() )
        reference = _( "<no reference designator>" );

    return wxString::Format( _( "Footprint %s" ), reference );
}


BITMAP_DEF MODULE::GetMenuImage() const
{
    return module_xpm;
}


EDA_ITEM* MODULE::Clone() const
{
    return new MODULE( *this );
}


void MODULE::RunOnChildren( const std::function<void (BOARD_ITEM*)>& aFunction )
{
    try
    {
        for( D_PAD* pad : m_pads )
            aFunction( static_cast<BOARD_ITEM*>( pad ) );

        for( MODULE_ZONE_CONTAINER* zone : m_fp_zones )
            aFunction( static_cast<MODULE_ZONE_CONTAINER*>( zone ) );

        for( PCB_GROUP* group : m_fp_groups )
            aFunction( static_cast<PCB_GROUP*>( group ) );

        for( BOARD_ITEM* drawing : m_drawings )
            aFunction( static_cast<BOARD_ITEM*>( drawing ) );

        aFunction( static_cast<BOARD_ITEM*>( m_Reference ) );
        aFunction( static_cast<BOARD_ITEM*>( m_Value ) );
    }
    catch( std::bad_function_call& )
    {
        wxFAIL_MSG( "Error running MODULE::RunOnChildren" );
    }
}


void MODULE::GetAllDrawingLayers( int aLayers[], int& aCount, bool aIncludePads ) const
{
    std::unordered_set<int> layers;

    for( BOARD_ITEM* item : m_drawings )
        layers.insert( static_cast<int>( item->GetLayer() ) );

    if( aIncludePads )
    {
        for( D_PAD* pad : m_pads )
        {
            int pad_layers[KIGFX::VIEW::VIEW_MAX_LAYERS], pad_layers_count;
            pad->ViewGetLayers( pad_layers, pad_layers_count );

            for( int i = 0; i < pad_layers_count; i++ )
                layers.insert( pad_layers[i] );
        }
    }

    aCount = layers.size();
    int i = 0;

    for( int layer : layers )
        aLayers[i++] = layer;
}


void MODULE::ViewGetLayers( int aLayers[], int& aCount ) const
{
    aCount = 2;
    aLayers[0] = LAYER_ANCHOR;

    switch( m_Layer )
    {
    default:
        wxASSERT_MSG( false, "Illegal layer" );    // do you really have footprints placed on
                                                   // other layers?
        KI_FALLTHROUGH;

    case F_Cu:
        aLayers[1] = LAYER_MOD_FR;
        break;

    case B_Cu:
        aLayers[1] = LAYER_MOD_BK;
        break;
    }

    // If there are no pads, and only drawings on a silkscreen layer, then report the silkscreen
    // layer as well so that the component can be edited with the silkscreen layer
    bool f_silk = false, b_silk = false, non_silk = false;

    for( BOARD_ITEM* item : m_drawings )
    {
        if( item->GetLayer() == F_SilkS )
            f_silk = true;
        else if( item->GetLayer() == B_SilkS )
            b_silk = true;
        else
            non_silk = true;
    }

    if( ( f_silk || b_silk ) && !non_silk && m_pads.empty() )
    {
        if( f_silk )
            aLayers[ aCount++ ] = F_SilkS;

        if( b_silk )
            aLayers[ aCount++ ] = B_SilkS;
    }
}


double MODULE::ViewGetLOD( int aLayer, KIGFX::VIEW* aView ) const
{
    int layer = ( m_Layer == F_Cu ) ? LAYER_MOD_FR :
                ( m_Layer == B_Cu ) ? LAYER_MOD_BK : LAYER_ANCHOR;

    // Currently this is only pertinent for the anchor layer; everything else is drawn from the
    // children.
    // The "good" value is experimentally chosen.
    #define MINIMAL_ZOOM_LEVEL_FOR_VISIBILITY 1.5

    if( aView->IsLayerVisible( layer ) )
        return MINIMAL_ZOOM_LEVEL_FOR_VISIBILITY;

    return std::numeric_limits<double>::max();
}


const BOX2I MODULE::ViewBBox() const
{
    EDA_RECT area = GetFootprintRect();

    // Calculate extended area including text fields
    area.Merge( m_Reference->GetBoundingBox() );
    area.Merge( m_Value->GetBoundingBox() );

    // Add the Clearance shape size: (shape around the pads when the clearance is shown.  Not
    // optimized, but the draw cost is small (perhaps smaller than optimization).
    BOARD* board = GetBoard();
    if( board )
    {
        int biggest_clearance = board->GetDesignSettings().GetBiggestClearanceValue();
        area.Inflate( biggest_clearance );
    }

    return area;
}


bool MODULE::IsLibNameValid( const wxString & aName )
{
    const wxChar * invalids = StringLibNameInvalidChars( false );

    if( aName.find_first_of( invalids ) != std::string::npos )
        return false;

    return true;
}


const wxChar* MODULE::StringLibNameInvalidChars( bool aUserReadable )
{
    // This list of characters is also duplicated in validators.cpp and
    // lib_id.cpp
    // TODO: Unify forbidden character lists
    static const wxChar invalidChars[] = wxT("%$<>\t\n\r\"\\/:");
    static const wxChar invalidCharsReadable[] = wxT("% $ < > 'tab' 'return' 'line feed' \\ \" / :");

    if( aUserReadable )
        return invalidCharsReadable;
    else
        return invalidChars;
}


void MODULE::Move( const wxPoint& aMoveVector )
{
    wxPoint newpos = m_Pos + aMoveVector;
    SetPosition( newpos );
}


void MODULE::Rotate( const wxPoint& aRotCentre, double aAngle )
{
    double  orientation = GetOrientation();
    double  newOrientation = orientation + aAngle;
    wxPoint newpos = m_Pos;
    RotatePoint( &newpos, aRotCentre, aAngle );
    SetPosition( newpos );
    SetOrientation( newOrientation );

    m_Reference->KeepUpright( orientation, newOrientation );
    m_Value->KeepUpright( orientation, newOrientation );

    for( BOARD_ITEM* item : m_drawings )
    {
        if( item->Type() == PCB_FP_TEXT_T )
            static_cast<FP_TEXT*>( item )->KeepUpright( orientation, newOrientation  );
    }
}


void MODULE::Flip( const wxPoint& aCentre, bool aFlipLeftRight )
{
    // Move footprint to its final position:
    wxPoint finalPos = m_Pos;

    // Now Flip the footprint.
    // Flipping a footprint is a specific transform:
    // it is not mirrored like a text.
    // We have to change the side, and ensure the footprint rotation is
    // modified accordint to the transform, because this parameter is used
    // in pick and place files, and when updating the footprint from library.
    // When flipped around the X axis (Y coordinates changed) orientation is negated
    // When flipped around the Y axis (X coordinates changed) orientation is 180 - old orient.
    // Because it is specfic to a footprint, we flip around the X axis, and after rotate 180 deg

    MIRROR( finalPos.y, aCentre.y );     /// Mirror the Y position (around the X axis)

    SetPosition( finalPos );

    // Flip layer
    SetLayer( FlipLayer( GetLayer() ) );

    // Reverse mirror orientation.
    m_Orient = -m_Orient;

    NORMALIZE_ANGLE_180( m_Orient );

    // Mirror pads to other side of board.
    for( D_PAD* pad : m_pads )
        pad->Flip( m_Pos, false );

    // Mirror zones to other side of board.
    for( ZONE_CONTAINER* zone : m_fp_zones )
        zone->Flip( m_Pos, aFlipLeftRight );

    // Mirror reference and value.
    m_Reference->Flip( m_Pos, false );
    m_Value->Flip( m_Pos, false );

    // Reverse mirror module graphics and texts.
    for( BOARD_ITEM* item : m_drawings )
    {
        switch( item->Type() )
        {
        case PCB_FP_SHAPE_T:
            static_cast<FP_SHAPE*>( item )->Flip( m_Pos, false );
            break;

        case PCB_FP_TEXT_T:
            static_cast<FP_TEXT*>( item )->Flip( m_Pos, false );
            break;

        default:
            wxMessageBox( wxT( "MODULE::Flip() error: Unknown Draw Type" ) );
            break;
        }
    }

    // Now rotate 180 deg if required
    if( aFlipLeftRight )
        Rotate( aCentre, 1800.0 );

    CalculateBoundingBox();
}


void MODULE::SetPosition( const wxPoint& aPos )
{
    wxPoint delta = aPos - m_Pos;

    m_Pos += delta;

    m_Reference->EDA_TEXT::Offset( delta );
    m_Value->EDA_TEXT::Offset( delta );

    for( D_PAD* pad : m_pads )
        pad->SetPosition( pad->GetPosition() + delta );

    for( ZONE_CONTAINER* zone : m_fp_zones )
        zone->Move( delta );

    for( BOARD_ITEM* item : m_drawings )
    {
        switch( item->Type() )
        {
        case PCB_FP_SHAPE_T:
        {
            FP_SHAPE* shape = static_cast<FP_SHAPE*>( item );
            shape->SetDrawCoord();
            break;
        }

        case PCB_FP_TEXT_T:
        {
            FP_TEXT* text = static_cast<FP_TEXT*>( item );
            text->EDA_TEXT::Offset( delta );
            break;
        }

        default:
            wxMessageBox( wxT( "Draw type undefined." ) );
            break;
        }
    }

    m_BoundaryBox.Move( delta );
}


void MODULE::MoveAnchorPosition( const wxPoint& aMoveVector )
{
    /* Move the reference point of the footprint
     * the footprints elements (pads, outlines, edges .. ) are moved
     * but:
     * - the footprint position is not modified.
     * - the relative (local) coordinates of these items are modified
     * - Draw coordinates are updated
     */


    // Update (move) the relative coordinates relative to the new anchor point.
    wxPoint moveVector = aMoveVector;
    RotatePoint( &moveVector, -GetOrientation() );

    // Update of the reference and value.
    m_Reference->SetPos0( m_Reference->GetPos0() + moveVector );
    m_Reference->SetDrawCoord();
    m_Value->SetPos0( m_Value->GetPos0() + moveVector );
    m_Value->SetDrawCoord();

    // Update the pad local coordinates.
    for( D_PAD* pad : m_pads )
    {
        pad->SetPos0( pad->GetPos0() + moveVector );
        pad->SetDrawCoord();
    }

    // Update the draw element coordinates.
    for( BOARD_ITEM* item : GraphicalItems() )
    {
        switch( item->Type() )
        {
        case PCB_FP_SHAPE_T:
            {
            FP_SHAPE* shape = static_cast<FP_SHAPE*>( item );
                shape->Move( moveVector );
            }
            break;

        case PCB_FP_TEXT_T:
            {
            FP_TEXT* text = static_cast<FP_TEXT*>( item );
            text->SetPos0( text->GetPos0() + moveVector );
            text->SetDrawCoord();
            }
            break;

        default:
            break;
        }
    }

    CalculateBoundingBox();
}


void MODULE::SetOrientation( double aNewAngle )
{
    double angleChange = aNewAngle - m_Orient;  // change in rotation

    NORMALIZE_ANGLE_180( aNewAngle );

    m_Orient = aNewAngle;

    for( D_PAD* pad : m_pads )
    {
        pad->SetOrientation( pad->GetOrientation() + angleChange );
        pad->SetDrawCoord();
    }

    for( ZONE_CONTAINER* zone : m_fp_zones )
    {
        zone->Rotate( GetPosition(), angleChange );
    }

    // Update of the reference and value.
    m_Reference->SetDrawCoord();
    m_Value->SetDrawCoord();

    // Displace contours and text of the footprint.
    for( BOARD_ITEM* item : m_drawings )
    {
        if( item->Type() == PCB_FP_SHAPE_T )
        {
            static_cast<FP_SHAPE*>( item )->SetDrawCoord();
        }
        else if( item->Type() == PCB_FP_TEXT_T )
        {
            static_cast<FP_TEXT*>( item )->SetDrawCoord();
        }
    }
}


BOARD_ITEM* MODULE::Duplicate() const
{
    MODULE* dupe = (MODULE*) Clone();
    const_cast<KIID&>( dupe->m_Uuid ) = KIID();

    dupe->RunOnChildren( [&]( BOARD_ITEM* child )
                         {
                             const_cast<KIID&>( child->m_Uuid ) = KIID();
                         });

    return static_cast<BOARD_ITEM*>( dupe );
}


BOARD_ITEM* MODULE::DuplicateItem( const BOARD_ITEM* aItem, bool aAddToModule )
{
    BOARD_ITEM* new_item = NULL;
    MODULE_ZONE_CONTAINER* new_zone = NULL;

    switch( aItem->Type() )
    {
    case PCB_PAD_T:
    {
        D_PAD* new_pad = new D_PAD( *static_cast<const D_PAD*>( aItem ) );
        const_cast<KIID&>( new_pad->m_Uuid ) = KIID();

        if( aAddToModule )
            m_pads.push_back( new_pad );

        new_item = new_pad;
        break;
    }

    case PCB_FP_ZONE_AREA_T:
    {
        new_zone = new MODULE_ZONE_CONTAINER( *static_cast<const MODULE_ZONE_CONTAINER*>( aItem ) );
        const_cast<KIID&>( new_zone->m_Uuid ) = KIID();

        if( aAddToModule )
            m_fp_zones.push_back( new_zone );

        new_item = new_zone;
        break;
    }

    case PCB_FP_TEXT_T:
    {
        FP_TEXT* new_text = new FP_TEXT( *static_cast<const FP_TEXT*>( aItem ) );
        const_cast<KIID&>( new_text->m_Uuid ) = KIID();

        if( new_text->GetType() == FP_TEXT::TEXT_is_REFERENCE )
        {
            new_text->SetText( wxT( "${REFERENCE}" ) );
            new_text->SetType( FP_TEXT::TEXT_is_DIVERS );
        }
        else if( new_text->GetType() == FP_TEXT::TEXT_is_VALUE )
        {
            new_text->SetText( wxT( "${VALUE}" ) );
            new_text->SetType( FP_TEXT::TEXT_is_DIVERS );
        }

        if( aAddToModule )
            Add( new_text );

        new_item = new_text;

        break;
    }

    case PCB_FP_SHAPE_T:
    {
        FP_SHAPE* new_shape = new FP_SHAPE( *static_cast<const FP_SHAPE*>( aItem ) );
        const_cast<KIID&>( new_shape->m_Uuid ) = KIID();

        if( aAddToModule )
            Add( new_shape );

        new_item = new_shape;
        break;
    }

    case PCB_GROUP_T:
        new_item = static_cast<const PCB_GROUP*>( aItem )->DeepDuplicate();
        break;

    case PCB_MODULE_T:
        // Ignore the footprint itself
        break;

    default:
        // Un-handled item for duplication
        wxFAIL_MSG( "Duplication not supported for items of class " + aItem->GetClass() );
        break;
    }

    return new_item;
}


wxString MODULE::GetNextPadName( const wxString& aLastPadName ) const
{
    std::set<wxString> usedNames;

    // Create a set of used pad numbers
    for( D_PAD* pad : m_pads )
        usedNames.insert( pad->GetName() );

    wxString prefix = UTIL::GetReferencePrefix( aLastPadName );
    int      num = GetTrailingInt( aLastPadName );

    while( usedNames.count( wxString::Format( "%s%d", prefix, num ) ) )
        num++;

    return wxString::Format( "%s%d", prefix, num );
}


void MODULE::IncrementReference( int aDelta )
{
    const wxString& refdes = GetReference();

    SetReference( wxString::Format( wxT( "%s%i" ),
                  UTIL::GetReferencePrefix( refdes ),
                  GetTrailingInt( refdes ) + aDelta ) );
}


// Calculate the area of aPolySet, after fracturation, because
// polygons with no hole are expected.
static double polygonArea( SHAPE_POLY_SET& aPolySet )
{
    double area = 0.0;

    for( int ii = 0; ii < aPolySet.OutlineCount(); ii++ )
    {
        SHAPE_LINE_CHAIN& outline = aPolySet.Outline( ii );
        // Ensure the curr outline is closed, to calculate area
        outline.SetClosed( true );

        area += outline.Area();
     }

    return area;
}

// a helper function to add a rectangular polygon aRect to aPolySet
static void addRect( SHAPE_POLY_SET& aPolySet, wxRect aRect )
{
    aPolySet.NewOutline();

    aPolySet.Append( aRect.GetX(), aRect.GetY() );
    aPolySet.Append( aRect.GetX()+aRect.width, aRect.GetY() );
    aPolySet.Append( aRect.GetX()+aRect.width, aRect.GetY()+aRect.height );
    aPolySet.Append( aRect.GetX(), aRect.GetY()+aRect.height );
}

double MODULE::CoverageRatio( const GENERAL_COLLECTOR& aCollector ) const
{
    double fpArea = GetFootprintRect().GetArea();
    SHAPE_POLY_SET coveredRegion;
    addRect( coveredRegion, GetFootprintRect() );

    // build list of holes (covered areas not available for selection)
    SHAPE_POLY_SET holes;

    for( D_PAD* pad : m_pads )
        addRect( holes, pad->GetBoundingBox() );

    addRect( holes, m_Reference->GetBoundingBox() );
    addRect( holes, m_Value->GetBoundingBox() );

    for( int i = 0; i < aCollector.GetCount(); ++i )
    {
        BOARD_ITEM* item = aCollector[i];

        switch( item->Type() )
        {
        case PCB_TEXT_T:
        case PCB_FP_TEXT_T:
        case PCB_TRACE_T:
        case PCB_ARC_T:
        case PCB_VIA_T:
            addRect( holes, item->GetBoundingBox() );
            break;
        default:
            break;
        }
    }

    SHAPE_POLY_SET uncoveredRegion;

    try
    {
        uncoveredRegion.BooleanSubtract( coveredRegion, holes, SHAPE_POLY_SET::PM_STRICTLY_SIMPLE );
        uncoveredRegion.Simplify( SHAPE_POLY_SET::PM_STRICTLY_SIMPLE );
        uncoveredRegion.Fracture( SHAPE_POLY_SET::PM_STRICTLY_SIMPLE );
    }
    catch( ClipperLib::clipperException& )
    {
        // better to be conservative (this will result in the disambiguate dialog)
        return 1.0;
    }

    double uncoveredRegionArea = polygonArea( uncoveredRegion );
    double coveredArea = fpArea - uncoveredRegionArea;
    double ratio = ( coveredArea / fpArea );

    return std::min( ratio, 1.0 );
}


std::shared_ptr<SHAPE> MODULE::GetEffectiveShape( PCB_LAYER_ID aLayer ) const
{
    std::shared_ptr<SHAPE> shape ( new SHAPE_NULL );

    return shape;
}


bool MODULE::BuildPolyCourtyard()
{
    m_poly_courtyard_front.RemoveAllContours();
    m_poly_courtyard_back.RemoveAllContours();
    // Build the courtyard area from graphic items on the courtyard.
    // Only PCB_FP_SHAPE_T have meaning, graphic texts are ignored.
    // Collect items:
    std::vector<PCB_SHAPE*> list_front;
    std::vector<PCB_SHAPE*> list_back;

    for( BOARD_ITEM* item : GraphicalItems() )
    {
        if( item->GetLayer() == B_CrtYd && item->Type() == PCB_FP_SHAPE_T )
            list_back.push_back( static_cast<PCB_SHAPE*>( item ) );

        if( item->GetLayer() == F_CrtYd && item->Type() == PCB_FP_SHAPE_T )
            list_front.push_back( static_cast<PCB_SHAPE*>( item ) );
    }

    // Note: if no item found on courtyard layers, return true.
    // false is returned only when the shape defined on courtyard layers
    // is not convertible to a polygon
    if( !list_front.size() && !list_back.size() )
        return true;

    wxString error_msg;

    #define ARC_ERROR_MAX 0.02      /* error max in mm to approximate a arc by segments */
    bool success = ConvertOutlineToPolygon( list_front, m_poly_courtyard_front,
                                            (unsigned) Millimeter2iu( ARC_ERROR_MAX ), &error_msg );

    if( success )
    {
        success = ConvertOutlineToPolygon( list_back, m_poly_courtyard_back,
                                           (unsigned) Millimeter2iu( ARC_ERROR_MAX ), &error_msg );
    }

    if( !error_msg.IsEmpty() )
    {
        wxLogMessage( wxString::Format( _( "Processing courtyard of \"%s\": %s" ),
                                        GetFPID().Format().wx_str(),
                                        error_msg ) );
    }

    return success;
}


void MODULE::SwapData( BOARD_ITEM* aImage )
{
    assert( aImage->Type() == PCB_MODULE_T );

    std::swap( *((MODULE*) this), *((MODULE*) aImage) );
}


bool MODULE::HasThroughHolePads() const
{
    for( D_PAD* pad : Pads() )
    {
        if( pad->GetAttribute() != PAD_ATTRIB_SMD )
            return true;
    }

    return false;
}


bool MODULE::cmp_drawings::operator()( const BOARD_ITEM* aFirst, const BOARD_ITEM* aSecond ) const
{
    if( aFirst->Type() != aSecond->Type() )
        return aFirst->Type() < aSecond->Type();

    if( aFirst->GetLayer() != aSecond->GetLayer() )
        return aFirst->GetLayer() < aSecond->GetLayer();

    if( aFirst->Type() == PCB_FP_SHAPE_T )
    {
        const FP_SHAPE* dwgA = static_cast<const FP_SHAPE*>( aFirst );
        const FP_SHAPE* dwgB = static_cast<const FP_SHAPE*>( aSecond );

        if( dwgA->GetShape() != dwgB->GetShape() )
            return dwgA->GetShape() < dwgB->GetShape();
    }

    if( aFirst->m_Uuid != aSecond->m_Uuid ) // shopuld be always the case foer valid boards
        return aFirst->m_Uuid < aSecond->m_Uuid;

    return aFirst < aSecond;
}


bool MODULE::cmp_pads::operator()( const D_PAD* aFirst, const D_PAD* aSecond ) const
{
    if( aFirst->GetName() != aSecond->GetName() )
        return StrNumCmp( aFirst->GetName(), aSecond->GetName() ) < 0;

    if( aFirst->m_Uuid != aSecond->m_Uuid ) // shopuld be always the case foer valid boards
        return aFirst->m_Uuid < aSecond->m_Uuid;

    return aFirst < aSecond;
}


static struct MODULE_DESC
{
    MODULE_DESC()
    {
        ENUM_MAP<PCB_LAYER_ID>& layerEnum = ENUM_MAP<PCB_LAYER_ID>::Instance();

        if( layerEnum.Choices().GetCount() == 0 )
        {
            layerEnum.Undefined( UNDEFINED_LAYER );

            for( LSEQ seq = LSET::AllLayersMask().Seq(); seq; ++seq )
                layerEnum.Map( *seq, LSET::Name( *seq ) );
        }

        wxPGChoices fpLayers;       // footprints might be placed only on F.Cu & B.Cu
        fpLayers.Add( LSET::Name( F_Cu ), F_Cu );
        fpLayers.Add( LSET::Name( B_Cu ), B_Cu );

        PROPERTY_MANAGER& propMgr = PROPERTY_MANAGER::Instance();
        REGISTER_TYPE( MODULE );
        propMgr.AddTypeCast( new TYPE_CAST<MODULE, BOARD_ITEM> );
        propMgr.AddTypeCast( new TYPE_CAST<MODULE, BOARD_ITEM_CONTAINER> );
        propMgr.InheritsAfter( TYPE_HASH( MODULE ), TYPE_HASH( BOARD_ITEM ) );
        propMgr.InheritsAfter( TYPE_HASH( MODULE ), TYPE_HASH( BOARD_ITEM_CONTAINER ) );

        auto layer = new PROPERTY_ENUM<MODULE, PCB_LAYER_ID, BOARD_ITEM>( _HKI( "Layer" ),
                    &MODULE::SetLayer, &MODULE::GetLayer );
        layer->SetChoices( fpLayers );
        propMgr.ReplaceProperty( TYPE_HASH( BOARD_ITEM ), _HKI( "Layer" ), layer );

        propMgr.AddProperty( new PROPERTY<MODULE, wxString>( _HKI( "Reference" ),
                    &MODULE::SetReference, &MODULE::GetReference ) );
        propMgr.AddProperty( new PROPERTY<MODULE, wxString>( _HKI( "Value" ),
                    &MODULE::SetValue, &MODULE::GetValue ) );
        propMgr.AddProperty( new PROPERTY<MODULE, double>( _HKI( "Orientation" ),
                    &MODULE::SetOrientationDegrees, &MODULE::GetOrientationDegrees,
                    PROPERTY_DISPLAY::DEGREE ) );
        propMgr.AddProperty( new PROPERTY<MODULE, int>( _HKI( "Local Clearance" ),
                    &MODULE::SetLocalClearance, &MODULE::GetLocalClearance,
                    PROPERTY_DISPLAY::DISTANCE ) );
        propMgr.AddProperty( new PROPERTY<MODULE, int>( _HKI( "Local Solderpaste Margin" ),
                    &MODULE::SetLocalSolderPasteMargin, &MODULE::GetLocalSolderPasteMargin,
                    PROPERTY_DISPLAY::DISTANCE ) );
        propMgr.AddProperty( new PROPERTY<MODULE, double>( _HKI( "Local Solderpaste Margin Ratio" ),
                    &MODULE::SetLocalSolderPasteMarginRatio, &MODULE::GetLocalSolderPasteMarginRatio ) );
        propMgr.AddProperty( new PROPERTY<MODULE, int>( _HKI( "Thermal Width" ),
                    &MODULE::SetThermalWidth, &MODULE::GetThermalWidth,
                    PROPERTY_DISPLAY::DISTANCE ) );
        propMgr.AddProperty( new PROPERTY<MODULE, int>( _HKI( "Thermal Gap" ),
                    &MODULE::SetThermalGap, &MODULE::GetThermalGap,
                    PROPERTY_DISPLAY::DISTANCE ) );
        // TODO zone connection, FPID?
    }
} _MODULE_DESC;
