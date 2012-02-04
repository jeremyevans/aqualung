#ifndef AQUALUNG_I18N_H
#define AQUALUNG_I18N_H

#include "gettext.h"
#include <locale.h>

#define  _(Text) gettext(Text)
#define N_(Text) gettext_noop(Text)
#define X_(Text) Text


#endif /* AQUALUNG_I18N_H */
