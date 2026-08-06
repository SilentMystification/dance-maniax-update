// releaseTag.h - the exact release tag this exe was built as.
// Stamped in by release.yml's "build" job right before compiling (see the "Stamp release tag"
// step), so a CI-built exe always knows exactly what it is without needing any local marker file.
// Left blank for local/dev builds, which are not an official release of any channel.

#ifndef _RELEASETAG_H
#define _RELEASETAG_H

#define DMX_RELEASE_TAG ""

#endif
