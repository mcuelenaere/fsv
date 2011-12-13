#ifndef __GNOME_DATE_EDIT_H_
#define __GNOME_DATE_EDIT_H_ 

#include <gtk/gtkhbox.h>
/* #include <libgnome/gnome-defs.h> */
 
/* BEGIN_GNOME_DECLS */


typedef enum {
	GNOME_DATE_EDIT_SHOW_TIME             = 1 << 0,
	GNOME_DATE_EDIT_24_HR                 = 1 << 1,
	GNOME_DATE_EDIT_WEEK_STARTS_ON_MONDAY = 1 << 2
} GnomeDateEditFlags;


#define GNOME_DATE_EDIT(obj)          GTK_CHECK_CAST (obj, gnome_date_edit_get_type(), GnomeDateEdit)
#define GNOME_DATE_EDIT_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gnome_date_edit_get_type(), GnomeDateEditClass)
#define GNOME_IS_DATE_EDIT(obj)       GTK_CHECK_TYPE (obj, gnome_date_edit_get_type ())

typedef struct {
	GtkHBox hbox;

	GtkWidget *date_entry;
	GtkWidget *date_button;
	
	GtkWidget *time_entry;
	GtkWidget *time_popup;

	GtkWidget *cal_label;
	GtkWidget *cal_popup;
	GtkWidget *calendar;

	int       lower_hour;
	int       upper_hour;
	
	time_t    initial_time;
	int       flags;
} GnomeDateEdit;

typedef struct {
	GtkHBoxClass parent_class;
	void (*date_changed) (GnomeDateEdit *gde);
	void (*time_changed) (GnomeDateEdit *gde);
} GnomeDateEditClass;

guint     gnome_date_edit_get_type        (void);
GtkWidget *gnome_date_edit_new            (time_t the_time, int show_time, int use_24_format);
GtkWidget *gnome_date_edit_new_flags      (time_t the_time, GnomeDateEditFlags flags);

void      gnome_date_edit_set_time        (GnomeDateEdit *gde, time_t the_time);
void      gnome_date_edit_set_popup_range (GnomeDateEdit *gde, int low_hour, int up_hour);
time_t    gnome_date_edit_get_date        (GnomeDateEdit *gde);
void      gnome_date_edit_set_flags       (GnomeDateEdit *gde, GnomeDateEditFlags flags);
int       gnome_date_edit_get_flags       (GnomeDateEdit *gde);


/* END_GNOME_DECLS */

#endif
