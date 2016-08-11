/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2016 CERN
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

#include <kiway.h>
#include <schframe.h>
#include "sim_plot_frame.h"

void SCH_EDIT_FRAME::OnSimulate( wxCommandEvent& event )
{
    SIM_PLOT_FRAME* simFrame = (SIM_PLOT_FRAME*) Kiway().Player( FRAME_SIMULATOR, true );
    simFrame->Show( true );

    // On Windows, Raise() does not bring the window on screen, when iconized
    if( simFrame->IsIconized() )
        simFrame->Iconize( false );

    simFrame->Raise();
}

// I apologize for the following lines, but this is more or less what wxWidgets
// authors suggest (http://docs.wxwidgets.org/trunk/classwx_cursor.html)

static const unsigned char cursor_probe[] = {
   0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x70,
   0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x04,
   0x00, 0x00, 0x80, 0x07, 0x00, 0x00, 0x40, 0x04, 0x00, 0x00, 0x20, 0x04,
   0x00, 0x00, 0x10, 0x02, 0x00, 0x00, 0x08, 0x01, 0x00, 0x00, 0x84, 0x00,
   0x00, 0x00, 0x42, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x80, 0x10, 0x00,
   0x00, 0x40, 0x08, 0x00, 0x00, 0x20, 0x04, 0x00, 0x00, 0x10, 0x02, 0x00,
   0x00, 0x08, 0x01, 0x00, 0x80, 0x85, 0x00, 0x00, 0x40, 0x42, 0x00, 0x00,
   0x20, 0x21, 0x00, 0x00, 0x20, 0x11, 0x00, 0x00, 0x20, 0x09, 0x00, 0x00,
   0x20, 0x16, 0x00, 0x00, 0x50, 0x10, 0x00, 0x00, 0x88, 0x08, 0x00, 0x00,
   0x44, 0x07, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00,
   0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00 };

static const unsigned char cursor_probe_mask[] {
   0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x70,
   0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x04,
   0x00, 0x00, 0x80, 0x07, 0x00, 0x00, 0xc0, 0x07, 0x00, 0x00, 0xe0, 0x07,
   0x00, 0x00, 0xf0, 0x03, 0x00, 0x00, 0xf8, 0x01, 0x00, 0x00, 0xfc, 0x00,
   0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x80, 0x1f, 0x00,
   0x00, 0xc0, 0x0f, 0x00, 0x00, 0xe0, 0x07, 0x00, 0x00, 0xf0, 0x03, 0x00,
   0x00, 0xf8, 0x01, 0x00, 0x80, 0xfd, 0x00, 0x00, 0xc0, 0x7f, 0x00, 0x00,
   0xe0, 0x3f, 0x00, 0x00, 0xe0, 0x1f, 0x00, 0x00, 0xe0, 0x0f, 0x00, 0x00,
   0xe0, 0x1f, 0x00, 0x00, 0xf0, 0x1f, 0x00, 0x00, 0xf8, 0x0f, 0x00, 0x00,
   0x7c, 0x07, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00,
   0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00 };

#ifdef __WXMSW__
struct CURSOR_PROBE_INIT
{
public:
    static wxImage& GetProbeImage()
    {
        static wxImage* probe_image = NULL;

        if( probe_image == NULL )
        {
            wxBitmap probe_bitmap( (const char*) cursor_probe, 32, 32 );
            wxBitmap probe_mask_bitmap( (const char*) cursor_probe_mask, 32, 32 );
            probe_bitmap.SetMask( new wxMask( probe_mask_bitmap ) );
            probe_image = new wxImage( probe_bitmap.ConvertToImage() );
            probe_image->SetOption( wxIMAGE_OPTION_CUR_HOTSPOT_X, 0 );
            probe_image->SetOption( wxIMAGE_OPTION_CUR_HOTSPOT_Y, 31 );
        }

        return *probe_image;
    }
};

const wxCursor SCH_EDIT_FRAME::CURSOR_PROBE( CURSOR_PROBE_INIT::GetProbeImage() );
#elif defined(__WXGTK__) or defined(__WXMOTIF__)
const wxCursor SCH_EDIT_FRAME::CURSOR_PROBE( (const char*) cursor_probe, 32, 32, 0, 31, (const char*) cursor_probe_mask );
#endif
