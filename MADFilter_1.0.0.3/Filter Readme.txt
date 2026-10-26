Hello,

This is the MAD Transform Filter. It is a DirectShow MPEG Audio
decoder filter, which can be used to decode MPEG audio to PCM
WAVE format. I can be used for instance as a replacement of the
Fraunhover filter, which is installed on most Windows systems.

Installation is easy:

1) Unzip MADFilter.ax together with register.bat to any folder (for
   example Windows\System).
2) Run register.bat or, if you put the filter into Windows\System, 
   type in a console, or via Start -> Run: "regsvr32 MADFilter.ax"

Deinstallation is also easy:

1) Unzip unregister.bat to the same folder as MADFilter.ax and run it.
   Or, if you put the filter into Windows\System, you can also type 
   in a console, or via Start -> Run: "regsvr32 -u MADFilter.ax"
2) Delete MADFilter.ax

There is no real support on this, but I'm always willing to try
to help if I have the time and I'm also open for suggestions,
comments etc.

Regards,

Armin Gerritsen,


http://sourceforge.net/projects/maddxshow/