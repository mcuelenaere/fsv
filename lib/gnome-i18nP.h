/*
 * Handles i18n for the Gnome libraries. Libraries need to use
 * dgettext in order to use a non-default translation domain.
 * Author: Tom Tromey <tromey@creche.cygnus.com>
 */

#ifndef __GNOME_I18NP_H__
#define __GNOME_I18NP_H__

#ifdef  __GNOME_I18N_H__
/* #warning "You should use either gnome-i18n.h OR gnome-i18nP.h" */
#endif

/* BEGIN_GNOME_DECLS */

#ifdef ENABLE_NLS
#    include <libintl.h>
#    undef _
#    define _(String) dgettext (PACKAGE, String)
#    ifdef gettext_noop
#        define N_(String) gettext_noop (String)
#    else
#        define N_(String) (String)
#    endif
#else
/* Stubs that do something close enough.  */
#    define textdomain(String) (String)
#    define gettext(String) (String)
#    define dgettext(Domain,Message) (Message)
#    define dcgettext(Domain,Message,Type) (Message)
#    define bindtextdomain(Domain,Directory) (Domain)
#    define _(String) (String)
#    define N_(String) (String)
#endif

const char *gnome_i18n_get_language(void);
GList      *gnome_i18n_get_language_list (const gchar *category_name);
void gnome_i18n_init (void);
void	   gnome_i18n_set_preferred_language (const char *val);
const char *gnome_i18n_get_preferred_language (void);

/* END_GNOME_DECLS */

#endif /* __GNOME_UTIL_H__ */
