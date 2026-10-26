/ version.h
//
// used to maintain the version number
//

/* 
 * MADFilter - DirectShow filter for the MAD library
 * http://sourceforge.net/projects/maddxshow
 * 
 * created by bond - September 2003
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


#define MADFILTER_VERSION_MAJOR		1
#define MADFILTER_VERSION_MINOR		0
#define MADFILTER_VERSION_PATCH		0
#define MADFILTER_VERSION_BUILD		3



#define MADFILTER_VERSION_STRINGIZE(str)	#str
#define MADFILTER_VERSION_STRING(num)		MADFILTER_VERSION_STRINGIZE(num)

#define MADFILTER_VERSION		MADFILTER_VERSION_STRING(MADFILTER_VERSION_MAJOR) "."  \
					MADFILTER_VERSION_STRING(MADFILTER_VERSION_MINOR) "."  \
					MADFILTER_VERSION_STRING(MADFILTER_VERSION_PATCH) "."  \
					MADFILTER_VERSION_STRING(MADFILTER_VERSION_BUILD)

// used in MADFilter.rc

#define MADFILTER_FILEVERSION		MADFILTER_VERSION_MAJOR,MADFILTER_VERSION_MINOR,MADFILTER_VERSION_PATCH,MADFILTER_VERSION_BUILD


#define MADFILTER_FILEVERSION_STRINGIZE(str)	#str
#define MADFILTER_FILEVERSION_STRING(num)	MADFILTER_FILEVERSION_STRINGIZE(num)

#define MADFILTER_FILEVERSION_STR	MADFILTER_FILEVERSION_STRING(MADFILTER_VERSION_MAJOR) ", "  \
					MADFILTER_FILEVERSION_STRING(MADFILTER_VERSION_MINOR) ", "  \
					MADFILTER_FILEVERSION_STRING(MADFILTER_VERSION_PATCH) ", "  \
					MADFILTER_FILEVERSION_STRING(MADFILTER_VERSION_BUILD) "\0"