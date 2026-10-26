/ MADFilterInterface.h
//

/* 
 * MADFilter - DirectShow filter for the MAD library
 * http://sourceforge.net/projects/maddxshow
 * 
 * Created by Armin Gerritsen - Copyright (c) April 2002
 * Contact: Armin.Gerritsen@@hetnet.nl
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
 * http://www.gnu.org/copyleft/gpl.html
 *
 */


#ifndef _MADFILTERINTERFACE_
#define _MADFILTERINTERFACE_

#include <streams.h>


#ifdef __cplusplus
extern "C" {
#endif

// {8E4F6871-5791-11d6-8C1F-00104B49AA66}
DEFINE_GUID( IID_IMADFilterInterface, 
0x8e4f6871, 0x5791, 0x11d6, 0x8c, 0x1f, 0x00, 0x10, 0x4b, 0x49, 0xaa, 0x66) ;
	
DECLARE_INTERFACE_( IMADFilterInterface, IUnknown )
{
 // Does nothing difficult, but this just shows how to create members.
 STDMETHOD (GetMADVersion) (THIS_ int *pVersion ) PURE ;
};

#ifdef __cplusplus
}
#endif

#endif //_MADFILTERINTERFACE_