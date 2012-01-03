#ifndef _I18N_H
#define _I18N_H

#include "gettext.h"
#include <locale.h>

#define  _(Text) gettext(Text)
#define N_(Text) gettext_noop(Text)
#define X_(Text) Text

#endif /* _I18N_H */
