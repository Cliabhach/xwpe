# XWPE-GIT README #

* Master branch [![Build Status](https://travis-ci.org/amagnasco/xwpe.svg?branch=master)](https://travis-ci.org/amagnasco/xwpe)
* Development [![Build Status](https://travis-ci.org/amagnasco/xwpe.svg?branch=devel)](https://travis-ci.org/amagnasco/xwpe)
* Experimental [![Build Status](https://travis-ci.org/amagnasco/xwpe.svg?branch=experimental)](https://travis-ci.org/amagnasco/xwpe)

## What is xwpe? ##

Xwpe is a development environment designed for use on UNIX
systems.  Fred Kruse wrote xwpe and released the software for free
under the GNU Public License.  The user interface was designed to
mimic the Borland C and Pascal family.  Extensive support is provided
for programming.

Syntax highlighting for many programming languages are included
and others may be added if necessary.  Any compiler can easily be
used by the program.  By compiling within xwpe, errors in the source
code can be jumped to and swiftly corrected.  Support for three
different debuggers are provided within the development environment.
Variables and the stack can be easily displayed.  Setting and
unsetting breakpoints can done directly within the source code.

The program can be run in several forms.  Xwpe runs the X
windows version of the programming environment.  Wpe simply runs a
terminal programming environment.  Xwe and we provide a simple text
editor for X windows and terminal modes respectively.

Online help describes the complete use of xwpe.

## What is xwpe-alpha? ##

The xwpe-alpha project was an attempt to reorganize the source code to
improve readability.  It also incorporated as many bug fixes as
available.  Attempts to contact the author of xwpe have received no
response so xwpe-alpha should be considered unsupported by Fred Kruse.
To signify the difference xwpe-alpha has increased the version number
from 1.4.x to 1.5.x despite relatively few changes at present.  Also
xwpe-alpha release end in 'a' (e.g. 1.5.4a).

## What is xwpe-git? ##

Since more than a decade has passed since the xwpe-alpha project closed,
the code has been ported to GitHub and made available to the community.
In keeping with the spirit of xwpe-alpha, xwpe-git has increased the 
version number from 1.5.x to 1.6.x despite relatively few changes as well.

## Why release it now? ##

The source code is still largely unchanged at this point.  The purpose
in the release is to discover problems early in the modifications.  Since
not all platforms are accessible to the developers come changes could
inadvertently break compilation on another system.  Also understanding and
modifying the structure of xwpe is a large undertaking and will take a long
time to complete.

## Copyright ##

Copyright (C) 1993 Fred Kruse. xwpe is free; anyone may
redistribute copies of xwpe to anyone under the terms stated in the
GNU General Public License.  The author assumes no responsibility
for errors or omissions or damages resulting from the use of xwpe or
this manual.

## Maintainer ##

This version is an unofficial update to xwpe and is not supported 
by Fred Kruse. Updates have previously been made available on the unofficial 
xwpe-alpha homepage, http://www.identicalsoftware.com/xwpe/. The previous
maintainer was Dennis Payne, dulsi@identicalsoftware.com.


As evidenced by their discussion list, xwpe-alpha has not seen active 
development since at least 2003. This GitHub version is maintained by 
Alessandro G. Magnasco at https://github.com/amagnasco/xwpe/. 

Guus Bonnema is contributor, bravely trying to refactor and clean up 
this mess: https://github.com/gbonnema.
    
Caveat emptor ; this software hails from last century. I can try to help
figure out any questions or problems you might have, but purely out of 
academic interest. Solutions are left as an exercise for the hacker.
If you have a patch, please let me know so we can review it.
    
## DEPENDENCIES ##

If you are having difficulties compiling your code, be aware that xwpe
requires a backport version of libcurses. As most modern systems should keep 
far, far away from the original UNIX library, at time of writing (02017-05)
this dependency is satisfied by libncurses5-dev or libncurses6.
    
## ENJOY ! ##
